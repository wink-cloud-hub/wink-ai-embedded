# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.services.promotion_service"""
from loop.services.promotion_service import *
from loop.services.promotion_service import PromotionService, compute_file_sha256, main

if __name__ == "__main__":
    main()
