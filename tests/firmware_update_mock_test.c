#define _GNU_SOURCE
#define WIBOX_OTA_GUARD_PATH "/tmp/wibox-fw-test.guard"
#define FLASH_DEVICE "/tmp/wibox-fw-test.mtd"
#define FLASH_VERIFY_DEVICE "/tmp/wibox-fw-test.mtd"
#define FLASH_MOUNTPOINT "/tmp/wibox-fw-test.mount"
#define MAX_IMAGE_SIZE 8192
#define main firmware_update_program_main
#include "../src/firmware_update.c"
#undef main

#include <stdarg.h>

#define TEST_IMAGE "/tmp/wibox-fw-test.img"
#define TEST_WATCHDOG "/tmp/wibox-fw-test.watchdog"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int flash_fd = -1;
static int watchdog_fd = -1;
static int flash_open_fails;
static int memgetinfo_fails;
static int erase_fail_at;
static int erase_count;
static int unmount_fails;
static int flash_write_fails;
static int flash_fsync_errno;
static int corrupt_after_close;
static int reboot_fails;
static int reboot_count;
static int watchdog_ioctl_fails;
static uint32_t mock_mtd_size = 8192;
static uint32_t mock_erase_size = 4096;
static char ssl_response[4096];
static size_t ssl_response_len;
static size_t ssl_response_offset;
static int ssl_server_port;
static int ssl_ctx_new_fails;
static int ssl_ca_load_fails;
static int ssl_new_fails;
static int ssl_set1_host_fails;
static int ssl_connect_fails;
static int ssl_verify_fails;
static int ssl_write_fails;

int __real_open(const char *path, int flags, ...);
ssize_t __real_write(int fd, const void *buffer, size_t length);
int __real_close(int fd);
int __real_fsync(int fd);

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list args;
    int fd;

    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    if (strcmp(path, FLASH_DEVICE) == 0 && (flags & O_WRONLY) && flash_open_fails) {
        errno = EACCES;
        return -1;
    }
    fd = (flags & O_CREAT) ? __real_open(path, flags, mode) : __real_open(path, flags);
    if (fd >= 0 && strcmp(path, FLASH_DEVICE) == 0 && (flags & O_WRONLY)) {
        flash_fd = fd;
    }
    if (fd >= 0 && strcmp(path, TEST_WATCHDOG) == 0) {
        watchdog_fd = fd;
    }
    return fd;
}

ssize_t __wrap_write(int fd, const void *buffer, size_t length)
{
    if (fd == flash_fd && flash_write_fails) {
        errno = EIO;
        return -1;
    }
    return __real_write(fd, buffer, length);
}

int __wrap_close(int fd)
{
    int was_flash = fd == flash_fd;
    int result = __real_close(fd);

    if (was_flash) {
        flash_fd = -1;
        if (corrupt_after_close) {
            int corrupt_fd = __real_open(FLASH_DEVICE, O_WRONLY);
            if (corrupt_fd >= 0) {
                unsigned char byte = 0xee;
                (void)lseek(corrupt_fd, 0, SEEK_SET);
                (void)__real_write(corrupt_fd, &byte, 1);
                (void)__real_close(corrupt_fd);
            }
        }
    }
    if (fd == watchdog_fd) {
        watchdog_fd = -1;
    }
    return result;
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    void *argument;
    va_list args;

    va_start(args, request);
    argument = va_arg(args, void *);
    va_end(args);
    if (fd == flash_fd && request == MEMGETINFO) {
        struct mtd_info_user_compat *mtd = argument;
        if (memgetinfo_fails) {
            errno = EIO;
            return -1;
        }
        memset(mtd, 0, sizeof(*mtd));
        mtd->size = mock_mtd_size;
        mtd->erasesize = mock_erase_size;
        return 0;
    }
    if (fd == flash_fd && request == MEMERASE) {
        erase_count++;
        if (erase_fail_at > 0 && erase_count == erase_fail_at) {
            errno = EIO;
            return -1;
        }
        return 0;
    }
    if (fd == watchdog_fd && request == WDIOC_SETOPTIONS) {
        if (watchdog_ioctl_fails) {
            errno = EIO;
            return -1;
        }
        return 0;
    }
    errno = ENOTTY;
    return -1;
}

