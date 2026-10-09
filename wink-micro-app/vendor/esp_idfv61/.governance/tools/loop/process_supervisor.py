# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.harness.process_supervisor"""
try:
    from loop.harness.process_supervisor import (
        ProcessSupervisor,
        Win32JobObject,
        safe_file_retry,
        close_process_pipes,
    )
except ImportError:
    from harness.process_supervisor import (
        ProcessSupervisor,
        Win32JobObject,
        safe_file_retry,
        close_process_pipes,
    )
