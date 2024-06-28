#ifndef VX_VIEW
#define VX_VIEW

#include "vx_models.h"

int create_background();

void interfaces_refresh();

int interfaces_chart_update();
int cpus_chart_update(CpuCollection* collection);
int memory_chart_update(MemoryCollection* collection);

void* select_interface(void* args);
void interfaces_chart_change_visibility();

#endif