int __wrap_umount2(const char *target, int flags)
{
    (void)target;
    (void)flags;
    if (unmount_fails) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}

int __wrap_fsync(int fd)
{
    if (fd == flash_fd && flash_fsync_errno) {
        errno = flash_fsync_errno;
        return -1;
    }
    return __real_fsync(fd);
}

int __wrap_reboot(int command)
{
    (void)command;
    reboot_count++;
    if (reboot_fails) {
        errno = EPERM;
        return -1;
    }
    return 0;
}

const SSL_METHOD *TLS_client_method(void) { return (const SSL_METHOD *)1; }
SSL_CTX *SSL_CTX_new(const SSL_METHOD *method) { (void)method; return ssl_ctx_new_fails ? NULL : (SSL_CTX *)1; }
void SSL_CTX_free(SSL_CTX *ctx) { (void)ctx; }
int SSL_CTX_set_verify(SSL_CTX *ctx, int mode, void *cb) { (void)ctx; (void)mode; (void)cb; return 1; }
int SSL_CTX_set_verify_depth(SSL_CTX *ctx, int depth) { (void)ctx; (void)depth; return 1; }
int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *file, const char *path) { (void)ctx; (void)file; (void)path; return ssl_ca_load_fails ? 0 : 1; }
SSL *SSL_new(SSL_CTX *ctx) { (void)ctx; ssl_response_len = 0; ssl_response_offset = 0; return ssl_new_fails ? NULL : (SSL *)1; }
void SSL_free(SSL *ssl) { (void)ssl; }
int SSL_set_fd(SSL *ssl, int fd) { (void)ssl; (void)fd; return 1; }
long SSL_ctrl(SSL *ssl, int cmd, long larg, void *parg) { (void)ssl; (void)cmd; (void)larg; (void)parg; return 1; }
int SSL_set1_host(SSL *ssl, const char *name) { (void)ssl; (void)name; return ssl_set1_host_fails ? 0 : 1; }
int SSL_connect(SSL *ssl) { (void)ssl; return ssl_connect_fails ? 0 : 1; }
long SSL_get_verify_result(const SSL *ssl) { (void)ssl; return ssl_verify_fails ? 7 : X509_V_OK; }
int SSL_write(SSL *ssl, const void *buf, int num) {
    const char *request = buf;
    (void)ssl;
    if (ssl_write_fails) return -1;
    if (strstr(request, "GET /redirect ")) {
        ssl_response_len = (size_t)snprintf(
            ssl_response, sizeof(ssl_response),
            "HTTP/1.0 302 Found\r\nLocation: https://127.0.0.1:%d/ok\r\n\r\n",
            ssl_server_port);
    } else if (strstr(request, "GET /loop ")) {
        ssl_response_len = (size_t)snprintf(
            ssl_response, sizeof(ssl_response),
            "HTTP/1.0 302 Found\r\nLocation: https://127.0.0.1:%d/loop\r\n\r\n",
            ssl_server_port);
    } else if (strstr(request, "GET /status ")) {
        snprintf(ssl_response, sizeof(ssl_response), "HTTP/1.0 503 Unavailable\r\n\r\n");
        ssl_response_len = strlen(ssl_response);
    } else if (strstr(request, "GET /malformed ")) {
        snprintf(ssl_response, sizeof(ssl_response), "not-http\r\n\r\n");
        ssl_response_len = strlen(ssl_response);
    } else if (strstr(request, "GET /empty ")) {
        ssl_response_len = 0;
    } else {
        snprintf(ssl_response, sizeof(ssl_response),
                 "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nhello-updater");
        ssl_response_len = strlen(ssl_response);
    }
    ssl_response_offset = 0;
    return num;
}
int SSL_read(SSL *ssl, void *buf, int num) {
    size_t remaining;
    size_t chunk;
    (void)ssl;
    if (ssl_response_offset >= ssl_response_len) return 0;
    remaining = ssl_response_len - ssl_response_offset;
    chunk = remaining < 11 ? remaining : 11;
    if (chunk > (size_t)num) chunk = (size_t)num;
    memcpy(buf, ssl_response + ssl_response_offset, chunk);
    ssl_response_offset += chunk;
    return (int)chunk;
}
void ERR_print_errors_fp(FILE *fp) { (void)fp; }

