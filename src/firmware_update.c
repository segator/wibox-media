#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef WIBOX_VERSION
#define WIBOX_VERSION "dev-unknown"
#endif

#define DEFAULT_REPO "segator/wibox-media"
#define DEFAULT_CONF "/mnt/mtd/sip_media.conf"
#define DEFAULT_CONF_FALLBACK "/etc/sip_media.conf.default"
#define DEFAULT_UPDATE_FILE "/tmp/update.img"
#define DEFAULT_RELEASE_LOG "/tmp/firmware_update.log"
#define FLASH_DEVICE "/dev/mtd4"
#define FLASH_VERIFY_DEVICE "/dev/mtd4"
#define FLASH_MOUNTPOINT "/usr"
#define FLASH_BLOCK 4096
#define MAX_IMAGE_SIZE 11534336
#define MAX_REDIRECTS 5
#define LEGACY_REPO "aymerici/wibox-media"

struct mtd_info_user_compat {
    uint8_t type;
    uint32_t flags;
    uint32_t size;
    uint32_t erasesize;
    uint32_t writesize;
    uint32_t oobsize;
    uint64_t padding;
};

struct erase_info_user_compat {
    uint32_t start;
    uint32_t length;
};

#ifndef MEMGETINFO
#define MEMGETINFO _IOR('M', 1, struct mtd_info_user_compat)
#endif
#ifndef MEMERASE
#define MEMERASE _IOW('M', 2, struct erase_info_user_compat)
#endif

#define SSL_VERIFY_PEER 0x01
#define X509_V_OK 0L

typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct ssl_method_st SSL_METHOD;

extern const SSL_METHOD *TLS_client_method(void);
extern SSL_CTX *SSL_CTX_new(const SSL_METHOD *meth);
extern void SSL_CTX_free(SSL_CTX *ctx);
extern int SSL_CTX_set_verify(SSL_CTX *ctx, int mode, void *callback);
extern int SSL_CTX_set_verify_depth(SSL_CTX *ctx, int depth);
extern int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile, const char *CApath);
extern SSL *SSL_new(SSL_CTX *ctx);
extern void SSL_free(SSL *ssl);
extern int SSL_set_fd(SSL *ssl, int fd);
extern long SSL_ctrl(SSL *ssl, int cmd, long larg, void *parg);
extern int SSL_set1_host(SSL *ssl, const char *name);
extern int SSL_connect(SSL *ssl);
extern long SSL_get_verify_result(const SSL *ssl);
extern int SSL_write(SSL *ssl, const void *buf, int num);
extern int SSL_read(SSL *ssl, void *buf, int num);
extern void ERR_print_errors_fp(FILE *fp);

#define SSL_CTRL_SET_TLSEXT_HOSTNAME 55
#define TLSEXT_NAMETYPE_host_name 0

static const char *CA_USERTRUST_ECC =
"-----BEGIN CERTIFICATE-----\n"
"MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL\n"
"MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n"
"eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n"
"JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx\n"
"MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT\n"
"Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg\n"
"VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm\n"
"aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo\n"
"I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng\n"
"o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G\n"
"A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD\n"
"VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB\n"
"zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW\n"
"RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=\n"
"-----END CERTIFICATE-----\n";

static const char *CA_ROOT_YR =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

typedef struct {
    char data[65536];
    size_t len;
} text_buffer_t;

typedef int (*body_writer_fn)(void *ctx, const unsigned char *data, size_t len);

static void log_line(FILE *fp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp ? fp : stderr, fmt, ap);
    fputc('\n', fp ? fp : stderr);
    fflush(fp ? fp : stderr);
    va_end(ap);
}

static const char *basename_const(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

static void trim(char *s) {
    size_t len;
    char *p;

    if (!s) return;
    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || isspace((unsigned char)s[len - 1]))) {
        s[--len] = '\0';
    }
    p = s;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
}

