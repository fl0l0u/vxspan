#ifndef VX_STATS
#define VX_STATS

#include <linux/types.h>
#include "vx_models.h"

// XDP struct
struct vlan_stats {
    __u64 bytes;
    __u64 packets;
    __u64 dropped_bytes;
    __u64 dropped;
};

int collect_interfaces_data(InterfaceCollection* collection);
int collect_cpus_data(CpuCollection* collection);
int collect_memory_data(MemoryCollection* collection);

#endif