static void reset_injections(void)
{
    flash_fd = -1;
    watchdog_fd = -1;
    flash_open_fails = 0;
    memgetinfo_fails = 0;
    erase_fail_at = 0;
    erase_count = 0;
    unmount_fails = 0;
    flash_write_fails = 0;
    flash_fsync_errno = 0;
    corrupt_after_close = 0;
    reboot_fails = 0;
    reboot_count = 0;
    watchdog_ioctl_fails = 0;
    mock_mtd_size = 8192;
    mock_erase_size = 4096;
    ssl_ctx_new_fails = 0;
    ssl_ca_load_fails = 0;
    ssl_new_fails = 0;
    ssl_connect_fails = 0;
    ssl_verify_fails = 0;
    ssl_write_fails = 0;
}

static int write_pattern_file(const char *path, size_t size, unsigned char seed)
{
    FILE *output = fopen(path, "wb");
    size_t index;

    if (!output) return -1;
    for (index = 0; index < size; index++) {
        unsigned char byte = (unsigned char)(seed + index * 17U);
        if (fwrite(&byte, 1, 1, output) != 1) {
            fclose(output);
            return -1;
        }
    }
    return fclose(output);
}

static int guard_has_state(const char *state)
{
    char expected[64];
    char contents[128];
    FILE *input = fopen(WIBOX_OTA_GUARD_PATH, "r");
    size_t length;

    if (!input) return 0;
    length = fread(contents, 1, sizeof(contents) - 1, input);
    fclose(input);
    contents[length] = '\0';
    snprintf(expected, sizeof(expected), "state=%s\n", state);
    return strstr(contents, expected) != NULL;
}

static int prepare_flash(char expected_md5[33])
{
    unlink(TEST_IMAGE);
    unlink(FLASH_DEVICE);
    unlink(WIBOX_OTA_GUARD_PATH);
    if (write_pattern_file(TEST_IMAGE, 5000, 7) != 0) return -1;
    if (write_pattern_file(FLASH_DEVICE, 8192, 0) != 0) return -1;
    return read_md5sum_file(TEST_IMAGE, expected_md5);
}

static int test_parsers(void)
{
    char value[64];
    char host[64];
    char path[128];
    char hash[33];
    int port;
    text_buffer_t buffer;
    static const char md5_text[] =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  other.img\n"
        "ABCDEF0123456789ABCDEF0123456789  wibox-media-v1.2.3.img\n";

    snprintf(value, sizeof(value), "  hello world \r\n");
    trim(value);
    CHECK(strcmp(value, "hello world") == 0);
    CHECK(version_is_valid("v1.2.3"));
    CHECK(version_is_valid("wibox-media-v2.0.1"));
    CHECK(!version_is_valid("1.2"));
    CHECK(version_compare("v1.2.4", "v1.2.3") > 0);
    CHECK(version_compare("v1.1.9", "v1.2.0") < 0);
    CHECK(version_compare("v1.2.3", "v1.2.3") == 0);
    CHECK(parse_https_url("https://example.test:8443/path", host, sizeof(host),
                          &port, path, sizeof(path)) == 0);
    CHECK(strcmp(host, "example.test") == 0 && port == 8443 && strcmp(path, "/path") == 0);
    CHECK(parse_https_url("http://example.test", host, sizeof(host), &port,
                          path, sizeof(path)) == -1);
    CHECK(parse_latest_tag("{\"tag_name\":\"v1.2.3\"}", value, sizeof(value)) == 0);
    CHECK(strcmp(value, "v1.2.3") == 0);
    CHECK(parse_latest_tag("{}", value, sizeof(value)) == -1);
    CHECK(parse_md5sum(md5_text, "wibox-media-v1.2.3.img", hash) == 0);
    CHECK(strcmp(hash, "abcdef0123456789abcdef0123456789") == 0);
    CHECK(parse_md5sum(md5_text, "missing.img", hash) == -1);
    memset(&buffer, 0, sizeof(buffer));
    CHECK(mem_writer(&buffer, (const unsigned char *)"abc", 3) == 0);
    CHECK(strcmp(buffer.data, "abc") == 0);
    buffer.len = sizeof(buffer.data) - 2;
    CHECK(mem_writer(&buffer, (const unsigned char *)"xx", 2) == -1);
    return 0;
}