static int read_cfg_value(const char *path, const char *key, char *out, size_t out_size) {
    FILE *fp;
    char line[256];

    if (!path || !key || !out || out_size == 0) return -1;
    fp = fopen(path, "r");
    if (!fp) return -1;
    while (fgets(line, sizeof(line), fp)) {
        char *eq;
        trim(line);
        if (!line[0] || line[0] == '#') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) != 0) continue;
        strncpy(out, eq + 1, out_size - 1);
        out[out_size - 1] = '\0';
        trim(out);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return -1;
}

static int read_release_version(const char *path, char *out, size_t out_size) {
    FILE *fp;
    char line[256];

    if (!path || !out || out_size == 0) return -1;
    fp = fopen(path, "r");
    if (!fp) return -1;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "WIBOX_VERSION=", 14) == 0) {
            strncpy(out, line + 14, out_size - 1);
            out[out_size - 1] = '\0';
            trim(out);
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

static const char *normalize_version(const char *version) {
    if (!version) return "";
    if (strncmp(version, "wibox-media-", 12) == 0) return version + 12;
    return version;
}

static int parse_version_triplet(const char *version, int *major, int *minor, int *patch) {
    const char *v = normalize_version(version);
    char *end = NULL;

    if (!v || !major || !minor || !patch) return -1;
    if (*v == 'v' || *v == 'V') v++;
    *major = (int)strtol(v, &end, 10);
    if (end == v || *end != '.') return -1;
    v = end + 1;
    *minor = (int)strtol(v, &end, 10);
    if (end == v || *end != '.') return -1;
    v = end + 1;
    *patch = (int)strtol(v, &end, 10);
    if (end == v) return -1;
    return 0;
}

static int version_is_valid(const char *version) {
    int major, minor, patch;
    return parse_version_triplet(version, &major, &minor, &patch) == 0;
}

static int version_compare(const char *a, const char *b) {
    int am, an, ap, bm, bn, bp;
    if (parse_version_triplet(a, &am, &an, &ap) != 0) return 0;
    if (parse_version_triplet(b, &bm, &bn, &bp) != 0) return 1;
    if (am != bm) return am < bm ? -1 : 1;
    if (an != bn) return an < bn ? -1 : 1;
    if (ap != bp) return ap < bp ? -1 : 1;
    return 0;
}

static int mem_writer(void *ctx, const unsigned char *data, size_t len) {
    text_buffer_t *buf = (text_buffer_t *)ctx;
    if (!buf || len == 0) return 0;
    if (buf->len + len >= sizeof(buf->data)) return -1;
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return 0;
}

static int file_writer(void *ctx, const unsigned char *data, size_t len) {
    FILE *fp = (FILE *)ctx;
    if (!fp || len == 0) return 0;
    return fwrite(data, 1, len, fp) == len ? 0 : -1;
}

static int run_capture_first_token(const char *cmd, char *out, size_t out_size) {
    FILE *fp;
    char line[256];
    char *tok;

    if (!cmd || !out || out_size == 0) return -1;
    fp = popen(cmd, "r");
    if (!fp) return -1;
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    trim(line);
    tok = strtok(line, " \t");
    if (!tok) return -1;
    strncpy(out, tok, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
}

static int read_md5sum_file(const char *path, char out_hex[33]) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "md5sum '%s'", path);
    return run_capture_first_token(cmd, out_hex, 33);
}

static int read_md5sum_prefix(const char *path, size_t bytes, char out_hex[33]) {
    char cmd[512];
    if (bytes % FLASH_BLOCK == 0) {
        snprintf(cmd, sizeof(cmd), "dd if='%s' bs=%d count=%zu 2>/dev/null | md5sum -",
                 path, FLASH_BLOCK, bytes / FLASH_BLOCK);
    } else {
        snprintf(cmd, sizeof(cmd), "dd if='%s' bs=1 count=%zu 2>/dev/null | md5sum -", path, bytes);
    }
    return run_capture_first_token(cmd, out_hex, 33);
}

