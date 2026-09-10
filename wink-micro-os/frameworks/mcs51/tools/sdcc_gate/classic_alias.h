/* SPDX-License-Identifier: Apache-2.0
 * Classic 8052 SFR-name aliases for portable carriers compiled against the
 * CMS8S78xx device header (GAP-03, Tier-S gate). CMS8S names the UART SFRs
 * SBUF0/SCON0/TI0/RI0; textbook/portable code uses the classic names. */
#ifndef MCS51_GATE_CLASSIC_ALIAS_H
#define MCS51_GATE_CLASSIC_ALIAS_H

#define SBUF SBUF0
#define SCON SCON0
#define TI   TI0
#define RI   RI0

#endif
