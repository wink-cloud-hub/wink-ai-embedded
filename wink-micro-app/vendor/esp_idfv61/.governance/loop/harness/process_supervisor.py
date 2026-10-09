# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.process_supervisor - Attempt-level Process Supervision & Win32 Job Bounded Reclamation
===================================================================================================
Implements attempt-level process tracking, Win32 Job Object kernel isolation,
CREATE_SUSPENDED binding, inheritance closure, pipe handle release, and
Windows file-lock exponential backoff (100ms, 200ms, 400ms).

Conforms to ADR-0012, AFG-Engine 2.5, and Loop Reliability Contract 2.2.
"""
from __future__ import annotations

import os
import signal
import sys
import time
import subprocess
from pathlib import Path
from typing import Any, Callable, List, Optional, Tuple, TypeVar

T = TypeVar("T")

if sys.platform == "win32":
    import ctypes
    from ctypes import wintypes

    class IO_COUNTERS(ctypes.Structure):
        _fields_ = [
            ("ReadOperationCount", ctypes.c_uint64),
            ("WriteOperationCount", ctypes.c_uint64),
            ("OtherOperationCount", ctypes.c_uint64),
            ("ReadTransferCount", ctypes.c_uint64),
            ("WriteTransferCount", ctypes.c_uint64),
            ("OtherTransferCount", ctypes.c_uint64),
        ]

    class JOBOBJECT_BASIC_LIMIT_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("PerProcessUserTimeLimit", ctypes.c_int64),
            ("PerJobUserTimeLimit", ctypes.c_int64),
            ("LimitFlags", wintypes.DWORD),
            ("MinimumWorkingSetSize", ctypes.c_size_t),
            ("MaximumWorkingSetSize", ctypes.c_size_t),
            ("ActiveProcessLimit", wintypes.DWORD),
            ("Affinity", ctypes.c_size_t),
            ("PriorityClass", wintypes.DWORD),
            ("SchedulingClass", wintypes.DWORD),
        ]

    class JOBOBJECT_EXTENDED_LIMIT_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("BasicLimitInformation", JOBOBJECT_BASIC_LIMIT_INFORMATION),
            ("IoInfo", IO_COUNTERS),
            ("ProcessMemoryLimit", ctypes.c_size_t),
            ("JobMemoryLimit", ctypes.c_size_t),
            ("PeakProcessMemoryUsed", ctypes.c_size_t),
            ("PeakJobMemoryUsed", ctypes.c_size_t),
        ]

    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x2000
    JobObjectExtendedLimitInformation = 9
    CREATE_SUSPENDED = 0x00000004
else:
    CREATE_SUSPENDED = 0
    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0


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
            # Check for Windows sharing violation error code 32 or access denied 5
            winerror = getattr(exc, "winerror", None)
            if sys.platform == "win32" and winerror not in (32, 5, None):
                raise
            time.sleep(delay_ms / 1000.0)

    # Try one final attempt after all delays
    try:
        return operation()
    except Exception as exc:
        raise OSError(
            f"[FILE_LOCK_TIMEOUT] Bounded backoff exhausted ({sum(backoffs_ms)}ms) for {op_name}: {exc}"
        ) from exc


def close_process_pipes(proc: subprocess.Popen) -> None:
    """Explicitly close all subprocess pipe handles to prevent Windows sharing violations."""
    for pipe_name in ("stdout", "stderr", "stdin"):
        pipe = getattr(proc, pipe_name, None)
        if pipe is not None:
            try:
                if not pipe.closed:
                    pipe.close()
            except Exception:
                pass


class Win32JobObject:
    """Encapsulates Windows Job Object lifecycle for robust multi-process tree isolation."""

    def __init__(self, name: Optional[str] = None):
        self.handle: Optional[int] = None
        if sys.platform == "win32":
            self._init_job(name)

    def _init_job(self, name: Optional[str] = None) -> None:
        kernel32 = ctypes.windll.kernel32
        self.handle = kernel32.CreateJobObjectW(None, name)
        if not self.handle:
            raise OSError(f"[JOB_CREATE_FAILED] Failed to create Job Object: {ctypes.GetLastError()}")

        info = JOBOBJECT_EXTENDED_LIMIT_INFORMATION()
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        ok = kernel32.SetInformationJobObject(
            self.handle,
            JobObjectExtendedLimitInformation,
            ctypes.byref(info),
            ctypes.sizeof(info),
        )
        if not ok:
            err = ctypes.GetLastError()
            self.close()
            raise OSError(f"[JOB_CONFIG_FAILED] Failed to set Job Object information: {err}")

    def assign_process(self, process_handle: int) -> bool:
        if not self.handle or sys.platform != "win32":
            return False
        return bool(ctypes.windll.kernel32.AssignProcessToJobObject(self.handle, process_handle))

    def terminate(self, exit_code: int = 1) -> bool:
        if not self.handle or sys.platform != "win32":
            return False
        return bool(ctypes.windll.kernel32.TerminateJobObject(self.handle, exit_code))

    def close(self) -> None:
        if self.handle and sys.platform == "win32":
            ctypes.windll.kernel32.CloseHandle(self.handle)
            self.handle = None

    def __enter__(self) -> "Win32JobObject":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()


class ProcessSupervisor:
    """Manages attempt-scoped child process lifecycle and bounded teardown."""

    def __init__(self):
        self._tracked: List[subprocess.Popen] = []

    def register_process(self, proc: subprocess.Popen) -> None:
        self._tracked.append(proc)

    def spawn_supervised_process(
        self,
        cmd: List[str],
        cwd: Optional[Path] = None,
        env: Optional[dict] = None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        stdin=None,
    ) -> subprocess.Popen:
        """Spawn a process strictly bound to a Win32 Job Object before execution begins."""
        job: Optional[Win32JobObject] = None
        creationflags = 0
        extra_kwargs: dict[str, Any] = {}

        if sys.platform == "win32":
            job = Win32JobObject()
            creationflags = CREATE_SUSPENDED
        else:
            extra_kwargs["start_new_session"] = True

        try:
            proc = subprocess.Popen(
                cmd,
                cwd=str(cwd) if cwd else None,
                stdout=stdout,
                stderr=stderr,
                stdin=stdin,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env or os.environ.copy(),
                creationflags=creationflags,
                **extra_kwargs,
            )
        except Exception:
            if job:
                job.close()
            raise

        if sys.platform == "win32" and job:
            assigned = job.assign_process(int(proc._handle))
            if not assigned:
                try:
                    proc.kill()
                except Exception:
                    pass
                job.close()
                raise RuntimeError(
                    f"[JOB_BIND_FAILED] Failed to bind process PID={proc.pid} to Job Object"
                )

            # Store job object on proc so it stays alive while proc lives
            proc._wink_job = job

            # Resume the suspended process execution
            status = ctypes.windll.ntdll.NtResumeProcess(int(proc._handle))
            if status != 0:
                try:
                    proc.kill()
                except Exception:
                    pass
                job.close()
                raise RuntimeError(
                    f"[PROCESS_RESUME_FAILED] NtResumeProcess failed for PID={proc.pid} (NTSTATUS={status})"
                )

        self.register_process(proc)
        return proc

    def terminate_all(self) -> int:
        count = 0
        for p in self._tracked:
            if p.poll() is None:
                if hasattr(p, "_wink_job") and p._wink_job:
                    p._wink_job.terminate(1)
                    p._wink_job.close()
                    p._wink_job = None
                self.terminate_process_tree(p.pid)
                close_process_pipes(p)
                count += 1
            else:
                if hasattr(p, "_wink_job") and p._wink_job:
                    p._wink_job.close()
                    p._wink_job = None
                close_process_pipes(p)
        return count

    def run_bounded_command(
        self,
        cmd: List[str],
        cwd: Optional[Path] = None,
        timeout_seconds: float = 120.0,
        env: Optional[dict] = None
    ) -> Tuple[int, str, str]:
        p = self.spawn_supervised_process(
            cmd,
            cwd=cwd,
            env=env,
        )
        try:
            stdout, stderr = p.communicate(timeout=timeout_seconds)
            return p.returncode, stdout, stderr
        except subprocess.TimeoutExpired:
            if hasattr(p, "_wink_job") and p._wink_job:
                p._wink_job.terminate(1)
                p._wink_job.close()
                p._wink_job = None
            self.terminate_process_tree(p.pid)
            close_process_pipes(p)
            try:
                stdout, stderr = p.communicate(timeout=2.0)
            except Exception:
                stdout, stderr = "", ""
            return 124, stdout, stderr
        finally:
            if hasattr(p, "_wink_job") and p._wink_job:
                p._wink_job.close()
                p._wink_job = None
            close_process_pipes(p)

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
                try:
                    pgid = os.getpgid(pid)
                    os.killpg(pgid, signal.SIGTERM)
                    time.sleep(min(0.2, graceful_timeout_s))
                    os.killpg(pgid, signal.SIGKILL)
                except Exception:
                    os.kill(pid, signal.SIGTERM)
                    time.sleep(min(0.2, graceful_timeout_s))
                    os.kill(pid, signal.SIGKILL)
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
        supervisor = cls()
        proc: Optional[subprocess.Popen] = None
        try:
            proc = supervisor.spawn_supervised_process(
                cmd,
                cwd=cwd,
                env=env,
            )
            stdout, stderr = proc.communicate(timeout=timeout_sec)
            combined = f"{stdout}\n--- [stderr] ---\n{stderr}" if (stdout and stderr) else (stdout or stderr)
            return proc.returncode, combined.strip()
        except subprocess.TimeoutExpired:
            if proc is not None:
                if hasattr(proc, "_wink_job") and proc._wink_job:
                    proc._wink_job.terminate(1)
                    proc._wink_job.close()
                    proc._wink_job = None
                cls.terminate_process_tree(proc.pid, graceful_timeout_s=1.0)
                close_process_pipes(proc)
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
                if hasattr(proc, "_wink_job") and proc._wink_job:
                    proc._wink_job.terminate(1)
                    proc._wink_job.close()
                    proc._wink_job = None
                cls.terminate_process_tree(proc.pid)
                close_process_pipes(proc)
            return 1, f"[EXECUTION_ERROR] Failed running command: {exc}"
        finally:
            if proc is not None:
                if hasattr(proc, "_wink_job") and proc._wink_job:
                    proc._wink_job.close()
                    proc._wink_job = None
                close_process_pipes(proc)