static int test_flash_failures(void)
{
    char expected_md5[33];
    bool touched;

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    flash_open_fails = 1;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && !touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    memgetinfo_fails = 1;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && !touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    mock_erase_size = 0;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && !touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    mock_mtd_size = 4096;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && !touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    unmount_fails = 1;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && !touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    erase_fail_at = 2;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    flash_write_fails = 1;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    flash_fsync_errno = EIO;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && touched);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    corrupt_after_close = 1;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == -1 && touched);
    CHECK(guard_has_state("VERIFYING"));
    return 0;
}

static int test_flash_success(void)
{
    char expected_md5[33];
    char actual_md5[33];
    bool touched;

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    flash_fsync_errno = EINVAL;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, false, &touched) == 0);
    CHECK(touched && erase_count == 2 && guard_has_state("COMPLETE"));
    CHECK(read_md5sum_prefix(FLASH_DEVICE, 5000, actual_md5) == 0);
    CHECK(strcasecmp(actual_md5, expected_md5) == 0);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    reboot_fails = 1;
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, true, &touched) == -1);
    CHECK(reboot_count == 1 && guard_has_state("COMPLETE"));

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    CHECK(flash_file(TEST_IMAGE, 5000, expected_md5, true, &touched) == 0);
    CHECK(reboot_count == 1);
    return 0;
}

static int test_guard_and_watchdog(void)
{
    char expected_md5[33];

    unlink(WIBOX_OTA_GUARD_PATH);
    CHECK(ota_guard_write_state("PREPARE", true) == 0);
    CHECK(guard_has_state("PREPARE"));
    CHECK(ota_guard_write_state("PREPARE", true) == -1);
    CHECK(ota_guard_write_state("FLASHING", false) == 0);
    CHECK(guard_has_state("FLASHING"));
    ota_guard_cancel();
    CHECK(access(WIBOX_OTA_GUARD_PATH, F_OK) != 0);

    unlink(TEST_WATCHDOG);
    CHECK(disarm_watchdog_for_flash(TEST_WATCHDOG) == 0);
    CHECK(write_pattern_file(TEST_WATCHDOG, 1, 0) == 0);
    CHECK(disarm_watchdog_for_flash(TEST_WATCHDOG) == 0);
    watchdog_ioctl_fails = 1;
    CHECK(disarm_watchdog_for_flash(TEST_WATCHDOG) == -1);
    watchdog_ioctl_fails = 0;

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    unmount_fails = 1;
    CHECK(guarded_flash_file(TEST_IMAGE, 5000, expected_md5, false,
                             "/tmp/no-watchdog-device") == -1);
    CHECK(access(WIBOX_OTA_GUARD_PATH, F_OK) != 0);

    reset_injections();
    CHECK(prepare_flash(expected_md5) == 0);
    erase_fail_at = 1;
    CHECK(guarded_flash_file(TEST_IMAGE, 5000, expected_md5, false,
                             "/tmp/no-watchdog-device") == -1);
    CHECK(guard_has_state("FLASHING"));
    CHECK(ota_guard_begin("/tmp/no-watchdog-device") == -1);
    ota_guard_cancel();
    return 0;
}

