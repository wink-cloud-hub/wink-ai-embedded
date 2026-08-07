// SPDX-License-Identifier: Apache-2.0
#include "input/dal_keypad.h"
#include "hal/pal_hal.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include <string.h>

static const char s_keymap_4x4[DAL_KEYPAD_MAX_ROWS][DAL_KEYPAD_MAX_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static const char s_keymap_3x4[DAL_KEYPAD_MAX_ROWS][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

static char dal_keypad_lookup_char(const dal_keypad_config_t *cfg, uint8_t row, uint8_t col) {
    if (row >= DAL_KEYPAD_MAX_ROWS || col >= DAL_KEYPAD_MAX_COLS) {
        return '\0';
    }
    if (cfg->custom_keymap != NULL || cfg->variant == DAL_KEYPAD_VARIANT_CUSTOM) {
        if (cfg->custom_keymap == NULL) return '\0';
        size_t idx = (size_t)row * (size_t)cfg->num_cols + (size_t)col;
        size_t len = strlen(cfg->custom_keymap);
        if (idx < len) {
            return cfg->custom_keymap[idx];
        }
        return '\0';
    }
    if (cfg->variant == DAL_KEYPAD_VARIANT_MATRIX_3X4) {
        if (col >= 3) return '\0';
        return s_keymap_3x4[row][col];
    }
    /* Default: 4x4 matrix */
    return s_keymap_4x4[row][col];
}

wink_status_t dal_keypad_init(dal_keypad_t *dev, const dal_keypad_config_t *cfg) {
    if (dev == NULL || cfg == NULL || cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    uint8_t num_rows = (cfg->num_rows > 0 && cfg->num_rows <= DAL_KEYPAD_MAX_ROWS) ? cfg->num_rows : DAL_KEYPAD_MAX_ROWS;
    uint8_t num_cols = (cfg->num_cols > 0 && cfg->num_cols <= DAL_KEYPAD_MAX_COLS) ? cfg->num_cols :
                       (cfg->variant == DAL_KEYPAD_VARIANT_MATRIX_3X4 ? 3 : DAL_KEYPAD_MAX_COLS);

    /* Claim row GPIO pins */
    uint8_t claimed_rows = 0;
    for (uint8_t r = 0; r < num_rows; r++) {
        if (cfg->row_pins[r] < 0) {
            continue;
        }
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->row_pins[r], cfg->owner);
        if (wink_status_is_error(st)) {
            for (uint8_t i = 0; i < claimed_rows; i++) {
                if (cfg->row_pins[i] >= 0) {
                    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->row_pins[i], cfg->owner));
                }
            }
            return st;
        }
        claimed_rows++;
    }

    /* Claim col GPIO pins */
    uint8_t claimed_cols = 0;
    for (uint8_t c = 0; c < num_cols; c++) {
        if (cfg->col_pins[c] < 0) {
            continue;
        }
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->col_pins[c], cfg->owner);
        if (wink_status_is_error(st)) {
            for (uint8_t i = 0; i < claimed_cols; i++) {
                if (cfg->col_pins[i] >= 0) {
                    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->col_pins[i], cfg->owner));
                }
            }
            for (uint8_t i = 0; i < num_rows; i++) {
                if (cfg->row_pins[i] >= 0) {
                    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->row_pins[i], cfg->owner));
                }
            }
            return st;
        }
        claimed_cols++;
    }

    /* Initialize row pins as input with pullup */
    for (uint8_t r = 0; r < num_rows; r++) {
        if (cfg->row_pins[r] >= 0) {
            WINK_IGNORE_UNUSED(pal_gpio_init((wink_pin_t)cfg->row_pins[r], PAL_GPIO_INPUT_PULLUP));
        }
    }

    /* Initialize col pins as output push-pull, default HIGH */
    for (uint8_t c = 0; c < num_cols; c++) {
        if (cfg->col_pins[c] >= 0) {
            WINK_IGNORE_UNUSED(pal_gpio_init((wink_pin_t)cfg->col_pins[c], PAL_GPIO_OUTPUT_PUSH_PULL));
            WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)cfg->col_pins[c], true));
        }
    }

    memcpy(&dev->config, cfg, sizeof(dal_keypad_config_t));
    dev->config.num_rows = num_rows;
    dev->config.num_cols = num_cols;
    if (dev->config.debounce_ms == 0) {
        dev->config.debounce_ms = 10; /* Default Guard C 10ms debounce */
    }

    dev->last_scan_ms = 0;
    dev->last_key = '\0';
    dev->last_row = 0xFF;
    dev->last_col = 0xFF;
    dev->is_pressed = false;
    dev->last_status = WINK_OK;
    dev->initialized = true;

    return WINK_OK;
}

