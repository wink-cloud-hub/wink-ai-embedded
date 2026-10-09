# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.services.admission_service"""
from loop.services.admission_service import *
from loop.services.admission_service import AdmissionService, AdmissionVerdict, compute_directory_sha256, main

if __name__ == "__main__":
    main()
