#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/types.h>
#include <net/if.h>
#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <sys/mount.h>
#include <pthread.h>
#include <signal.h>
#include <sys/timerfd.h>

#include "lvgl/lvgl.h"

#include "vx_config.h"
#include "vx_models.h"
#include "vx_network.h"
#include "vx_stats.h"
#include "vx_utils.h"
#include "vx_view.h"

// Global objects
Selector selector;
pthread_mutex_t main_mutex;
InterfaceCollection* interface_collection;

void setup_filesystems() {
    if (mount("none", "/dev", "devtmpfs", 0, NULL) != 0) {
        perror("Error mounting devtmpfs");
    }
    if (mount("none", "/proc", "proc", 0, NULL) != 0) {
        perror("Error mounting proc");
    }
    if (mount("none", "/sys", "sysfs", 0, NULL) != 0) {
        perror("Error mounting sysfs");
    }
    if (system("/bin/mdev -s") != 0) {
        perror("Error running mdev");
    }
    // echo /bin/mdev > /proc/sys/kernel/hotplug
    int fd = open("sys/class/leds/NAME:COLOR:LOCATION/brightness", O_WRONLY);
    if (fd != 1) {
        write(fd, "/bin/mdev", 9);
        close(fd);
    } else {
        perror("Error writing /bin/mdev to /proc/sys/kernel/hotplug");
    }
}

void cleanup(int sig) {
    xdp_cleanup(interface_collection);
    rtnl_cleanup();
    exit(EXIT_FAILURE);
}

int main(int argc, char const *argv[]) {
    struct timespec   now;
    struct itimerspec new_value;
    ssize_t           s;
    uint64_t          exp;
    CpuCollection*    cpu_collection;
    MemoryCollection* memory_collection;

#ifndef VX_DEV
    setup_filesystems();
#endif

    // Set up signal handlers for cleanup
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGKILL, cleanup);

    lv_init();

    // Linux display device init
    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    // Create GUI background
    create_background();

    // Initialize objects
    interface_collection = init_interfaces();
    if (!interface_collection) {
        perror("Error: init_interfaces");
        exit(EXIT_FAILURE);
    }
    cpu_collection = init_cpus();
    if (!cpu_collection) {
        perror("Error: init_cpus");
        exit(EXIT_FAILURE);
    }
    memory_collection = init_memory();
    if (!memory_collection) {
        perror("Error: init_memory");
        exit(EXIT_FAILURE);
    }

    // Initialize Netlink socket
    if (rtnl_initialize() < 0)
        cleanup(0);

    // Load config and initialize objects
    if (load_configuration(interface_collection) < 0) {
        perror("load_configuration");
        cleanup(0);
    }

    // Init selector on first interface
    selector.selected = (void*)interface_collection->input_head;
    selector.display_mode = VX_DISPLAY_BYTES;

    interfaces_chart_change_visibility();

    // Initialize timer
    if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
        perror("clock_gettime");
        cleanup(0);
    }
    // Initial expiration
    new_value.it_value.tv_sec  =  now.tv_sec + (now.tv_nsec + VX_REFRESH_TIME) / 1000000000L;
    new_value.it_value.tv_nsec = (now.tv_nsec + VX_REFRESH_TIME) % 1000000000L;
    // Interval for periodic timer
    new_value.it_interval.tv_sec  = VX_REFRESH_TIME / 1000000000L;
    new_value.it_interval.tv_nsec = VX_REFRESH_TIME % 1000000000L;

    int fd = timerfd_create(CLOCK_REALTIME, 0);
    if (fd == -1) {
        perror("timerfd_create");
        cleanup(0);
    }
    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1) {
        perror("timerfd_settime");
        cleanup(0);
    }

    // Input listener thread
    pthread_t evdev_thread;
    pthread_create(&evdev_thread, NULL, select_interface, NULL);

    // Main loop
    size_t tick = 0;
    while(1) {

#ifdef VX_DEV
        if (active_tty()) {
            lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lv_scr_act(), LV_OBJ_FLAG_HIDDEN);
            lv_obj_invalidate(lv_scr_act());
        }
#endif

        if (tick%10 == 0) {
            // Check interfaces up/down
            interfaces_refresh();

            // Update charts
            pthread_mutex_lock(&main_mutex);
            if (interfaces_chart_update() < 0)
                cleanup(0);
            pthread_mutex_unlock(&main_mutex);

            if (cpus_chart_update(cpu_collection) < 0)
                cleanup(0);

            if (memory_chart_update(memory_collection) < 0)
                cleanup(0);
        }

        pthread_mutex_lock(&main_mutex);
        lv_timer_handler();
        pthread_mutex_unlock(&main_mutex);

        s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) {
            perror("timerfd read");
            cleanup(0);
        }
        tick = (tick+1)%100;
    }

    cleanup(0);
}
