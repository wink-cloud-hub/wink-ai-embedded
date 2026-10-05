/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_CONSOLE_H
#define ESP_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart_vfs.h"

typedef struct linenoiseCompletions linenoiseCompletions;

/**
 * @brief Parameters for console initialization
 */
typedef struct {
    size_t max_cmdline_length;  //!< length of command line buffer, in bytes
    size_t max_cmdline_args;    //!< maximum number of command line arguments to parse
    uint32_t heap_alloc_caps;   //!< heap allocation caps
    int hint_color;             //!< ASCII color code of hint text
    int hint_bold;              //!< Set to 1 to print hint text in bold
} esp_console_config_t;

#define ESP_CONSOLE_CONFIG_DEFAULT() \
    { \
        .max_cmdline_length = 256, \
        .max_cmdline_args = 32, \
        .heap_alloc_caps = 0, \
        .hint_color = 39, \
        .hint_bold = 0 \
    }

/**
 * @brief Parameters for console REPL (Read Eval Print Loop)
 */
typedef struct {
    uint32_t max_history_len;      //!< maximum length for the history
    const char *history_save_path; //!< file path used to save history commands
    uint32_t task_stack_size;      //!< repl task stack size
    uint32_t task_priority;        //!< repl task priority
    BaseType_t task_core_id;       //!< repl task affinity
    const char *prompt;            //!< prompt (NULL represents default: "esp32> ")
    size_t max_cmdline_length;     //!< maximum length of a command line
    size_t max_cmdline_args;       //!< maximum number of command line arguments to parse
} esp_console_repl_config_t;

#define ESP_CONSOLE_REPL_CONFIG_DEFAULT() \
    { \
        .max_history_len = 32, \
        .history_save_path = NULL, \
        .task_stack_size = 4096, \
        .task_priority = 2, \
        .task_core_id = tskNO_AFFINITY, \
        .prompt = NULL, \
        .max_cmdline_length = 0, \
        .max_cmdline_args = 0, \
    }

#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM 0
#endif

#define ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT() \
    { \
        .channel = CONFIG_ESP_CONSOLE_UART_NUM, \
        .baud_rate = 115200, \
        .tx_gpio_num = -1, \
        .rx_gpio_num = -1, \
    }

typedef int (*esp_console_cmd_func_t)(int argc, char **argv);
typedef int (*esp_console_cmd_func_with_context_t)(void *context, int argc, char **argv);

/**
 * @brief Console command description
 */
typedef struct {
    const char *command;
    const char *help;
    const char *hint;
    esp_console_cmd_func_t func;
    void *argtable;
    esp_console_cmd_func_with_context_t func_w_context;
    void *context;
} esp_console_cmd_t;

esp_err_t esp_console_init(const esp_console_config_t *config);
esp_err_t esp_console_deinit(void);

esp_err_t esp_console_cmd_register(const esp_console_cmd_t *cmd);
esp_err_t esp_console_cmd_deregister(const char *cmd_name);
esp_err_t esp_console_run(const char *cmdline, int *cmd_ret);
size_t esp_console_split_argv(char *line, char **argv, size_t argv_size);

esp_err_t esp_console_register_help_command(void);
esp_err_t esp_console_deregister_help_command(void);

typedef struct esp_console_repl_s esp_console_repl_t;

struct esp_console_repl_s {
    esp_err_t (*del)(esp_console_repl_t *repl);
    char prompt[64];
    uint8_t uart_channel;
    TaskHandle_t task_handle;
    bool running;
};

esp_err_t esp_console_new_repl_uart(const esp_console_dev_uart_config_t *dev_config,
                                    const esp_console_repl_config_t *repl_config,
                                    esp_console_repl_t **ret_repl);

esp_err_t esp_console_start_repl(esp_console_repl_t *repl);
esp_err_t esp_console_stop_repl(esp_console_repl_t *repl);

#ifdef __cplusplus
}
#endif

#endif /* ESP_CONSOLE_H */