static int tcp_connect(const char *host, int port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai;
    char portbuf[16];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    snprintf(portbuf, sizeof(portbuf), "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &res) != 0) {
        return -1;
    }
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static int load_ca_bundle(SSL_CTX *ctx) {
    const char *bundle_path = "/tmp/wibox-ca-bundle.pem";
    FILE *fp;

    if (!ctx) return -1;
    fp = fopen(bundle_path, "w");
    if (!fp) return -1;
    if (fputs(CA_USERTRUST_ECC, fp) < 0 || fputs(CA_ROOT_YR, fp) < 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return SSL_CTX_load_verify_locations(ctx, bundle_path, NULL) == 1 ? 0 : -1;
}

static int parse_https_url(const char *url, char *host, size_t host_size, int *port, char *path, size_t path_size) {
    const char *p;
    const char *slash;
    const char *colon;
    size_t host_len;

    if (!url || strncmp(url, "https://", 8) != 0) return -1;
    p = url + 8;
    slash = strchr(p, '/');
    colon = strchr(p, ':');
    if (!slash) slash = url + strlen(url);
    if (colon && colon < slash) {
        host_len = (size_t)(colon - p);
        *port = atoi(colon + 1);
    } else {
        host_len = (size_t)(slash - p);
        *port = 443;
    }
    if (host_len == 0 || host_len >= host_size) return -1;
    memcpy(host, p, host_len);
    host[host_len] = '\0';
    if (*slash) {
        snprintf(path, path_size, "%s", slash);
    } else {
        snprintf(path, path_size, "/");
    }
    return 0;
}

static const char *header_value(const char *headers, const char *key) {
    const char *p = headers;
    size_t key_len = strlen(key);
    while ((p = strcasestr(p, key)) != NULL) {
        if ((p == headers || p[-1] == '\n') && strncasecmp(p, key, key_len) == 0 && p[key_len] == ':') {
            p += key_len + 1;
            while (*p == ' ' || *p == '\t') p++;
            return p;
        }
        p += key_len;
    }
    return NULL;
}

static int fetch_url(const char *url, body_writer_fn writer, void *writer_ctx,
                     text_buffer_t *response_text, char *final_url, size_t final_url_size,
                     int depth);

static int fetch_once(const char *url, body_writer_fn writer, void *writer_ctx,
                      text_buffer_t *response_text, char *redirect_url, size_t redirect_size) {
    char host[256];
    char path[1024];
    char request[1400];
    int port;
    int fd;
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    unsigned char buf[2048];
    size_t header_len = 0;
    size_t body_len = 0;
    unsigned char *body_start = NULL;
    char headers[8192];
    int status = 0;

    (void)response_text;
    if (parse_https_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        log_line(stderr, "[!] bad url: %s", url);
        return -1;
    }
    fd = tcp_connect(host, port);
    if (fd < 0) {
        log_line(stderr, "[!] tcp connect failed: %s", host);
        return -1;
    }

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        log_line(stderr, "[!] SSL_CTX_new failed");
        close(fd);
        return -1;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_verify_depth(ctx, 6);
    if (load_ca_bundle(ctx) != 0) {
        log_line(stderr, "[!] failed to load CA bundle");
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
        log_line(stderr, "[!] SSL_new failed");
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }
    SSL_set_fd(ssl, fd);
    SSL_ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (void *)host);
    if (SSL_set1_host(ssl, host) != 1) {
        log_line(stderr, "[!] SSL_set1_host failed for %s", host);
    }
    if (SSL_connect(ssl) != 1) {
        log_line(stderr, "[!] SSL_connect failed for %s", host);
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        log_line(stderr, "[!] TLS verify failed for %s", host);
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }

    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: wibox-media-update/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n",
             path, host);
    if (SSL_write(ssl, request, (int)strlen(request)) <= 0) {
        log_line(stderr, "[!] SSL_write failed for %s", host);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }

    headers[0] = '\0';
    while (header_len + sizeof(buf) < sizeof(headers)) {
        int n = SSL_read(ssl, buf, sizeof(buf));
        unsigned char *end;
        if (n <= 0) break;
        memcpy(headers + header_len, buf, (size_t)n);
        header_len += (size_t)n;
        headers[header_len] = '\0';
        end = (unsigned char *)strstr(headers, "\r\n\r\n");
        if (end) {
            size_t head_only = (size_t)(end - (unsigned char *)headers) + 4;
            size_t header_text_len = head_only >= 4 ? head_only - 4 : 0;
            body_len = header_len - head_only;
            body_start = (unsigned char *)headers + head_only;
            header_len = header_text_len;
            headers[header_len] = '\0';
            break;
        }
    }
    if (header_len == 0) {
        log_line(stderr, "[!] no response from %s", host);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }
    if (sscanf(headers, "HTTP/%*d.%*d %d", &status) != 1) {
        log_line(stderr, "[!] malformed HTTP response from %s", host);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }

    if (status >= 300 && status < 400) {
        const char *loc = header_value(headers, "Location");
        if (loc) {
            char tmp[1024];
            size_t len = strcspn(loc, "\r\n");
            if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
            memcpy(tmp, loc, len);
            tmp[len] = '\0';
            strncpy(redirect_url, tmp, redirect_size - 1);
            redirect_url[redirect_size - 1] = '\0';
        }
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return status;
    }

    if (status != 200) {
        log_line(stderr, "[!] HTTP status %d from %s", status, host);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return -1;
    }

    if (body_len > 0 && body_start) {
        if (writer && writer_ctx && writer(writer_ctx, body_start, body_len) != 0) {
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(fd);
            return -1;
        }
    }

    for (;;) {
        int n = SSL_read(ssl, buf, sizeof(buf));
        if (n <= 0) break;
        if (writer && writer_ctx && writer(writer_ctx, buf, (size_t)n) != 0) {
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(fd);
            return -1;
        }
    }

    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);
    return 0;
}

