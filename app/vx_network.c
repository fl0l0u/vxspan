#include <net/if.h>
#include <netlink/route/link/vlan.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#include <netlink/netlink.h>
#include <netlink/cache.h>

#include "vx_network.h"
#include "vx_config.h"
#include "vx_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

struct nl_sock *sock;

int rtnl_initialize() {
    sock = nl_socket_alloc();
    if (!sock) {
        perror("Error allocating netlink socket");
        return -1;
    }
    if (nl_connect(sock, NETLINK_ROUTE) != 0) {
        perror("Error connecting to netlink socket");
        nl_socket_free(sock);
        return -1;
    }
    return 0;
}

void rtnl_cleanup() {
    if (sock)
        nl_close(sock);
    if (sock)
        nl_socket_free(sock);
}

int interface_set_flag(const int if_index, const unsigned int flag) {
	struct rtnl_link* link;
	struct rtnl_link* change;
	struct nl_cache* cache;
	if (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache) < 0) {
		perror("Error allocating netlink cache");
		return -1;
	}
	link = rtnl_link_get(cache, if_index);
	if (!link) {
		perror("Error allocating netlink link");
		nl_cache_free(cache);
		return -1;
	}
	change = rtnl_link_alloc();
	rtnl_link_set_flags(change, flag);
	if (rtnl_link_change(sock, link, change, 0) < 0) {
		perror("Can't change flags");
		rtnl_link_put(link);
		rtnl_link_put(change);
		nl_cache_free(cache);
		return -1;
	}
	rtnl_link_put(link);
	rtnl_link_put(change);
	nl_cache_free(cache);
	return 0;
}
int interface_set_up(const char* interface_name) {
	int if_index = if_nametoindex(interface_name);
	// printf("interface_set_up(%d)\n", if_index);
	return interface_set_flag(if_index, IFF_UP);
}
int interface_set_promisc(const char* interface_name) {
	int if_index = if_nametoindex(interface_name);
	// printf("interface_set_promisc(%d)\n", if_index);
	return interface_set_flag(if_index, IFF_PROMISC);
}

unsigned int interface_get_flags(const int if_index) {
	struct rtnl_link *link;
	struct nl_cache* cache;
	if (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache) < 0) {
		perror("Error allocating netlink cache");
		return -1;
	}
	link = rtnl_link_get(cache, if_index);
	if (!link) {
		perror("Error allocating netlink link");
		nl_cache_free(cache);
		return -1;
	}
	unsigned int flags = rtnl_link_get_flags(link);
	rtnl_link_put(link);
	nl_cache_free(cache);
	return flags;
}
bool interface_is_up(const int if_index) {
	unsigned int flags = interface_get_flags(if_index);
	return (flags & IFF_UP) && (flags & IFF_RUNNING);
}
bool interface_is_promisc(const int if_index) {
	unsigned int flags = interface_get_flags(if_index);
	return (flags & IFF_PROMISC);
}

int interface_disable_rxvlan(const char* ifname) {
	char cmdline[64];
	if(sprintf(cmdline, "ethtool -K %.16s rx-vlan-offload off", ifname) < 0) {
		perror("cmdline sprintf");
		return -1;
	}
	int ret = system(cmdline);
	return ret;
}
int interface_disable_txvlan(const char* ifname) {
	char cmdline[64];
	if(sprintf(cmdline, "ethtool -K %.16s tx-vlan-offload off", ifname) < 0) {
		perror("cmdline sprintf");
		return -1;
	}
	int ret = system(cmdline);
	return ret;
}
int prepare_input_interface(const char* ifname) {
	if (interface_disable_rxvlan(ifname) < 0) {
		perror("interface_disable_rxvlan");
		return -1;
	}
	if (interface_set_promisc(ifname) < 0) {
		perror("interface_set_promisc");
		return -1;
	}
	if (interface_set_up(ifname) < 0) {
		perror("interface_set_up");
		return -1;
	}
	return 0;
}
int prepare_output_interface(const char* ifname) {
	if (interface_disable_txvlan(ifname) < 0) {
		perror("interface_disable_txvlan");
		return -1;
	}
	if (interface_set_up(ifname) < 0) {
		perror("interface_set_up");
		return -1;
	}
	return 0;
}