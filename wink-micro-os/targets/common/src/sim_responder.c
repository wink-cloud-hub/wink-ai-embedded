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

static sim_i2c_sensor_mpu9250_t s_default_mpu9250;
static bool s_default_mpu9250_inited = false;
static sim_i2c_eeprom_at24c02_t s_default_at24c02;
static bool s_default_at24c02_inited = false;

void sim_responder_reset_all(void)
{
    for (uint32_t i = 0u; i < s_responder_count; i++) {
        if (s_responders[i] != NULL) {
            s_responders[i]->in_use = false;
            s_responders[i] = NULL;
        }
    }
    s_responder_count = 0u;
    s_default_mpu9250_inited = false;
    s_default_at24c02_inited = false;
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
        if (bus_type == SIM_BUS_TYPE_I2C && (address == 0x68u || address == 0x69u)) {
            if (!s_default_mpu9250_inited) {
                sim_i2c_sensor_mpu9250_init(&s_default_mpu9250, port, address);
                s_default_mpu9250_inited = true;
            }
            target = &s_default_mpu9250.base;
        } else if (bus_type == SIM_BUS_TYPE_I2C && (address >= 0x50u && address <= 0x57u)) {
            if (!s_default_at24c02_inited) {
                sim_i2c_eeprom_at24c02_init(&s_default_at24c02, port, address);
                s_default_at24c02_inited = true;
            }
            target = &s_default_at24c02.base;
        } else {
            return WINK_ERR_NOT_FOUND;
        }
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

    /* 1. Combined transaction with Read phase (e.g. i2c_master_transmit_receive) */
    if (read_buf != NULL && read_len > 0u) {
        if (write_buf != NULL && write_len > 0u) {
            if (write_len == 1u) {
                eeprom->word_addr = write_buf[0];
            } else {
                /* 16-bit word address (big endian) */
                eeprom->word_addr = (uint8_t)(((uint16_t)write_buf[write_len - 2u] << 8) | write_buf[write_len - 1u]);
            }
        }
        for (size_t i = 0u; i < read_len; i++) {
            read_buf[i] = eeprom->memory[eeprom->word_addr];
            eeprom->word_addr = (uint8_t)((eeprom->word_addr + 1u) & 0xFFu);
        }
        return WINK_OK;
    }

    /* 2. Pure Write phase */
    if (write_buf != NULL && write_len > 0u) {
        if (write_len == 1u) {
            eeprom->word_addr = write_buf[0];
        } else if (write_buf[0] == 0u && write_len > 2u) {
            /* 16-bit word address with payload (e.g. address 0x0010, payload starting at byte 2) */
            eeprom->word_addr = (uint8_t)(((uint16_t)write_buf[0] << 8) | write_buf[1]);
            for (size_t i = 2u; i < write_len; i++) {
                eeprom->memory[eeprom->word_addr] = write_buf[i];
                eeprom->word_addr = (uint8_t)((eeprom->word_addr + 1u) & 0xFFu);
            }
        } else {
            /* 8-bit word address with payload */
            eeprom->word_addr = write_buf[0];
            for (size_t i = 1u; i < write_len; i++) {
                eeprom->memory[eeprom->word_addr] = write_buf[i];
                eeprom->word_addr = (uint8_t)((eeprom->word_addr + 1u) & 0xFFu);
            }
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Built-in Responder: MPU9250 I2C IMU
 * ═══════════════════════════════════════════════════════════════════════════ */

static wink_status_t mpu9250_transfer(sim_responder_t *self,
                                      const uint8_t *write_buf, size_t write_len,
                                      uint8_t *read_buf, size_t read_len)
{
    sim_i2c_sensor_mpu9250_t *mpu = (sim_i2c_sensor_mpu9250_t *)self;
    if (mpu == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Write phase: write_buf[0] is register pointer, subsequent bytes are written */
    if (write_buf != NULL && write_len > 0u) {
        mpu->current_reg = write_buf[0] & 0x7Fu;
        for (size_t i = 1u; i < write_len; i++) {
            uint8_t reg = (uint8_t)((mpu->current_reg + (i - 1u)) & 0x7Fu);
            mpu->registers[reg] = write_buf[i];
            /* Soft reset on PWR_MGMT_1 bit 7 */
            if (reg == 0x6Bu && (write_buf[i] & 0x80u)) {
                sim_i2c_sensor_mpu9250_reset(mpu);
            }
        }
    }

    /* Read phase */
    if (read_buf != NULL && read_len > 0u) {
        for (size_t i = 0u; i < read_len; i++) {
            uint8_t reg = (uint8_t)((mpu->current_reg + i) & 0x7Fu);
            read_buf[i] = mpu->registers[reg];
        }
    }

    return WINK_OK;
}

wink_status_t sim_i2c_sensor_mpu9250_init(sim_i2c_sensor_mpu9250_t *mpu,
                                          uint8_t port, uint16_t address)
{
    if (mpu == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    memset(mpu, 0, sizeof(*mpu));
    sim_i2c_sensor_mpu9250_reset(mpu);

    mpu->base.bus_type = SIM_BUS_TYPE_I2C;
    mpu->base.port = port;
    mpu->base.address = address;
    mpu->base.on_transfer = mpu9250_transfer;
    mpu->base.user_data = mpu;

    return sim_responder_register(&mpu->base);
}

void sim_i2c_sensor_mpu9250_reset(sim_i2c_sensor_mpu9250_t *mpu)
{
    if (mpu != NULL) {
        memset(mpu->registers, 0, sizeof(mpu->registers));
        mpu->registers[0x75] = 0x71u; /* WHO_AM_I default 0x71 */
        mpu->registers[0x6B] = 0x01u; /* PWR_MGMT_1 default clock */
        mpu->current_reg = 0u;
    }
}
