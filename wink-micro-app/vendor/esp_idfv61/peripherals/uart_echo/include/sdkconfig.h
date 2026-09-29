/* App-level Kconfig overlay for the upstream uart_echo example (vendor suite). */
#include "sdkconfig_base.h"

#define CONFIG_EXAMPLE_UART_PORT_NUM 1
#define CONFIG_EXAMPLE_UART_BAUD_RATE 115200
#define CONFIG_EXAMPLE_UART_TXD 4
#define CONFIG_EXAMPLE_UART_RXD 5
#define CONFIG_EXAMPLE_TASK_STACK_SIZE 4096