static int fetch_url(const char *url, body_writer_fn writer, void *writer_ctx,
                     text_buffer_t *response_text, char *final_url, size_t final_url_size,
                     int depth) {
    char redirect[1024];
    int rc;

    (void)response_text;
    if (depth > MAX_REDIRECTS) return -1;
    redirect[0] = '\0';
    rc = fetch_once(url, writer, writer_ctx, response_text, redirect, sizeof(redirect));
    if (rc >= 300 && rc < 400 && redirect[0]) {
        if (final_url && final_url_size > 0) {
            strncpy(final_url, redirect, final_url_size - 1);
            final_url[final_url_size - 1] = '\0';
        }
        return fetch_url(redirect, writer, writer_ctx, response_text, final_url, final_url_size, depth + 1);
    }
    return rc;
}

static int download_text(const char *url, text_buffer_t *out) {
    out->len = 0;
    out->data[0] = '\0';
    return fetch_url(url, mem_writer, out, out, NULL, 0, 0);
}

static int parse_latest_tag(const char *json, char *tag, size_t tag_size) {
    const char *p = strstr(json, "\"tag_name\":\"");
    const char *end;
    size_t len;
    if (!p) return -1;
    p += strlen("\"tag_name\":\"");
    end = strchr(p, '"');
    if (!end) return -1;
    len = (size_t)(end - p);
    if (len >= tag_size) len = tag_size - 1;
    memcpy(tag, p, len);
    tag[len] = '\0';
    return 0;
}

static int parse_md5sum(const char *text, const char *asset_name, char md5_hex[33]) {
    const char *p = text;
    char line[256];

    while (*p) {
        size_t len = strcspn(p, "\r\n");
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        if (strlen(line) >= 34 && strstr(line, asset_name)) {
            char hash[33];
            memcpy(hash, line, 32);
            hash[32] = '\0';
            if (strspn(hash, "0123456789abcdefABCDEF") == 32) {
                for (int i = 0; i < 32; ++i) {
                    md5_hex[i] = (char)tolower((unsigned char)hash[i]);
                }
                md5_hex[32] = '\0';
                return 0;
            }
        }
        p += len;
        while (*p == '\r' || *p == '\n') p++;
    }
    return -1;
}

static int get_local_version(char *out, size_t out_size) {
    if (read_release_version("/etc/wibox-release", out, out_size) == 0) return 0;
    if (read_release_version("/mnt/mtd/wibox-release", out, out_size) == 0) return 0;
    strncpy(out, WIBOX_VERSION, out_size - 1);
    out[out_size - 1] = '\0';
    trim(out);
    return 0;
}

