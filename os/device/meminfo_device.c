#include <proc/proc.h>
#include <ucore/defs.h>
#include "meminfo_device.h"
#include <mem/string.h>

void meminfo_device_init() {
    device_handler[MEMINFO_DEVICE].read = meminfo_read;
    device_handler[MEMINFO_DEVICE].write = meminfo_write;
}

int64 meminfo_write(char *src, int64 len, int from_user) {
    return 0;
}

static void append_info(char *buf, char *title, int value, char *unit) {
    strcat(buf, title);
    strcat(buf, ": ");
    char svalue[16];
    itoa(value, svalue, 10);
    strcat(buf, svalue);
    strcat(buf, " ");
    strcat(buf, unit);
    strcat(buf, "\n");
}

int64 meminfo_read(char *dst, int64 len, int to_user) {
    char buf[1024];
    append_info(buf, "MemAvailable", get_free_page_count() * 4, "kB");
    infof("meminfo: %s", buf);
    return either_copyout(dst, buf, strlen(buf), to_user);
}
