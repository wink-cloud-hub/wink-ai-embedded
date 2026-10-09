#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
cli.run_loop - Autonomous Governance Loop Runner CLI
===================================================
Standard CLI entry point to discover, filter, and execute application scenarios.
"""
from __future__ import annotations

import sys
from loop.pipeline.runner import main

if __name__ == "__main__":
    main()
