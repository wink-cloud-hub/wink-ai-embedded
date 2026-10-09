# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.process_supervisor - Attempt-level Process Supervision & Bounded Reclamation
========================================================================================
Implements attempt-level process tracking, bounded timeout termination, inheritance
closure, and Windows file-lock exponential backoff (100ms, 200ms, 400ms).
"""
from __future__ import annotations

import os
import sys
import time
import subprocess
from pathlib import Path
from typing import Any, Callable, List, Optional, Tuple, TypeVar

T = TypeVar("T")


def safe_file_retry(
    operation: Callable[[], T],
    backoffs_ms: Tuple[int, ...] = (100, 200, 400),
    op_name: str = "file operation"
) -> T:
    """Execute file operation with exponential backoff on Windows sharing violations."""
    last_err: Optional[Exception] = None
    for attempt_idx, delay_ms in enumerate(backoffs_ms):
        try:
            return operation()
        except (PermissionError, OSError) as exc:
            last_err = exc
            # Check for Windows sharing violation error code 32 if applicable
            winerror = getattr(exc, "winerror", None)
            if sys.platform == "win32" and winerror not in (32, 5, None):
                # Non-transient errors fail immediately
                raise
            time.sleep(delay_ms / 1000.0)
    
    # Try one final attempt after all delays
    try:
        return operation()
    except Exception as exc:
        raise OSError(
            f"[FILE_LOCK_TIMEOUT] Bounded backoff exhausted ({sum(backoffs_ms)}ms) for {op_name}: {exc}"
        ) from exc


class ProcessSupervisor:
    """Manages attempt-scoped child process lifecycle and bounded teardown."""

    def __init__(self):
        self._tracked: List[subprocess.Popen] = []

    def register_process(self, proc: subprocess.Popen) -> None:
        self._tracked.append(proc)

    def terminate_all(self) -> int:
        count = 0
        for p in self._tracked:
            if p.poll() is None:
                self.terminate_process_tree(p.pid)
                count += 1
        return count

    def run_bounded_command(
        self,
        cmd: List[str],
        cwd: Optional[Path] = None,
        timeout_seconds: float = 120.0,
        env: Optional[dict] = None
    ) -> Tuple[int, str, str]:
        p = subprocess.Popen(
            cmd,
            cwd=str(cwd) if cwd else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env or os.environ.copy()
        )
        self.register_process(p)
        try:
            stdout, stderr = p.communicate(timeout=timeout_seconds)
            return p.returncode, stdout, stderr
        except subprocess.TimeoutExpired:
            self.terminate_process_tree(p.pid)
            try:
                stdout, stderr = p.communicate(timeout=2.0)
            except Exception:
                stdout, stderr = "", ""
            return 124, stdout, stderr

    @staticmethod
    def terminate_process_tree(pid: int, graceful_timeout_s: float = 1.0) -> None:
        """Terminate process tree with bounded graceful wait and forced cleanup."""
        if sys.platform == "win32":
            try:
                # taskkill /T /F ensures tree-wide bounded cleanup on Windows
                subprocess.run(
                    ["taskkill", "/F", "/T", "/PID", str(pid)],
                    capture_output=True,
                    timeout=3.0,
                    check=False
                )
            except Exception:
                pass
        else:
            try:
                os.kill(pid, 15)  # SIGTERM
                time.sleep(min(0.2, graceful_timeout_s))
                os.kill(pid, 9)   # SIGKILL
            except ProcessLookupError:
                pass
            except Exception:
                pass

    @classmethod
    def run_bounded_process(
        cls,
        cmd: List[str],
        cwd: Path,
        timeout_sec: int = 120,
        env: Optional[dict] = None
    ) -> Tuple[int, str]:
        """Run process with strict attempt-level boundary, tree cleanup, and captured output."""
        start_time = time.time()
        proc: Optional[subprocess.Popen] = None
        try:
            proc = subprocess.Popen(
                cmd,
                cwd=str(cwd),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env or os.environ.copy()
            )
            stdout, stderr = proc.communicate(timeout=timeout_sec)
            combined = f"{stdout}\n--- [stderr] ---\n{stderr}" if (stdout and stderr) else (stdout or stderr)
            return proc.returncode, combined.strip()
        except subprocess.TimeoutExpired:
            if proc is not None:
                cls.terminate_process_tree(proc.pid, graceful_timeout_s=1.0)
                try:
                    stdout, stderr = proc.communicate(timeout=2.0)
                    partial = (stdout or "") + (stderr or "")
                except Exception:
                    partial = ""
                diagnostic = f"[TIMEOUT] Process exceeded {timeout_sec}s wallclock limit. [DIAGNOSTIC_TRUNCATED: {partial[:500]}]"
                return 124, diagnostic
            return 124, f"[TIMEOUT] Process exceeded {timeout_sec}s"
        except Exception as exc:
            if proc is not None:
                cls.terminate_process_tree(proc.pid)
            return 1, f"[EXECUTION_ERROR] Failed running command: {exc}"
