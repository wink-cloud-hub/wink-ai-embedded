#ifndef WINK_LOG_H
#define WINK_LOG_H

/**
 * @file wink_log.h
 * @brief App-facing logging shim.
 *
 * Apps include this instead of reaching into <pal_log.h>.  Today it is a
 * thin re-export of the PAL LOG_E/W/I/D macros so app code never includes
 * a PAL header directly; in the future runtime metadata (uptime, fault
 * sequence number) can be injected here without touching app code.
 *
 * Copyright (c) 2026 Wink-AI. App-facing API — stable across versions.
 */

#include "pal_log.h"

#endif /* WINK_LOG_H */
