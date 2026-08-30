#include "hardware_watchdog.h"
#include "../watchdog_protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#ifndef WDIOC_SETOPTIONS
#define WDIOC_SETOPTIONS _IOR('W', 4, int)
#endif
#ifndef WDIOC_KEEPALIVE
#define WDIOC_KEEPALIVE _IOR('W', 5, int)
#endif
#ifndef WDIOC_SETTIMEOUT
#define WDIOC_SETTIMEOUT _IOWR('W', 6, int)
#endif
#ifndef WDIOS_DISABLECARD
#define WDIOS_DISABLECARD 0x0001
#endif
#ifndef WDIOS_ENABLECARD
#define WDIOS_ENABLECARD 0x0002
#endif

typedef struct {
    pthread_mutex_t mutex;
    pthread_t thread;
    int fd;
    int running;
    int thread_started;
    int timeout_seconds;
    int feed_interval_seconds;
    unsigned long long last_main_heartbeat_ms;
} hardware_watchdog_state_t;

static hardware_watchdog_state_t watchdog_state = {
    PTHREAD_MUTEX_INITIALIZER, 0, -1, 0, 0, 0, 0, 0
};

static unsigned long long watchdog_now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static int watchdog_keepalive(int fd) {
    int ignored = 0;

    if (ioctl(fd, WDIOC_KEEPALIVE, &ignored) == 0) {
        return 0;
    }

    if (write(fd, "\0", 1) == 1) {
        return 0;
    }

    return -1;
}

static void watchdog_disarm_and_close(int fd, int disarm) {
    int options = WDIOS_DISABLECARD;

    if (fd < 0) {
        return;
    }

    if (disarm) {
        if (ioctl(fd, WDIOC_SETOPTIONS, &options) == 0) {
            printf("Hardware watchdog disarmed\n");
        } else if (write(fd, "V", 1) == 1) {
            printf("Hardware watchdog magic-close requested\n");
        } else {
            printf("WARNING: failed to disarm hardware watchdog: %s\n",
                   strerror(errno));
        }
    }

    close(fd);
}

static void* hardware_watchdog_thread(void* arg) {
    unsigned long long last_feed_ms = watchdog_now_ms();
    int stale_reported = 0;

    (void)arg;

    for (;;) {
        unsigned long long now;
        unsigned long long last_heartbeat;
        unsigned long long stale_after_ms;
        unsigned long long feed_interval_ms;
        int running;
        int fd;
        int timeout_seconds;
        int feed_interval_seconds;

        pthread_mutex_lock(&watchdog_state.mutex);
        running = watchdog_state.running;
        fd = watchdog_state.fd;
        timeout_seconds = watchdog_state.timeout_seconds;
        feed_interval_seconds = watchdog_state.feed_interval_seconds;
        last_heartbeat = watchdog_state.last_main_heartbeat_ms;
        pthread_mutex_unlock(&watchdog_state.mutex);

        if (!running) {
            break;
        }

        now = watchdog_now_ms();
        stale_after_ms = (unsigned long long)timeout_seconds * 500ULL;
        feed_interval_ms = (unsigned long long)feed_interval_seconds * 1000ULL;

        if (now > last_heartbeat && now - last_heartbeat >= stale_after_ms) {
            if (!stale_reported) {
                printf("ERROR: main loop heartbeat stale for %llu ms; "
                       "hardware watchdog feeding suspended\n",
                       now - last_heartbeat);
                stale_reported = 1;
            }
        } else {
            stale_reported = 0;
            if (now - last_feed_ms >= feed_interval_ms) {
                if (watchdog_keepalive(fd) != 0) {
                    printf("ERROR: hardware watchdog keepalive failed: %s\n",
                           strerror(errno));
                } else {
                    last_feed_ms = now;
                }
            }
        }

        usleep(100000);
    }

    return NULL;
}

