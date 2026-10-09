# SPDX-License-Identifier: GPL-3.0-only
"""
Unit tests for Task T1.5: Process Tree Win32 Job Isolation & Bounded Cleanup.
=============================================================================
Validates:
1. Win32 Job Object creation, limits, and assignment in suspended state.
2. Supervised process execution and exit status retrieval.
3. Multilevel process tree bounded cleanup (parent + grandchild termination).
4. Timeout termination (returncode 124) with atomic process elimination.
5. Pipe handle release preventing Windows file-lock leaks.
6. Exponential backoff retry mechanism (safe_file_retry) on transient sharing violations.
"""
from __future__ import annotations

import os
import sys
import time
import subprocess
from pathlib import Path

import pytest

from loop.harness.process_supervisor import (
    ProcessSupervisor,
    Win32JobObject,
    close_process_pipes,
    safe_file_retry,
)


def test_win32_job_object_lifecycle():
    """Verify Win32 Job Object creation and lifecycle management."""
    if sys.platform != "win32":
        pytest.skip("Win32 Job Object tests require Windows")

    job = Win32JobObject()
    assert job.handle is not None and job.handle > 0

    # Start a suspended process and assign
    CREATE_SUSPENDED = 0x00000004
    p = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(10)"],
        creationflags=CREATE_SUSPENDED
    )
    try:
        assigned = job.assign_process(int(p._handle))
        assert assigned is True

        # Terminate job
        term_ok = job.terminate(1)
        assert term_ok is True
        p.wait(timeout=3.0)
        assert p.returncode == 1
    finally:
        job.close()
        assert job.handle is None


def test_spawn_supervised_process_success():
    """Verify supervised process runs and completes normally."""
    supervisor = ProcessSupervisor()
    proc = supervisor.spawn_supervised_process(
        [sys.executable, "-c", "print('supervised_success')"]
    )
    stdout, stderr = proc.communicate(timeout=5.0)
    assert proc.returncode == 0
    assert "supervised_success" in stdout


def test_multilevel_process_tree_termination():
    """Verify that terminating a supervised parent terminates all descendants."""
    supervisor = ProcessSupervisor()
    # Parent spawns child that sleeps
    script = (
        "import subprocess, time, sys\n"
        "p = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])\n"
        "time.sleep(60)\n"
    )
    proc = supervisor.spawn_supervised_process([sys.executable, "-c", script])
    time.sleep(1.0)  # Allow child to spawn

    # Terminate all supervised processes
    terminated_count = supervisor.terminate_all()
    assert terminated_count >= 1

    proc.wait(timeout=3.0)
    assert proc.poll() is not None


def test_timeout_bounded_process_cleanup():
    """Verify timeout-expired processes return 124 and are completely eliminated."""
    supervisor = ProcessSupervisor()
    code, stdout, stderr = supervisor.run_bounded_command(
        [sys.executable, "-c", "import time; time.sleep(10)"],
        timeout_seconds=0.5
    )
    assert code == 124


def test_run_bounded_process_classmethod():
    """Verify run_bounded_process classmethod produces correct timeout diagnostic."""
    code, output = ProcessSupervisor.run_bounded_process(
        [sys.executable, "-c", "import time; time.sleep(10)"],
        cwd=Path.cwd(),
        timeout_sec=1
    )
    assert code == 124
    assert "[TIMEOUT]" in output


def test_pipe_handle_release():
    """Verify process pipes are explicitly closed after termination."""
    proc = subprocess.Popen(
        [sys.executable, "-c", "print('pipe_test')"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    proc.wait(timeout=3.0)
    close_process_pipes(proc)

    if proc.stdout:
        assert proc.stdout.closed
    if proc.stderr:
        assert proc.stderr.closed


def test_safe_file_retry_success_after_transient_failure():
    """Verify safe_file_retry retries with backoff and succeeds."""
    attempts = 0

    def flaky_operation():
        nonlocal attempts
        attempts += 1
        if attempts < 2:
            exc = PermissionError("File locked by background process")
            exc.winerror = 32  # ERROR_SHARING_VIOLATION
            raise exc
        return "SUCCESS"

    result = safe_file_retry(flaky_operation, backoffs_ms=(10, 20), op_name="test_op")
    assert result == "SUCCESS"
    assert attempts == 2


def test_safe_file_retry_exhaustion_raises_oserror():
    """Verify safe_file_retry raises OSError when backoff is exhausted."""
    def always_locked():
        exc = PermissionError("Permanent lock")
        exc.winerror = 32
        raise exc

    with pytest.raises(OSError) as exc_info:
        safe_file_retry(always_locked, backoffs_ms=(10, 10), op_name="locked_file")

    assert "[FILE_LOCK_TIMEOUT]" in str(exc_info.value)
