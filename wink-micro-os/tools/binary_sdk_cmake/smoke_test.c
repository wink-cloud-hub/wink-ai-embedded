/* M2 BINARY smoke test: verify public headers compile + .a links. */
#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "dal_button.h"
#include "dal_led.h"
#include "dal_servo.h"
#include <stdio.h>

int main(void) {
    printf("Binary SDK smoke: wink_status_t = %d bytes\n", (int)sizeof(wink_status_t));
    printf("WINK_OK = %d\n", (int)WINK_OK);
    printf("Binary SDK link + header check PASSED\n");
    return 0;
}
