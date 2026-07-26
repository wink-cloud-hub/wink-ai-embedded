#!/usr/bin/env python3
"""wink.py — Unified build orchestrator and CLI gateway for Wink Micro OS."""

import sys
from pathlib import Path

# Ensure SDK root is on sys.path
SDK_ROOT = Path(__file__).resolve().parent.parent
if str(SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(SDK_ROOT))

from tools.cli.bootstrap import bootstrap
from tools.cli.dispatcher import dispatch


def main():
    ctx = bootstrap(sys.argv[1:])
    rc = dispatch(ctx, sys.argv[1:])
    if rc is not None and rc != 0:
        sys.exit(rc)


if __name__ == "__main__":
    main()
