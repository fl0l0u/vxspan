#ifndef VX_NETWORK
#define VX_NETWORK

int  rtnl_initialize();
void rtnl_cleanup();

bool interface_is_up(const int if_index);
bool interface_is_promisc(const int if_index);

int prepare_input_interface(const char* ifname);
int prepare_output_interface(const char* ifname);

#endif