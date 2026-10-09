# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for ProcessSupervisor (loop.harness.process_supervisor).
"""
import sys
import subprocess
from loop.harness.process_supervisor import ProcessSupervisor, safe_file_retry


def test_process_supervisor_tracks_and_terminates():
    sup = ProcessSupervisor()
    proc = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(10)"])
    sup.register_process(proc)
    assert proc.poll() is None
    terminated = sup.terminate_all()
    assert terminated >= 1
    assert proc.poll() is not None


def test_safe_file_retry_succeeds_on_transient():
    attempts = [0]
    def op():
        attempts[0] += 1
        if attempts[0] < 2:
            raise PermissionError("Simulated locked file")
        return "success"

    res = safe_file_retry(op, backoffs_ms=(10, 20), op_name="test_retry")
    assert res == "success"
    assert attempts[0] == 2
