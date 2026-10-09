# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.services.defect_feedback"""
from loop.services.defect_feedback import *
from loop.services.defect_feedback import DefectFeedbackManager, main

if __name__ == "__main__":
    main()
