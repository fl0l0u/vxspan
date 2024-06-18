#ifndef VX_UTILS
#define VX_UTILS

#include "vx_config.h"
#include "vx_models.h"

char* calculate_size(uint64_t size);

void interface_update_sma(Interface* interface);
void vlan_update_sma(Vlan* vlan);

int highest_set_bit_position(uint64_t value);

#ifdef VX_DEV
bool active_tty();
#endif

#endif