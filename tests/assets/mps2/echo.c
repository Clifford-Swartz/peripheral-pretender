/*
 * Runner test image: echo UART input lines until "exit <n>".
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * Every received line (terminated by CR, LF or CRLF) is printed back as
 * "echo: <line>". The line "exit <n>" ends the test with PP:EXIT <n>.
 * Without input the image waits forever, which tests the runner timeout.
 */
#include "cmsdk_uart.h"

#define LINE_MAX 200

static char line[LINE_MAX + 1];

static unsigned read_line(void)
{
    unsigned len = 0;
    char c;

    for (;;) {
        c = uart_getc();
        if (c == '\r' || c == '\n') {
            if (len) {
                break;
            }
            continue;       /* skip the LF of CRLF and empty lines */
        }
        if (len < LINE_MAX) {
            line[len++] = c;
        }
    }
    line[len] = '\0';
    return len;
}

static int parse_exit(const char *s, uint32_t *code)
{
    static const char prefix[] = "exit ";
    uint32_t v = 0;

    for (unsigned i = 0; prefix[i]; i++) {
        if (s[i] != prefix[i]) {
            return 0;
        }
    }
    s += sizeof(prefix) - 1;
    if (*s < '0' || *s > '9') {
        return 0;
    }
    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (uint32_t)(*s++ - '0');
    }
    *code = v;
    return *s == '\0';
}

int main(void)
{
    uint32_t code;

    uart_init();
    uart_puts("pp runner test: echo ready\r\n");
    for (;;) {
        read_line();
        uart_puts("echo: ");
        uart_puts(line);
        uart_puts("\r\n");
        if (parse_exit(line, &code)) {
            pp_exit(code);
            idle_forever();
        }
    }
}
