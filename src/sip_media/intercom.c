#include "intercom.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// Command definitions
typedef struct {
    intercom_cmd_t cmd;
    const char* name;
    unsigned char code[4];
} intercom_command_def_t;

static intercom_command_def_t command_defs[] = {
    {INTERCOM_CMD_UNLOCK_DOOR,       "UNLOCK_DOOR",       {0xFB, 0x12, 0x01, 0x1E}},
    {INTERCOM_CMD_START_CALL,        "START_CALL",        {0xFB, 0x14, 0x01, 0x20}},
    {INTERCOM_CMD_STOP_CALL,         "STOP_CALL",         {0xFB, 0x14, 0x00, 0x1F}},
    {INTERCOM_CMD_ENABLE_PUSH_STATE, "ENABLE_PUSH_STATE", {0xFB, 0x19, 0x01, 0x25}},
    {INTERCOM_CMD_DISABLE_PUSH_STATE,"DISABLE_PUSH_STATE",{0xFB, 0x19, 0x00, 0x24}},
};

#define NUM_COMMANDS (sizeof(command_defs) / sizeof(command_defs[0]))

static int intercom_initialized = 0;

int intercom_init(void) {
    // Check if device exists
    if (access("/dev/ttySGK1", W_OK) != 0) {
        printf("Warning: Intercom device /dev/ttySGK1 not accessible: %s\n", strerror(errno));
        // Continue anyway - device might become available later
    }

    intercom_initialized = 1;
    printf("Intercom module initialized\n");
    return 0;
}

int intercom_send_command(intercom_cmd_t cmd) {
    if (!intercom_initialized) {
        printf("Intercom not initialized\n");
        return -1;
    }

    // Find command definition
    intercom_command_def_t* cmd_def = NULL;
    int i;
    for (i = 0; i < NUM_COMMANDS; i++) {
        if (command_defs[i].cmd == cmd) {
            cmd_def = &command_defs[i];
            break;
        }
    }

    if (!cmd_def) {
        printf("Unknown intercom command: %d\n", cmd);
        return -1;
    }

    // Open device
    int fd = open("/dev/ttySGK1", O_WRONLY);
    if (fd < 0) {
        printf("Failed to open intercom device: %s\n", strerror(errno));
        return -1;
    }

    // Send command
    ssize_t bytes_written = write(fd, cmd_def->code, 4);
    if (bytes_written != 4) {
        printf("Failed to write intercom command: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    printf("Sent intercom command: %s [%02X %02X %02X %02X]\n",
           cmd_def->name, cmd_def->code[0], cmd_def->code[1],
           cmd_def->code[2], cmd_def->code[3]);
    return 0;
}

void intercom_cleanup(void) {
    intercom_initialized = 0;
    printf("Intercom module cleaned up\n");
}
