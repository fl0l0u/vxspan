#ifndef VX_MODELS
#define VX_MODELS 

#include <stdbool.h>
#include <linux/if.h>
#include <linux/if_link.h>

#include "vx_config.h"

// Interfaces
typedef struct InterfaceStats {
    uint64_t rx_bytes;
    uint64_t rx_packets;
    uint64_t rx_dropped;
    uint64_t tx_bytes;
    uint64_t tx_packets;
    uint64_t tx_dropped;
} InterfaceStats;

typedef struct InterfaceBuffer {
    struct InterfaceStats data[VX_NETWORK_CHART_SIZE+1];
    int head;
    int count;
} InterfaceBuffer;

typedef enum {
    VX_CLASS_INPUT_INTERFACE,
    VX_CLASS_OUTPUT_INTERFACE,
    VX_CLASS_VLAN
} vx_class;

struct Interface;

typedef struct Vlan {
    vx_class type;
    // Stats
    struct InterfaceBuffer buffer;
    struct Vlan* prev;
    struct Vlan* next;
    struct InterfaceStats diff_max;
    struct InterfaceStats diff_sma;
    // Identifiers
    struct Interface* parent;
    struct Interface* redirection;
    int vlan_id;
    // Display
    lv_obj_t* name;
    lv_obj_t*          line;
    lv_point_precise_t points[4];
    bool   is_up;
    // Chart
    lv_obj_t*          network_chart;
    lv_chart_series_t* rx_bytes;
    lv_chart_series_t* rx_packets;
    lv_chart_series_t* rx_dropped;
    lv_obj_t*          chart_label;
} Vlan;

struct InterfaceCollection;

typedef struct Interface {
    // Same alignement as Vlan --
    vx_class type;
    // Stats
    struct InterfaceBuffer buffer;
    struct Interface* prev;
    struct Interface* next;
    struct InterfaceStats diff_max;
    struct InterfaceStats diff_sma;
    // --
    struct Vlan*  vlan_stats;
    // Identifiers
    struct InterfaceCollection* parent;
    int  if_index;
    char interface_name[IFNAMSIZ];
    // BPF
    int    vlan_stats_fd;
    int    vlan_redirect_map_fd;
    // Display
    lv_obj_t* name;
    lv_obj_t* image;
    lv_obj_t* status;
    bool      is_up;
    struct bpf_object* bpf_prog;
    lv_obj_t*          xdp_mode;
    struct Vlan*  vlan_selected;
    // Chart
    lv_chart_series_t* rx_bytes;
    lv_chart_series_t* rx_packets;
    lv_chart_series_t* rx_dropped;
    lv_chart_series_t* tx_bytes;
    lv_chart_series_t* tx_packets;
    lv_chart_series_t* tx_dropped;
    int bytes_scale;
    int packets_scale;
} Interface;

typedef struct InterfaceCollection {
    struct Interface* input_head;
    struct Interface* output_head;
    int input_count;
    int output_count;
    lv_obj_t* network_chart;
    lv_obj_t* network_label;
    lv_obj_t* network_rx_label;
    lv_obj_t* network_tx_label;
} InterfaceCollection;

// CPUs
typedef struct CpuBuffer {
    int data[VX_CPU_CHART_SIZE+1]; // percent use
    int head;
    int count;
} CpuBuffer;

struct CpuCollection;

typedef struct Cpu {
    struct CpuCollection* parent;
    int id;
    struct CpuBuffer buffer;
    struct Cpu* next;
    lv_chart_series_t* cpu_load;
    lv_obj_t*          cpu_label;
} Cpu;

typedef struct CpuCollection {
    struct Cpu* head;
    int count;
    lv_obj_t* cpus_label;
    lv_obj_t* cpus_chart;
} CpuCollection;

// Memory
typedef struct MemoryBuffer {
    uint64_t data[VX_MEMORY_CHART_SIZE+1];
    int head;
    int count;
} MemoryBuffer;

struct MemoryCollection;

typedef struct Memory {
    struct MemoryCollection* parent;
    char name[32];
    struct MemoryBuffer buffer;
    struct Memory* next;
    lv_chart_series_t* memory_load;
    lv_obj_t*          memory_label;
} Memory;

typedef struct MemoryCollection {
    struct Memory* head;
    int count;
    lv_obj_t* memory_label;
    lv_obj_t* memory_chart;
    uint64_t total;
} MemoryCollection;

// Interfaces
InterfaceCollection* init_interfaces();

Interface* add_input_interface(InterfaceCollection* collection, int if_index, const char* interface_name);
Interface* add_output_interface(InterfaceCollection* collection, int if_index, const char* interface_name);
void Interface_up(Interface* interface);
void Interface_down(Interface* interface);
void Interface_refresh(Interface* interface);
void Interface_set_focus(Interface* interface, const bool focus, const vx_display_mode mode);
void OutputInterface_position(Interface* interface, int i);
void update_interface_data(Interface* interface, InterfaceStats time_interval_stats);

Vlan* add_or_update_vlan(Interface* interface, int vlan_id);
void Vlan_reposition(Vlan* vlan);
void Vlan_refresh(Vlan* vlan);
void Vlan_visible(Vlan* vlan, const bool state);
void Vlan_set_focus(Vlan* vlan, const bool focus, const vx_display_mode mode);
void update_vlan_data(Vlan* vlan, InterfaceStats time_interval_stats);

int  init_circular_buffer(InterfaceBuffer* buffer);
void add_data_to_buffer(InterfaceBuffer* buffer, InterfaceStats time_interval_stats);

// Cpu
CpuCollection* init_cpus();
Cpu* add_cpu(CpuCollection* collection, int id);
void update_cpu_data(Cpu* cpu, int load);

// Memory
MemoryCollection* init_memory();
Memory* add_memory(MemoryCollection* collection, const char* name);
void update_memory_data(Memory* memory, uint64_t load);

#endif