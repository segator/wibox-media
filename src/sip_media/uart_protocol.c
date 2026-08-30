#include "uart_protocol.h"

#include <ctype.h>
#include <string.h>

static const uart_code_def_t uart_codes[] = {
    {UART_CODE_ALARM_REPORT,    "ALARM_REPORT",    "alarm_report",    {0xFB, 0x11, 0x00, 0x1C}},
    {UART_CODE_CMD_RESET,       "CMD_RESET",       "cmd_reset",       {0xFB, 0x20, 0x00, 0x2B}},
    {UART_CODE_STA_TO_AP,       "STA_TO_AP",       "sta_to_ap",       {0xFB, 0x21, 0x00, 0x2C}},
    {UART_CODE_START_CALL,      "START_CALL",      "start_call",      {0xFB, 0x14, 0x01, 0x20}},
    {UART_CODE_HANG_UP_0,       "HANG_UP_0",       "hang_up_0",       {0xFB, 0x13, 0x00, 0x1E}},
    {UART_CODE_HANG_UP_1,       "HANG_UP_1",       "hang_up_1",       {0xFB, 0x13, 0x01, 0x1F}},
    {UART_CODE_PHYSICAL_HANDSET_ANSWERED,
     "PHYSICAL_HANDSET_ANSWERED", "physical_handset_answered",
     {0xFB, 0x23, 0x00, 0x2E}},
    {UART_CODE_PUSH_STATE_0,    "PUSH_STATE_0",    "push_state_0",    {0xFB, 0x19, 0x00, 0x24}},
    {UART_CODE_PUSH_STATE_1,    "PUSH_STATE_1",    "push_state_1",    {0xFB, 0x19, 0x01, 0x25}},
    {UART_CODE_MCU_STATE_0,     "MCU_STATE_0",     "mcu_state_0",     {0xFB, 0x16, 0x00, 0x21}},
    {UART_CODE_MCU_STATE_1,     "MCU_STATE_1",     "mcu_state_1",     {0xFB, 0x16, 0x01, 0x22}},
    {UART_CODE_CMD_DOWN_LONG_1, "CMD_DOWN_LONG_1", "cmd_down_long_1", {0xFB, 0x24, 0x01, 0x30}},
    {UART_CODE_CMD_DOWN_LONG_2, "CMD_DOWN_LONG_2", "cmd_down_long_2", {0xFB, 0x24, 0x02, 0x31}}
};

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

const uart_code_def_t *uart_protocol_find(const unsigned char frame[4])
{
    size_t i;

    if (!frame) return NULL;
    for (i = 0; i < sizeof(uart_codes) / sizeof(uart_codes[0]); i++) {
        if (memcmp(frame, uart_codes[i].bytes, 4) == 0) {
            return &uart_codes[i];
        }
    }
    return NULL;
}

int uart_protocol_parse_control_frame(const char *input, unsigned char frame[4])
{
    const char *p;
    int count = 0;

    if (!input || !frame || strncmp(input, "UART", 4) != 0) return -1;
    p = input + 4;

    while (*p && count < 4) {
        int hi;
        int lo;

        while (*p && (isspace((unsigned char)*p) || *p == ':' || *p == '-')) p++;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        if (p[0] == '\\' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        // Check p[0] before reading p[1]: after the separator/prefix skips above
        // p may sit on the NUL terminator, and reading p[1] would be one byte
        // out of bounds. hex_nibble('\0') < 0, so this bails before touching p[1].
        hi = hex_nibble(p[0]);
        if (hi < 0) return -1;
        lo = hex_nibble(p[1]);
        if (lo < 0) return -1;
        frame[count++] = (unsigned char)((hi << 4) | lo);
        p += 2;
    }

    while (*p && isspace((unsigned char)*p)) p++;
    return count == 4 && *p == '\0' ? 0 : -1;
}

void uart_protocol_format_bytes(const unsigned char *data, size_t len,
                                char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    size_t pos = 0;

    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!data) return;

    for (i = 0; i < len && pos + 4 < out_size; i++) {
        if (i > 0) out[pos++] = ' ';
        out[pos++] = hex[(data[i] >> 4) & 0x0f];
        out[pos++] = hex[data[i] & 0x0f];
    }
    out[pos] = '\0';
}
