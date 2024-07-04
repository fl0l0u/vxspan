#ifndef VX_VIEW
#define VX_VIEW

#include "vx_models.h"

typedef enum {
	VX_NONE,
	VX_RX_BYTES,
	VX_TX_BYTES,
	VX_RX_DROPPED_BYTES,
	VX_RX_PACKETS,
	VX_TX_PACKETS,
	VX_RX_DROPPED,
	VX_TX_DROPPED,
} vx_display;

int create_background();

void interfaces_refresh();

int interfaces_chart_update();
int cpus_chart_update(CpuCollection* collection);
int memory_chart_update(MemoryCollection* collection);

void* select_interface(void* args);
int  interfaces_chart_change_visibility();

#endif