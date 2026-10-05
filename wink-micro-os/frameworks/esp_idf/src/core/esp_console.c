/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_console.h"
#include "esp_log.h"
#include "hal/pal_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static inline int esp_console_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        fputs(buf, stdout);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)n);
    }
    return n;
}

#undef printf
#define printf esp_console_printf

#define MAX_CONSOLE_CMDS 64

static esp_console_cmd_t s_commands[MAX_CONSOLE_CMDS];
static size_t s_cmd_count = 0;
static esp_console_repl_t s_global_repl;

size_t esp_console_split_argv(char *line, char **argv, size_t argv_size) {
    if (!line || !argv || argv_size == 0) {
        return 0;
    }
    size_t argc = 0;
    char *p = line;
    while (*p && argc + 1 < argv_size) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) {
            break;
        }
        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') {
                p++;
            }
            if (*p == '"') {
                *p++ = '\0';
            }
        } else {
            argv[argc++] = p;
            while (*p && !isspace((unsigned char)*p)) {
                p++;
            }
            if (*p) {
                *p++ = '\0';
            }
        }
    }
    argv[argc] = NULL;
    return argc;
}

esp_err_t esp_console_init(const esp_console_config_t *config) {
    (void)config;
    s_cmd_count = 0;
    return ESP_OK;
}

esp_err_t esp_console_deinit(void) {
    s_cmd_count = 0;
    return ESP_OK;
}

esp_err_t esp_console_cmd_register(const esp_console_cmd_t *cmd) {
    if (!cmd || !cmd->command) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cmd->func && !cmd->func_w_context) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cmd->func && cmd->func_w_context) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < s_cmd_count; i++) {
        if (strcmp(s_commands[i].command, cmd->command) == 0) {
            s_commands[i] = *cmd;
            return ESP_OK;
        }
    }
    if (s_cmd_count >= MAX_CONSOLE_CMDS) {
        return ESP_ERR_NO_MEM;
    }
    s_commands[s_cmd_count++] = *cmd;
    return ESP_OK;
}

esp_err_t esp_console_cmd_deregister(const char *cmd_name) {
    if (!cmd_name) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < s_cmd_count; i++) {
        if (strcmp(s_commands[i].command, cmd_name) == 0) {
            for (size_t j = i; j + 1 < s_cmd_count; j++) {
                s_commands[j] = s_commands[j + 1];
            }
            s_cmd_count--;
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_console_run(const char *cmdline, int *cmd_ret) {
    if (!cmdline) {
        return ESP_ERR_INVALID_ARG;
    }
    char copy[1024];
    strncpy(copy, cmdline, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    size_t len = strlen(copy);
    while (len > 0 && (copy[len - 1] == '\r' || copy[len - 1] == '\n')) {
        copy[--len] = '\0';
    }
    char *argv[32];
    size_t argc = esp_console_split_argv(copy, argv, 32);
    if (argc == 0) {
        return ESP_OK;
    }

    for (size_t i = 0; i < s_cmd_count; i++) {
        if (strcmp(s_commands[i].command, argv[0]) == 0) {
            int ret = 0;
            if (s_commands[i].func) {
                ret = s_commands[i].func((int)argc, argv);
            } else if (s_commands[i].func_w_context) {
                ret = s_commands[i].func_w_context(s_commands[i].context, (int)argc, argv);
            }
            if (cmd_ret) {
                *cmd_ret = ret;
            }
            return ESP_OK;
        }
    }
    printf("Command '%s' not found. Type 'help' to see available commands.\n", argv[0]);
    return ESP_ERR_NOT_FOUND;
}

static int esp_console_help_cmd_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("\nAvailable commands:\n");
    for (size_t i = 0; i < s_cmd_count; i++) {
        printf("  %-24s %s\n", s_commands[i].command, s_commands[i].help ? s_commands[i].help : "");
    }
    printf("\n");
    return 0;
}

esp_err_t esp_console_register_help_command(void) {
    esp_console_cmd_t help_cmd = {
        .command = "help",
        .help = "Print the list of registered commands",
        .hint = NULL,
        .func = &esp_console_help_cmd_handler,
    };
    return esp_console_cmd_register(&help_cmd);
}

esp_err_t esp_console_deregister_help_command(void) {
    return esp_console_cmd_deregister("help");
}

static void esp_console_repl_task(void *arg) {
    esp_console_repl_t *repl = (esp_console_repl_t *)arg;
    uint8_t port = repl->uart_channel;
    char line_buf[1024];
    size_t line_len = 0;

    // Emit prompt initially
    printf("%s", repl->prompt);
    fflush(stdout);

    while (repl->running) {
        uint8_t ch = 0;
        uint32_t read_bytes = 0;
        wink_status_t st = pal_uart_read(port, &ch, 1, &read_bytes);
        if (st == WINK_OK && read_bytes > 0) {
            if (ch == '\r' || ch == '\n') {
                line_buf[line_len] = '\0';
                printf("\n");
                fflush(stdout);
                if (line_len > 0) {
                    int ret = 0;
                    esp_console_run(line_buf, &ret);
                    line_len = 0;
                }
                printf("%s", repl->prompt);
                fflush(stdout);
            } else if (ch == '\b' || ch == 127) {
                if (line_len > 0) {
                    line_len--;
                }
            } else {
                if (line_len + 1 < sizeof(line_buf)) {
                    line_buf[line_len++] = (char)ch;
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    vTaskDelete(NULL);
}

static esp_err_t esp_console_repl_del(esp_console_repl_t *repl) {
    if (!repl) {
        return ESP_ERR_INVALID_ARG;
    }
    repl->running = false;
    return ESP_OK;
}

esp_err_t esp_console_new_repl_uart(const esp_console_dev_uart_config_t *dev_config,
                                    const esp_console_repl_config_t *repl_config,
                                    esp_console_repl_t **ret_repl) {
    if (!ret_repl) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&s_global_repl, 0, sizeof(s_global_repl));
    s_global_repl.del = &esp_console_repl_del;
    s_global_repl.uart_channel = dev_config ? (uint8_t)dev_config->channel : 0;
    s_global_repl.running = false;

    if (repl_config && repl_config->prompt) {
        strncpy(s_global_repl.prompt, repl_config->prompt, sizeof(s_global_repl.prompt) - 1);
    } else {
        strncpy(s_global_repl.prompt, "esp32> ", sizeof(s_global_repl.prompt) - 1);
    }

    *ret_repl = &s_global_repl;
    return ESP_OK;
}

esp_err_t esp_console_start_repl(esp_console_repl_t *repl) {
    if (!repl) {
        return ESP_ERR_INVALID_ARG;
    }
    if (repl->running) {
        return ESP_ERR_INVALID_STATE;
    }
    repl->running = true;
    BaseType_t ok = xTaskCreatePinnedToCore(
        esp_console_repl_task,
        "esp_console_repl",
        4096,
        repl,
        2,
        &repl->task_handle,
        tskNO_AFFINITY
    );
    if (ok != pdPASS) {
        repl->running = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_console_stop_repl(esp_console_repl_t *repl) {
    if (!repl) {
        return ESP_ERR_INVALID_ARG;
    }
    repl->running = false;
    return ESP_OK;
}