static int flash_file(const char *image_path, size_t image_size, const char *expected_md5, bool reboot_after) {
    int image_fd = -1;
    int flash_fd = -1;
    unsigned char buf[FLASH_BLOCK];
    ssize_t n;
    char flash_md5[33];
    struct mtd_info_user_compat mtd;
    uint32_t erase_len;
    uint32_t erased;

    if (umount2(FLASH_MOUNTPOINT, MNT_DETACH) != 0) {
        if (umount2("/dev/mtdblock4", MNT_DETACH) != 0) {
            log_line(stderr, "[!] unable to unmount %s: %s", FLASH_MOUNTPOINT, strerror(errno));
            return -1;
        }
    }

    image_fd = open(image_path, O_RDONLY);
    if (image_fd < 0) {
        log_line(stderr, "[!] open(%s): %s", image_path, strerror(errno));
        return -1;
    }
    flash_fd = open(FLASH_DEVICE, O_WRONLY);
    if (flash_fd < 0) {
        log_line(stderr, "[!] open(%s): %s", FLASH_DEVICE, strerror(errno));
        close(image_fd);
        return -1;
    }

    memset(&mtd, 0, sizeof(mtd));
    if (ioctl(flash_fd, MEMGETINFO, &mtd) != 0) {
        log_line(stderr, "[!] MEMGETINFO(%s): %s", FLASH_DEVICE, strerror(errno));
        close(image_fd);
        close(flash_fd);
        return -1;
    }
    if (mtd.erasesize == 0 || image_size > mtd.size) {
        log_line(stderr, "[!] invalid MTD geometry: size=%u erasesize=%u image=%zu",
                 mtd.size, mtd.erasesize, image_size);
        close(image_fd);
        close(flash_fd);
        return -1;
    }
    erase_len = (uint32_t)(((image_size + mtd.erasesize - 1) / mtd.erasesize) * mtd.erasesize);
    log_line(stderr, "[*] erasing %u bytes on %s (mtd size=%u erasesize=%u)",
             erase_len, FLASH_DEVICE, mtd.size, mtd.erasesize);
    for (erased = 0; erased < erase_len; erased += mtd.erasesize) {
        struct erase_info_user_compat erase;
        erase.start = erased;
        erase.length = mtd.erasesize;
        if (ioctl(flash_fd, MEMERASE, &erase) != 0) {
            log_line(stderr, "[!] MEMERASE(%s start=%u len=%u): %s",
                     FLASH_DEVICE, erase.start, erase.length, strerror(errno));
            close(image_fd);
            close(flash_fd);
            return -1;
        }
    }
    if (lseek(flash_fd, 0, SEEK_SET) < 0) {
        log_line(stderr, "[!] lseek(%s): %s", FLASH_DEVICE, strerror(errno));
        close(image_fd);
        close(flash_fd);
        return -1;
    }
    log_line(stderr, "[*] writing %zu bytes to %s", image_size, FLASH_DEVICE);

    while ((n = read(image_fd, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(flash_fd, buf + off, (size_t)(n - off));
            if (w < 0) {
                log_line(stderr, "[!] write(%s): %s", FLASH_DEVICE, strerror(errno));
                close(image_fd);
                close(flash_fd);
                return -1;
            }
            off += w;
        }
    }
    if (n < 0) {
        log_line(stderr, "[!] read(%s): %s", image_path, strerror(errno));
        close(image_fd);
        close(flash_fd);
        return -1;
    }
    sync();
    if (fsync(flash_fd) != 0) {
        log_line(stderr, "[!] fsync(%s): %s", FLASH_DEVICE, strerror(errno));
    }
    close(image_fd);
    close(flash_fd);

    if (read_md5sum_prefix(FLASH_VERIFY_DEVICE, image_size, flash_md5) != 0) {
        log_line(stderr, "[!] unable to hash flashed image");
        return -1;
    }
    if (strcasecmp(flash_md5, expected_md5) != 0) {
        log_line(stderr, "[!] flash verification failed: expected=%s got=%s", expected_md5, flash_md5);
        return -1;
    }
    log_line(stderr, "[*] flash verification OK (%s)", flash_md5);

    if (reboot_after) {
        log_line(stderr, "[*] rebooting");
        sync();
        reboot(RB_AUTOBOOT);
    }
    return 0;
}

static int fetch_latest_release(const char *repo, char *tag, size_t tag_size) {
    char api_url[512];
    text_buffer_t body;
    snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases/latest", repo);
    if (download_text(api_url, &body) != 0) {
        log_line(stderr, "[!] API download failed: %s", api_url);
        return -1;
    }
    log_line(stderr, "[*] API response size=%zu", body.len);
    if (body.len > 0) {
        size_t preview = body.len < 120 ? body.len : 120;
        char tmp[128];
        memcpy(tmp, body.data, preview);
        tmp[preview] = '\0';
        log_line(stderr, "[*] API preview: %s", tmp);
    }
    return parse_latest_tag(body.data, tag, tag_size);
}

int main(int argc, char **argv) {
    const char *conf_file = DEFAULT_CONF;
    const char *update_file = DEFAULT_UPDATE_FILE;
    const char *install_image = NULL;
    const char *install_expected_md5 = NULL;
    const char *repo = DEFAULT_REPO;
    bool force = false;
    bool reboot_after = true;
    bool status_only = false;
    char conf_repo[128];
    char conf_enabled[16];
    char local_version[64];
    char remote_version[64];
    char image_name[128];
    char image_url[512];
    char md5s_url[512];
    char expected_md5[33];
    size_t downloaded_size = 0;
    int opt;

    static const struct option long_opts[] = {
        { "config", required_argument, NULL, 'c' },
        { "repo", required_argument, NULL, 'r' },
        { "force", no_argument, NULL, 'f' },
        { "no-reboot", no_argument, NULL, 'n' },
        { "output", required_argument, NULL, 'o' },
        { "image", required_argument, NULL, 'i' },
        { "expected-md5", required_argument, NULL, 'm' },
        { "status", no_argument, NULL, 's' },
        { 0, 0, 0, 0 }
    };

    while ((opt = getopt_long(argc, argv, "c:r:fno:i:m:s", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'c':
            conf_file = optarg;
            break;
        case 'r':
            repo = optarg;
            break;
        case 'f':
            force = true;
            break;
        case 'n':
            reboot_after = false;
            break;
        case 'o':
            update_file = optarg;
            break;
        case 'i':
            install_image = optarg;
            break;
        case 'm':
            install_expected_md5 = optarg;
            break;
        case 's':
            status_only = true;
            break;
        default:
            fprintf(stderr, "usage: %s [--config FILE] [--repo OWNER/REPO] [--force] [--no-reboot] [--output FILE] [--image FILE --expected-md5 MD5] [--status]\n", basename_const(argv[0]));
            return 2;
        }
    }

    if (install_image) {
        struct stat st;
        char local_md5[33];

        if (!install_expected_md5 || strlen(install_expected_md5) != 32) {
            log_line(stderr, "[!] --image requires --expected-md5 MD5");
            return 2;
        }
        if (stat(install_image, &st) != 0) {
            log_line(stderr, "[!] stat(%s): %s", install_image, strerror(errno));
            return 1;
        }
        if (st.st_size <= 0 || (size_t)st.st_size > MAX_IMAGE_SIZE) {
            log_line(stderr, "[!] invalid image size: %ld", (long)st.st_size);
            return 1;
        }
        if (read_md5sum_file(install_image, local_md5) != 0 ||
            strcasecmp(local_md5, install_expected_md5) != 0) {
            log_line(stderr, "[!] local image checksum mismatch: expected=%s got=%s",
                     install_expected_md5, local_md5);
            return 1;
        }
        return flash_file(install_image, (size_t)st.st_size, install_expected_md5, reboot_after) == 0 ? 0 : 1;
    }

    conf_enabled[0] = '\0';
    conf_repo[0] = '\0';
    if (read_cfg_value(conf_file, "firmware_update_enabled", conf_enabled, sizeof(conf_enabled)) != 0 &&
        read_cfg_value(DEFAULT_CONF_FALLBACK, "firmware_update_enabled", conf_enabled, sizeof(conf_enabled)) != 0) {
        snprintf(conf_enabled, sizeof(conf_enabled), "1");
    }
    if (strcmp(conf_enabled, "0") == 0) {
        log_line(stderr, "[*] firmware updates disabled in config");
        return 0;
    }
    if (read_cfg_value(conf_file, "firmware_update_repo", conf_repo, sizeof(conf_repo)) == 0 && conf_repo[0]) {
        repo = conf_repo;
    } else if (read_cfg_value(DEFAULT_CONF_FALLBACK, "firmware_update_repo", conf_repo, sizeof(conf_repo)) == 0 && conf_repo[0]) {
        repo = conf_repo;
    }
    if (strcmp(repo, LEGACY_REPO) == 0) {
        repo = DEFAULT_REPO;
    }

    if (fetch_latest_release(repo, remote_version, sizeof(remote_version)) != 0) {
        log_line(stderr, "[!] unable to query latest release for %s", repo);
        return 1;
    }
    if (!version_is_valid(remote_version)) {
        log_line(stderr, "[!] latest release tag is not a supported version string: %s", remote_version);
        return 1;
    }

    get_local_version(local_version, sizeof(local_version));
    snprintf(image_name, sizeof(image_name), "wibox-media-%s.img", remote_version);
    snprintf(image_url, sizeof(image_url), "https://github.com/%s/releases/download/%s/%s", repo, remote_version, image_name);
    snprintf(md5s_url, sizeof(md5s_url), "https://github.com/%s/releases/download/%s/MD5SUMS", repo, remote_version);

    if (status_only) {
        int available = 1;
        if (version_is_valid(local_version) && version_compare(remote_version, local_version) <= 0) {
            available = 0;
        }
        printf("repo=%s\n", repo);
        printf("local_version=%s\n", local_version);
        printf("remote_version=%s\n", remote_version);
        printf("available=%d\n", available);
        printf("image_url=%s\n", image_url);
        return 0;
    }

    if (!force && version_is_valid(local_version) && version_compare(remote_version, local_version) <= 0) {
        log_line(stderr, "[*] already up to date: local=%s remote=%s", local_version, remote_version);
        return 0;
    }

    {
        text_buffer_t md5s;
        if (download_text(md5s_url, &md5s) != 0) {
            log_line(stderr, "[!] unable to download MD5SUMS from %s", md5s_url);
            return 1;
        }
        if (parse_md5sum(md5s.data, image_name, expected_md5) != 0) {
            log_line(stderr, "[!] unable to find checksum for %s", image_name);
            return 1;
        }
    }

    unlink(update_file);

    log_line(stderr, "[*] repo=%s local=%s remote=%s", repo, local_version, remote_version);
    log_line(stderr, "[*] downloading %s", image_url);

    {
        FILE *fp;
        int rc;
        struct stat st;
        char downloaded_md5[33];

        fp = fopen(update_file, "wb");
        if (!fp) {
            log_line(stderr, "[!] open(%s): %s", update_file, strerror(errno));
            return 1;
        }
        rc = fetch_url(image_url, file_writer, fp, NULL, NULL, 0, 0);
        fclose(fp);
        if (rc != 0) {
            unlink(update_file);
            log_line(stderr, "[!] image download failed");
            return 1;
        }
        if (read_md5sum_file(update_file, downloaded_md5) != 0) {
            unlink(update_file);
            log_line(stderr, "[!] unable to hash downloaded file");
            return 1;
        }
        if (strcasecmp(downloaded_md5, expected_md5) != 0) {
            unlink(update_file);
            log_line(stderr, "[!] checksum mismatch: expected=%s got=%s", expected_md5, downloaded_md5);
            return 1;
        }
        if (stat(update_file, &st) == 0) {
            downloaded_size = (size_t)st.st_size;
        }
        if (downloaded_size == 0 || downloaded_size > MAX_IMAGE_SIZE) {
            unlink(update_file);
            log_line(stderr, "[!] downloaded image size is invalid: %zu", downloaded_size);
            return 1;
        }
        log_line(stderr, "[*] download verified md5=%s bytes=%zu", downloaded_md5, downloaded_size);
    }

    if (flash_file(update_file, downloaded_size, expected_md5, reboot_after) != 0) {
        log_line(stderr, "[!] flashing failed");
        return 1;
    }

    return 0;
}
