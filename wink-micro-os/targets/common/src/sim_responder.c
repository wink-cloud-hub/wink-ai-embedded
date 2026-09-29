// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file sim_responder.c
 * @brief Implementation of Pluggable Virtual Peripheral Responders for Simulation Targets.
 */

#include "sim_responder.h"
#include <string.h>

#define SIM_MAX_RESPONDERS 16u

static sim_responder_t *s_responders[SIM_MAX_RESPONDERS];
static uint32_t s_responder_count = 0u;

wink_status_t sim_responder_register(sim_responder_t *responder)
{
    if (responder == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Check if already registered */
    for (uint32_t i = 0u; i < s_responder_count; i++) {
        if (s_responders[i] == responder) {
            return WINK_OK;
        }
        if (s_responders[i]->bus_type == responder->bus_type &&
            s_responders[i]->port == responder->port &&
            s_responders[i]->address == responder->address) {
            /* Address collision on same bus */
            return WINK_ERR_INVALID_STATE;
        }
    }

    if (s_responder_count >= SIM_MAX_RESPONDERS) {
        return WINK_ERR_NO_MEM;
    }

    responder->in_use = true;
    s_responders[s_responder_count++] = responder;
    return WINK_OK;
}

wink_status_t sim_responder_unregister(sim_responder_t *responder)
{
    if (responder == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0u; i < s_responder_count; i++) {
        if (s_responders[i] == responder) {
            responder->in_use = false;
            /* Shift down remaining */
            for (uint32_t j = i; j + 1u < s_responder_count; j++) {
                s_responders[j] = s_responders[j + 1u];
            }
            s_responders[s_responder_count - 1u] = NULL;
            s_responder_count--;
            return WINK_OK;
        }
    }

    return WINK_ERR_NOT_FOUND;
}

void sim_responder_reset_all(void)
{
    for (uint32_t i = 0u; i < s_responder_count; i++) {
        if (s_responders[i] != NULL) {
            s_responders[i]->in_use = false;
            s_responders[i] = NULL;
        }
    }
    s_responder_count = 0u;
}

wink_status_t sim_responder_dispatch(sim_bus_type_t bus_type, uint8_t port, uint16_t address,
                                     const uint8_t *write_buf, size_t write_len,
                                     uint8_t *read_buf, size_t read_len)
{
    sim_responder_t *target = NULL;
    for (uint32_t i = 0u; i < s_responder_count; i++) {
        sim_responder_t *r = s_responders[i];
        if (r != NULL && r->in_use &&
            r->bus_type == bus_type &&
            r->port == port &&
            r->address == address) {
            target = r;
            break;
        }
    }

    if (target == NULL) {
        return WINK_ERR_NOT_FOUND;
    }

    /* Zero-length probe (I2C probe / ACK test) */
    if (write_len == 0u && read_len == 0u) {
        return WINK_OK;
    }

    if (target->on_transfer != NULL) {
        return target->on_transfer(target, write_buf, write_len, read_buf, read_len);
    }

    return WINK_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Built-in Responder: AT24C02 I2C EEPROM
 * ═══════════════════════════════════════════════════════════════════════════ */

static wink_status_t at24c02_transfer(sim_responder_t *self,
                                      const uint8_t *write_buf, size_t write_len,
                                      uint8_t *read_buf, size_t read_len)
{
    sim_i2c_eeprom_at24c02_t *eeprom = (sim_i2c_eeprom_at24c02_t *)self;
    if (eeprom == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Write phase */
    if (write_buf != NULL && write_len > 0u) {
        eeprom->word_addr = write_buf[0];
        for (size_t i = 1u; i < write_len; i++) {
            eeprom->memory[eeprom->word_addr] = write_buf[i];
            eeprom->word_addr = (uint8_t)((eeprom->word_addr + 1u) & 0xFFu);
        }
    }

    /* Read phase */
    if (read_buf != NULL && read_len > 0u) {
        for (size_t i = 0u; i < read_len; i++) {
            read_buf[i] = eeprom->memory[eeprom->word_addr];
            eeprom->word_addr = (uint8_t)((eeprom->word_addr + 1u) & 0xFFu);
        }
    }

    return WINK_OK;
}

wink_status_t sim_i2c_eeprom_at24c02_init(sim_i2c_eeprom_at24c02_t *eeprom,
                                          uint8_t port, uint16_t address)
{
    if (eeprom == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    memset(eeprom, 0, sizeof(*eeprom));
    memset(eeprom->memory, 0xFF, sizeof(eeprom->memory));

    eeprom->base.bus_type = SIM_BUS_TYPE_I2C;
    eeprom->base.port = port;
    eeprom->base.address = address;
    eeprom->base.on_transfer = at24c02_transfer;
    eeprom->base.user_data = eeprom;

    return sim_responder_register(&eeprom->base);
}

void sim_i2c_eeprom_at24c02_reset(sim_i2c_eeprom_at24c02_t *eeprom)
{
    if (eeprom != NULL) {
        memset(eeprom->memory, 0xFF, sizeof(eeprom->memory));
        eeprom->word_addr = 0u;
    }
}
