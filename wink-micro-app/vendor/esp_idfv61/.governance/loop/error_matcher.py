try:
    from loop.afg.error_matcher import *
except ImportError:
    try:
        from .afg.error_matcher import *
    except (ImportError, ValueError):
        from afg.error_matcher import *