wink_status_t dal_keypad_deinit(dal_keypad_t *dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    for (uint8_t r = 0; r < dev->config.num_rows; r++) {
        if (dev->config.row_pins[r] >= 0) {
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)dev->config.row_pins[r], dev->config.owner));
        }
    }
    for (uint8_t c = 0; c < dev->config.num_cols; c++) {
        if (dev->config.col_pins[c] >= 0) {
            WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.col_pins[c], true));
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)dev->config.col_pins[c], dev->config.owner));
        }
    }

    memset(dev, 0, sizeof(dal_keypad_t));
    return WINK_OK;
}

static char dal_keypad_scan_matrix(const dal_keypad_t *dev, uint8_t *out_row, uint8_t *out_col) {
    uint8_t num_rows = dev->config.num_rows;
    uint8_t num_cols = dev->config.num_cols;

    for (uint8_t c = 0; c < num_cols; c++) {
        int16_t c_pin = dev->config.col_pins[c];
        if (c_pin < 0) continue;

        /* Drive column pin LOW to scan */
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)c_pin, false));

        for (uint8_t r = 0; r < num_rows; r++) {
            int16_t r_pin = dev->config.row_pins[r];
            if (r_pin < 0) continue;

            bool level = true;
            if (wink_status_is_success(pal_gpio_read((wink_pin_t)r_pin, &level))) {
                if (!level) { /* LOW level means key at (r, c) is pressed */
                    WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)c_pin, true));
                    if (out_row) *out_row = r;
                    if (out_col) *out_col = c;
                    return dal_keypad_lookup_char(&dev->config, r, c);
                }
            }
        }

        /* Restore column pin to HIGH */
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)c_pin, true));
    }

    if (out_row) *out_row = 0xFF;
    if (out_col) *out_col = 0xFF;
    return '\0';
}

wink_status_t dal_keypad_get_key(dal_keypad_t *dev, char *out_key) {
    if (dev == NULL || out_key == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    uint8_t r = 0xFF, c = 0xFF;
    char key = dal_keypad_scan_matrix(dev, &r, &c);

    dev->last_key = key;
    dev->last_row = r;
    dev->last_col = c;
    dev->is_pressed = (key != '\0');
    dev->last_status = WINK_OK;
    *out_key = key;

    return WINK_OK;
}

wink_status_t dal_keypad_is_pressed(const dal_keypad_t *dev, bool *out_pressed) {
    if (dev == NULL || out_pressed == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    uint8_t r = 0xFF, c = 0xFF;
    char key = dal_keypad_scan_matrix(dev, &r, &c);
    *out_pressed = (key != '\0');
    return WINK_OK;
}

wink_status_t dal_keypad_poll(dal_keypad_t *dev, bool *out_changed, char *out_key) {
    if (dev == NULL || out_changed == NULL || out_key == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    char prev_key = dev->last_key;
    uint32_t now = pal_os_get_ms();

    /* Apply Guard E non-blocking debounce timing */
    if (now - dev->last_scan_ms < dev->config.debounce_ms) {
        *out_changed = false;
        *out_key = dev->last_key;
        return WINK_OK;
    }

    dev->last_scan_ms = now;
    uint8_t r = 0xFF, c = 0xFF;
    char current_key = dal_keypad_scan_matrix(dev, &r, &c);

    bool changed = (current_key != prev_key);
    dev->last_key = current_key;
    dev->last_row = r;
    dev->last_col = c;
    dev->is_pressed = (current_key != '\0');
    dev->last_status = WINK_OK;

    *out_changed = changed;
    *out_key = current_key;
    return WINK_OK;
}

wink_status_t dal_keypad_get_status(const dal_keypad_t *dev, wink_status_t *out_status) {
    if (dev == NULL || out_status == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    *out_status = dev->last_status;
    return WINK_OK;
}