static int test_operator_mode(void)
{
    char *missing_md5[] = {"firmware_update", "--image", TEST_IMAGE, NULL};
    char *bad_md5[] = {"firmware_update", "--image", TEST_IMAGE,
                       "--expected-md5", "00000000000000000000000000000000", NULL};

    optind = 1;
    opterr = 0;
    CHECK(firmware_update_program_main(3, missing_md5) == 2);
    optind = 1;
    CHECK(firmware_update_program_main(5, bad_md5) == 1);
    return 0;
}

static int reject_body(void *context, const unsigned char *data, size_t len)
{
    (void)context;
    (void)data;
    (void)len;
    return -1;
}

static int start_tls_listener(void)
{
    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 64) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_len) != 0) {
        close(listener);
        return -1;
    }
    ssl_server_port = ntohs(address.sin_port);
    return listener;
}

static void local_url(char *url, size_t size, const char *path)
{
    snprintf(url, size, "https://127.0.0.1:%d/%s", ssl_server_port, path);
}

static int test_https_transport(void)
{
    char url[256];
    char final_url[256];
    text_buffer_t body;
    int listener = start_tls_listener();

    CHECK(listener >= 0);
    local_url(url, sizeof(url), "ok");
    CHECK(download_text(url, &body) == 0);
    CHECK(strcmp(body.data, "hello-updater") == 0);

    local_url(url, sizeof(url), "redirect");
    memset(&body, 0, sizeof(body));
    memset(final_url, 0, sizeof(final_url));
    CHECK(fetch_url(url, mem_writer, &body, &body,
                    final_url, sizeof(final_url), 0) == 0);
    CHECK(strcmp(body.data, "hello-updater") == 0 && strstr(final_url, "/ok"));

    local_url(url, sizeof(url), "status");
    CHECK(download_text(url, &body) == -1);
    local_url(url, sizeof(url), "malformed");
    CHECK(download_text(url, &body) == -1);
    local_url(url, sizeof(url), "empty");
    CHECK(download_text(url, &body) == -1);
    local_url(url, sizeof(url), "ok");
    CHECK(fetch_url(url, reject_body, NULL, NULL, NULL, 0, 0) == 0);
    CHECK(fetch_url(url, reject_body, &body, NULL, NULL, 0, 0) == -1);

    ssl_ctx_new_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_ctx_new_fails = 0;
    ssl_ca_load_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_ca_load_fails = 0;
    ssl_new_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_new_fails = 0;
    ssl_set1_host_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_set1_host_fails = 0;
    ssl_connect_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_connect_fails = 0;
    ssl_verify_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_verify_fails = 0;
    ssl_write_fails = 1;
    CHECK(download_text(url, &body) == -1);
    ssl_write_fails = 0;

    local_url(url, sizeof(url), "loop");
    CHECK(download_text(url, &body) == -1);
    close(listener);
    unlink("/tmp/wibox-ca-bundle.pem");
    return 0;
}

int main(void)
{
    unlink(WIBOX_OTA_GUARD_PATH);
    unlink(TEST_IMAGE);
    unlink(FLASH_DEVICE);
    unlink(TEST_WATCHDOG);
    CHECK(test_parsers() == 0);
    CHECK(test_flash_failures() == 0);
    CHECK(test_flash_success() == 0);
    CHECK(test_guard_and_watchdog() == 0);
    CHECK(test_operator_mode() == 0);
    CHECK(test_https_transport() == 0);
    unlink(WIBOX_OTA_GUARD_PATH);
    unlink(TEST_IMAGE);
    unlink(FLASH_DEVICE);
    unlink(TEST_WATCHDOG);
    printf("RESULT firmware_update_mock PASS\n");
    return 0;
}
