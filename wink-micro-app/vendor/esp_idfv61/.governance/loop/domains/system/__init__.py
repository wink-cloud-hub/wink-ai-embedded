# SPDX-License-Identifier: Apache-2.0
"""
loop.domains.system - System & Watchdog Domain Verification Handlers
===================================================================
"""
from .twdt_timeout import TwdtTimeoutPipeline, TwdtDomainPlugin

__all__ = [
    "TwdtTimeoutPipeline",
    "TwdtDomainPlugin",
]
