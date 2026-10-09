# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.lock_lease - Cross-Process File Lock Lease Protocol (I-10)
========================================================================
Implements self-healing file-based locking with lease timeout, process liveness
probing, and jittered exponential backoff. Prevents deadlocks from crashed or
aborted supervisor processes.
"""
from __future__ import annotations

import datetime
import json
import os
import random
import socket
import sys
import time
from pathlib import Path
from typing import Any, Dict, Optional


def is_process_alive(pid: int) -> bool:
    """Checks whether a process with the given PID is currently active."""
    if pid <= 0:
        return False
    if sys.platform == "win32":
        try:
            import ctypes
            # PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
            kernel32 = ctypes.windll.kernel32
            handle = kernel32.OpenProcess(0x1000, False, pid)
            if handle:
                # Process exists; check exit code
                exit_code = ctypes.c_ulong()
                kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code))
                kernel32.CloseHandle(handle)
                # STILL_ACTIVE = 259
                return exit_code.value == 259
            else:
                err = kernel32.GetLastError()
                # ERROR_INVALID_PARAMETER (87) means PID not found
                if err == 87:
                    return False
                # Access denied (5) means it exists but owned by higher privilege
                return err == 5
        except Exception:
            return True  # Be conservative on unexpected error
    else:
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return True
        except Exception:
            return True


class LockLeaseError(Exception):
    """Raised when lock acquisition fails or times out."""
    pass


import uuid


class FileLockLease:
    """File lock with time-bounded lease and dead-process self-healing."""

    def __init__(
        self,
        lock_path: Path,
        lease_ttl_seconds: float = 30.0,
        hostname: Optional[str] = None,
    ):
        self.lock_path = lock_path.resolve()
        self.lease_ttl_seconds = lease_ttl_seconds
        self.hostname = hostname or socket.gethostname()
        self._acquired = False
        self._token: Optional[str] = None

    @property
    def token(self) -> Optional[str]:
        return self._token

    def acquire(self, timeout_seconds: float = 10.0, poll_interval_ms: int = 100) -> None:
        """Acquire lock within timeout, healing stale or dead-process locks."""
        start_time = time.time()
        pid = os.getpid()

        while True:
            now = time.time()
            if self._try_acquire(pid):
                self._acquired = True
                return

            # Check if existing lock is held by a dead process or expired cross-host lease
            if self._heal_stale_lock():
                # Attempt immediate acquisition after healing
                if self._try_acquire(pid):
                    self._acquired = True
                    return

            if (now - start_time) >= timeout_seconds:
                raise LockLeaseError(
                    f"Timed out after {timeout_seconds:.1f}s waiting for lock: {self.lock_path}"
                )

            # Jittered sleep: backoff with small random variation
            jitter = random.uniform(0.8, 1.2)
            time.sleep((poll_interval_ms / 1000.0) * jitter)

    def _try_acquire(self, pid: int) -> bool:
        """Atomically create lock file via exclusive creation with a unique fencing token."""
        token = f"{pid}-{int(time.time() * 1000)}-{uuid.uuid4().hex[:8]}"
        lock_data = {
            "token": token,
            "pid": pid,
            "hostname": self.hostname,
            "created_at": time.time(),
            "expires_at": time.time() + self.lease_ttl_seconds,
            "created_iso": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        }
        try:
            self.lock_path.parent.mkdir(parents=True, exist_ok=True)
            # Use os.O_CREAT | os.O_EXCL | os.O_WRONLY for atomic creation
            fd = os.open(str(self.lock_path), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                json.dump(lock_data, f, indent=2)
            self._token = token
            return True
        except FileExistsError:
            return False
        except OSError:
            return False

    def _heal_stale_lock(self) -> bool:
        """
        Check if lock is held by a dead process or expired cross-host lease.
        Constraint 3.5: Forbids evicting active living processes merely due to TTL on same host.
        """
        try:
            if not self.lock_path.is_file():
                return False

            raw = self.lock_path.read_text(encoding="utf-8")
            data = json.loads(raw)
            holder_pid = data.get("pid", 0)
            holder_host = data.get("hostname", "")
            expires_at = data.get("expires_at", 0)

            now = time.time()
            is_expired = now > expires_at

            # If on the same host, check process liveness
            if holder_host == self.hostname and holder_pid > 0:
                is_alive = is_process_alive(holder_pid)
                if is_alive:
                    # Active process is still running: never evict merely due to TTL
                    return False
                # Process is dead / crashed: safe to break
                try:
                    self.lock_path.unlink(missing_ok=True)
                    return True
                except OSError:
                    return False
            else:
                # Cross-host: cannot probe PID, rely on TTL expiration
                if is_expired:
                    try:
                        self.lock_path.unlink(missing_ok=True)
                        return True
                    except OSError:
                        return False
        except Exception:
            pass
        return False

    def is_owner(self) -> bool:
        """Check if this instance currently owns the lock on disk."""
        if not self._acquired or not self._token:
            return False
        try:
            if not self.lock_path.is_file():
                return False
            data = json.loads(self.lock_path.read_text(encoding="utf-8"))
            return data.get("token") == self._token
        except Exception:
            return False

    def renew(self, extension_seconds: Optional[float] = None) -> bool:
        """Heartbeat renewal: extend lease expiration if we are the current owner."""
        if not self.is_owner():
            return False
        ext = extension_seconds or self.lease_ttl_seconds
        try:
            raw = self.lock_path.read_text(encoding="utf-8")
            data = json.loads(raw)
            if data.get("token") != self._token:
                return False
            data["expires_at"] = time.time() + ext
            data["renewed_at"] = time.time()
            # Write via temporary file replacement for atomicity
            tmp = self.lock_path.with_suffix(f".tmp.{os.getpid()}")
            tmp.write_text(json.dumps(data, indent=2), encoding="utf-8")
            os.replace(tmp, self.lock_path)
            return True
        except Exception:
            return False

    def release(self) -> None:
        """Release the lock if acquired by this instance with matching token."""
        if not self._acquired:
            return
        try:
            if self.lock_path.is_file():
                # Verify token ownership before removing to prevent deleting new holder's lock
                try:
                    data = json.loads(self.lock_path.read_text(encoding="utf-8"))
                    if data.get("token") == self._token:
                        self.lock_path.unlink(missing_ok=True)
                except Exception:
                    pass
        finally:
            self._acquired = False
            self._token = None

    def __enter__(self) -> FileLockLease:
        self.acquire()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.release()

