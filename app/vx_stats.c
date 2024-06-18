#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/types.h>
#include <net/if.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <sys/sysinfo.h>
#include <netlink/cache.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "lvgl/lvgl.h"

#include "vx_stats.h"
#include "vx_network.h"

// Interfaces
extern struct nl_sock *sock;

int collect_interface_data(const int if_index, InterfaceStats* interface_stats) {
    struct rtnl_link* link;
    if (rtnl_link_get_kernel(sock, if_index, NULL, &link) < 0) {
        perror("collect_interface_data: Netlink get object from kernel failed");
        return -1;
    }
    interface_stats->rx_bytes   =  rtnl_link_get_stat(link, RTNL_LINK_RX_BYTES);
    interface_stats->rx_packets =  rtnl_link_get_stat(link, RTNL_LINK_RX_PACKETS);
    interface_stats->rx_dropped =  rtnl_link_get_stat(link, RTNL_LINK_RX_DROPPED);
    interface_stats->tx_bytes   =  rtnl_link_get_stat(link, RTNL_LINK_TX_BYTES);
    interface_stats->tx_packets =  rtnl_link_get_stat(link, RTNL_LINK_TX_PACKETS);
    interface_stats->tx_dropped =  rtnl_link_get_stat(link, RTNL_LINK_TX_DROPPED);
    rtnl_link_put(link);
    return 0;
}

int collect_interfaces_data(InterfaceCollection* collection) {
    Interface* interface = collection->input_head;

    bool vlans[4096];

    while (interface) {
        InterfaceStats interface_stats;
        if (collect_interface_data(interface->if_index, &interface_stats))
            return -1;
        update_interface_data(interface, interface_stats);

        int if_index = interface->if_index;
        int map_fd = interface->vlan_stats_fd;
        long long key, prev_key;
        key = -1;
        // Collect from BPF map VLAN list
        if (interface->type == VX_CLASS_INPUT_INTERFACE) {
            while(bpf_map_get_next_key(map_fd, &prev_key, &key) == 0) {
                int vlan_id = key;
                struct vlan_stats value;
                if (bpf_map_lookup_elem(map_fd, &key, &value) < 0) {
                    printf("DEBUG %d, %d, %x\n", map_fd, key, &value);
                    perror("collect_interfaces_data: bpf_map_lookup_elem");
                    return -1;
                }
                InterfaceStats interface_stats;
                interface_stats.rx_bytes   = value.bytes;
                interface_stats.rx_packets = value.packets;
                interface_stats.rx_dropped = value.dropped;
                Vlan* vlan = add_or_update_vlan(interface, vlan_id);
                if (!vlan)
                    return -1;
                update_vlan_data(vlan, interface_stats);
                vlans[vlan_id] = true;
                prev_key=key;
            }
            // Fill stats for configured VLANs with no data in BPF map
            Vlan* vlan = interface->vlan_stats;
            InterfaceStats zeros = {.rx_bytes = 0, .rx_packets = 0, .rx_dropped = 0};
            while (vlan) {
                if (!vlans[vlan->vlan_id]) {
                    if (vlan->buffer.count > 0) {
                        update_vlan_data(vlan, vlan->buffer.data[(vlan->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1)]);
                    } else {
                        update_vlan_data(vlan, zeros);
                    }
                }
                vlan = vlan->next;
            }
        }
        interface = interface->next;
    }

    interface = collection->output_head;
    while (interface) {
        InterfaceStats interface_stats;
        if (collect_interface_data(interface->if_index, &interface_stats))
            return -1;
        update_interface_data(interface, interface_stats);
        interface = interface->next;
    }
    return 0;
}

// CPUs
int collect_cpus_data(CpuCollection* collection) {
    FILE* file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    Cpu* cpu = collection->head;
    char buffer[256];
    int cpu_index, cpus_count = sysconf(_SC_NPROCESSORS_ONLN), i = 0;
    unsigned long user, nice, system, idle, iowait, irq, softirq, total, busy;
    while (fgets(buffer, sizeof(buffer), file) && cpu) {
        if (sscanf(buffer, "cpu%d %lu %lu %lu %lu %lu %lu %lu", &cpu_index, &user, &nice, &system, &idle, &iowait, &irq, &softirq) == 8) {
            if (!i++)
                continue;
            total = user + nice + system + idle + iowait + irq + softirq;
            busy = total - idle;
            // printf("-stat: cpu%d : %f %%\n", cpu_index, (double)busy/total*100.0);
            update_cpu_data(cpu, (int)((double)busy / total * 100.0));
            cpu = cpu->next;
        }
    }
    fclose(file);
    return 0;
}

// Memory
int collect_memory_data(MemoryCollection* collection) {
    Memory* memory = collection->head;
    if (!memory) {
        perror("'Main' Memory not initialized");
        return -1;
    }
    // Main
    struct sysinfo info;
    if (sysinfo(&info) < 0) {
        perror("sysinfo");
        return -1;
    }
    uint64_t freeram;
    freeram  = ((uint64_t) info.freeram * info.mem_unit);
    update_memory_data(memory, (collection->total - freeram));

    if (!memory->next) {
        perror("'Self' Memory not initialized");
        return -1;
    }
    // Self
    FILE *file;
    char fstatm[1024];
    file = fopen("/proc/self/status", "r");
    if (file == NULL) {
        perror("Error opening file /proc/self/status");
        exit(EXIT_FAILURE);
    }
    char line[128];
    uint64_t vmrss;
    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            int i = strlen(line);
            const char* p = line;
            while (*p < '0' || *p > '9')
                p++;
            line[i-3] = '\0';
            vmrss = atol(p);
            break;
        }
    }
    fclose(file);
    update_memory_data(memory->next, vmrss*1000);

    return 0;
}