int hardware_watchdog_start(int enabled,
                            const char* device,
                            int timeout_seconds,
                            int feed_interval_seconds) {
    int actual_timeout;
    int options = WDIOS_ENABLECARD;
    int fd;
    int flags;
    int rc;

    if (!enabled) {
        return 0;
    }

    if (access(WIBOX_OTA_GUARD_PATH, F_OK) == 0) {
        printf("Hardware watchdog intentionally not armed while OTA guard is active\n");
        return 0;
    }

    // Upper bounds guard the feed_interval_seconds * 2 comparison against signed
    // integer overflow (a huge configured value would otherwise wrap negative and
    // slip past the sanity check, leaving the watchdog effectively unfed).
    if (!device || !device[0] || timeout_seconds < 5 || timeout_seconds > 3600 ||
        feed_interval_seconds < 1 || feed_interval_seconds > 3600 ||
        feed_interval_seconds * 2 >= timeout_seconds) {
        printf("ERROR: invalid hardware watchdog configuration\n");
        return -1;
    }

    pthread_mutex_lock(&watchdog_state.mutex);
    if (watchdog_state.running || watchdog_state.fd >= 0) {
        pthread_mutex_unlock(&watchdog_state.mutex);
        return 0;
    }
    pthread_mutex_unlock(&watchdog_state.mutex);

    fd = open(device, O_RDWR);
    if (fd < 0) {
        printf("ERROR: failed to open hardware watchdog %s: %s\n",
               device, strerror(errno));
        return -1;
    }

    flags = fcntl(fd, F_GETFD);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }

    actual_timeout = timeout_seconds;
    if (ioctl(fd, WDIOC_SETTIMEOUT, &actual_timeout) != 0) {
        printf("ERROR: failed to set hardware watchdog timeout: %s\n",
               strerror(errno));
        watchdog_disarm_and_close(fd, 1);
        return -1;
    }

    if (actual_timeout <= feed_interval_seconds * 2) {
        printf("ERROR: hardware watchdog timeout %d is too short for feed "
               "interval %d\n", actual_timeout, feed_interval_seconds);
        watchdog_disarm_and_close(fd, 1);
        return -1;
    }

    if (ioctl(fd, WDIOC_SETOPTIONS, &options) != 0 &&
        errno != EINVAL && errno != ENOTTY) {
        printf("ERROR: failed to enable hardware watchdog: %s\n",
               strerror(errno));
        watchdog_disarm_and_close(fd, 1);
        return -1;
    }

    if (watchdog_keepalive(fd) != 0) {
        printf("ERROR: initial hardware watchdog keepalive failed: %s\n",
               strerror(errno));
        watchdog_disarm_and_close(fd, 1);
        return -1;
    }

    pthread_mutex_lock(&watchdog_state.mutex);
    watchdog_state.fd = fd;
    watchdog_state.running = 1;
    watchdog_state.timeout_seconds = actual_timeout;
    watchdog_state.feed_interval_seconds = feed_interval_seconds;
    watchdog_state.last_main_heartbeat_ms = watchdog_now_ms();
    pthread_mutex_unlock(&watchdog_state.mutex);

    rc = pthread_create(&watchdog_state.thread, NULL,
                        hardware_watchdog_thread, NULL);
    if (rc != 0) {
        printf("ERROR: failed to start hardware watchdog thread: %s\n",
               strerror(rc));
        pthread_mutex_lock(&watchdog_state.mutex);
        watchdog_state.fd = -1;
        watchdog_state.running = 0;
        pthread_mutex_unlock(&watchdog_state.mutex);
        watchdog_disarm_and_close(fd, 1);
        return -1;
    }

    pthread_mutex_lock(&watchdog_state.mutex);
    watchdog_state.thread_started = 1;
    pthread_mutex_unlock(&watchdog_state.mutex);

    printf("Hardware watchdog active: device=%s timeout=%ds feed=%ds "
           "main-loop-stale=%ds\n", device, actual_timeout,
           feed_interval_seconds, actual_timeout / 2);
    return 0;
}

void hardware_watchdog_heartbeat(void) {
    pthread_mutex_lock(&watchdog_state.mutex);
    if (watchdog_state.running) {
        watchdog_state.last_main_heartbeat_ms = watchdog_now_ms();
    }
    pthread_mutex_unlock(&watchdog_state.mutex);
}

void hardware_watchdog_stop(int disarm) {
    pthread_t thread;
    int thread_started;
    int fd;

    pthread_mutex_lock(&watchdog_state.mutex);
    watchdog_state.running = 0;
    thread = watchdog_state.thread;
    thread_started = watchdog_state.thread_started;
    pthread_mutex_unlock(&watchdog_state.mutex);

    if (thread_started) {
        pthread_join(thread, NULL);
    }

    pthread_mutex_lock(&watchdog_state.mutex);
    fd = watchdog_state.fd;
    watchdog_state.fd = -1;
    watchdog_state.thread_started = 0;
    watchdog_state.timeout_seconds = 0;
    watchdog_state.feed_interval_seconds = 0;
    watchdog_state.last_main_heartbeat_ms = 0;
    pthread_mutex_unlock(&watchdog_state.mutex);

    watchdog_disarm_and_close(fd, disarm);
}
