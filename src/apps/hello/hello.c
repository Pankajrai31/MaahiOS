/**
 * hello.c - First MaahiOS .mex application
 *
 * Demonstrates a clean .mex app using MaahiOS libraries.
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libprocess/libprocess.h"

/* ─── Application entry point ─── */

void mex_main(void) {
    liblog_init();

    liblog(LOG_INFO, "HELLO", "Hello from the first .mex application!");
    liblog_hex(LOG_INFO, "HELLO", "My PID is", libprocess_get_pid());
    liblog(LOG_INFO, "HELLO", "Entering idle loop...");

    /* Stay alive */
    while (1) {
        libprocess_sleep(1000);
    }
}
