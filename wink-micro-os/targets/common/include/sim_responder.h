// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file sim_responder.h
 * @brief Pluggable Virtual Peripheral Responders for Simulation Targets (ADR-0092 Tier 2).
 */
#ifndef SIM_RESPONDER_H
#define SIM_RESPONDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIM_BUS_TYPE_I2C = 0,
    SIM_BUS_TYPE_SPI = 1,
    SIM_BUS_TYPE_UART = 2,
} sim_bus_type_t;

typedef struct sim_responder_t sim_responder_t;

typedef wink_status_t (*sim_responder_transfer_fn)(sim_responder_t *self,
                                                   const uint8_t *write_buf, size_t write_len,
                                                   uint8_t *read_buf, size_t read_len);

struct sim_responder_t {
    sim_bus_type_t bus_type;
    uint8_t        port;
    uint16_t       address;     /* e.g. 7-bit I2C address, or SPI CS pin */
    bool           in_use;
    sim_responder_transfer_fn on_transfer;
    void          *user_data;
    sim_responder_t *next;
};

/** Register a virtual peripheral responder */
wink_status_t sim_responder_register(sim_responder_t *responder);

/** Unregister a virtual peripheral responder */
wink_status_t sim_responder_unregister(sim_responder_t *responder);

/** Reset all registered responders */
void sim_responder_reset_all(void);

/** Dispatch a bus transaction to registered responders. Returns WINK_ERR_NOT_FOUND if no responder matched. */
wink_status_t sim_responder_dispatch(sim_bus_type_t bus_type, uint8_t port, uint16_t address,
                                     const uint8_t *write_buf, size_t write_len,
                                     uint8_t *read_buf, size_t read_len);

/* ── Standard Built-in Responder: AT24C02 I2C EEPROM ── */
#define SIM_EEPROM_AT24C02_SIZE 256u

typedef struct {
    sim_responder_t base;
    uint8_t         memory[SIM_EEPROM_AT24C02_SIZE];
    uint8_t         word_addr;
} sim_i2c_eeprom_at24c02_t;

/** Initialize and register an AT24C02 virtual EEPROM on specified port & address */
wink_status_t sim_i2c_eeprom_at24c02_init(sim_i2c_eeprom_at24c02_t *eeprom,
                                          uint8_t port, uint16_t address);

/** Reset AT24C02 memory contents (e.g. fill with 0xFF) */
void sim_i2c_eeprom_at24c02_reset(sim_i2c_eeprom_at24c02_t *eeprom);

#ifdef __cplusplus
}
#endif

#endif /* SIM_RESPONDER_H */
