#include <linux/types.h>
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#define VLAN_VID_MASK 0x0fff
/*
 * Note: including linux/compiler.h or linux/kernel.h for the macros below
 * conflicts with vmlinux.h include in BPF files, so we define them here.
 *
 * Following functions are taken from kernel sources and
 * break aliasing rules in their original form.
 *
 * While kernel is compiled with -fno-strict-aliasing,
 * perf uses -Wstrict-aliasing=3 which makes build fail
 * under gcc 4.4.
 *
 * Using extra __may_alias__ type to allow aliasing
 * in this case.
 */
typedef __u8  __attribute__((__may_alias__))  __u8_alias_t;
typedef __u16 __attribute__((__may_alias__)) __u16_alias_t;
typedef __u32 __attribute__((__may_alias__)) __u32_alias_t;
typedef __u64 __attribute__((__may_alias__)) __u64_alias_t;

static __always_inline void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(__u8_alias_t  *) res = *(volatile __u8_alias_t  *) p; break;
	case 2: *(__u16_alias_t *) res = *(volatile __u16_alias_t *) p; break;
	case 4: *(__u32_alias_t *) res = *(volatile __u32_alias_t *) p; break;
	case 8: *(__u64_alias_t *) res = *(volatile __u64_alias_t *) p; break;
	default:
		asm volatile ("" : : : "memory");
		__builtin_memcpy((void *)res, (const void *)p, size);
		asm volatile ("" : : : "memory");
	}
}

static __always_inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile  __u8_alias_t *) p = *(__u8_alias_t  *) res; break;
	case 2: *(volatile __u16_alias_t *) p = *(__u16_alias_t *) res; break;
	case 4: *(volatile __u32_alias_t *) p = *(__u32_alias_t *) res; break;
	case 8: *(volatile __u64_alias_t *) p = *(__u64_alias_t *) res; break;
	default:
		asm volatile ("" : : : "memory");
		__builtin_memcpy((void *)p, (const void *)res, size);
		asm volatile ("" : : : "memory");
	}
}

#define READ_ONCE(x)					\
({							\
	union { typeof(x) __val; char __c[1]; } __u =	\
		{ .__c = { 0 } };			\
	__read_once_size(&(x), __u.__c, sizeof(x));	\
	__u.__val;					\
})

#define WRITE_ONCE(x, val)				\
({							\
	union { typeof(x) __val; char __c[1]; } __u =	\
		{ .__val = (val) }; 			\
	__write_once_size(&(x), __u.__c, sizeof(x));	\
	__u.__val;					\
})

/* Add a value using relaxed read and relaxed write. Less expensive than
 * fetch_add when there is no write concurrency.
 */
#define NO_TEAR_ADD(x, val) WRITE_ONCE((x), READ_ONCE(x) + (val))
#define NO_TEAR_INC(x) NO_TEAR_ADD((x), 1)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct vlan_stat {
	__u64 bytes;
	__u64 packets;
	__u64 dropped;
};

// Define a map to store per-VLAN redirect interfaces
// 0 -> untagged (default)
// 1..4094 -> tagged N
// 4095 -> all
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u32);
	__type(value, __u32);
	__uint(max_entries, 4096);
} vlan_redirect_map SEC(".maps");

// Define a map to store per-VLAN statistics (bytes and packets)
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u32);
	__type(value, struct vlan_stat);
	__uint(max_entries, 4096);
} vlan_stats SEC(".maps");

struct dot1q {
	unsigned char h_dest[6];   /* destination eth addr */
	unsigned char h_source[6]; /* source ether addr	*/
	__be16		h_proto;	 /* packet type ID field */
	__be16		vlan_tcid;   /* VLAN TCI field	   */
};

static __always_inline void update_statistics(__u32 vlan_id, int size) {
	struct vlan_stat *stats = bpf_map_lookup_elem(&vlan_stats, &vlan_id);
	if (stats) {
		NO_TEAR_ADD(stats->bytes, size);
		NO_TEAR_INC(stats->packets);
	} else {
		struct vlan_stat new_stats = {.bytes = size, .packets = 1, .dropped = 0};
		bpf_map_update_elem(&vlan_stats, &vlan_id, &new_stats, BPF_ANY);
	}
}
static __always_inline void register_drop(__u32 vlan_id) {
	struct vlan_stat *stats = bpf_map_lookup_elem(&vlan_stats, &vlan_id);
	if (stats) {
		NO_TEAR_INC(stats->dropped);
	} else {
		struct vlan_stat new_stats = {.bytes = 0, .packets = 0, .dropped = 1};
		bpf_map_update_elem(&vlan_stats, &vlan_id, &new_stats, BPF_ANY);
	}
}

SEC("xdp")
int xdp_vlan_filter(struct xdp_md *ctx) {
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
	struct dot1q *vlan_hdr;
	__u32 vlan_id = 0; // Default VLAN ID for untagged packets
	__u32 *ifindex = NULL;

	// Check if the packet is large enough to contain Ethernet header
	if ((void*)eth + sizeof(*eth) > data_end) {
		register_drop(vlan_id);
		return XDP_DROP;
	}

	// Check if the packet has VLAN tag
	if (eth->h_proto == bpf_htons(ETH_P_8021Q) || eth->h_proto == bpf_htons(ETH_P_8021AD)) {
		vlan_hdr = (void*)eth;
		if ((void*)vlan_hdr + sizeof(*vlan_hdr) > data_end) {
			register_drop(vlan_id);
			return XDP_DROP;
		}
		vlan_id = bpf_ntohs(vlan_hdr->vlan_tcid) & VLAN_VID_MASK;
	}

	ifindex = bpf_map_lookup_elem(&vlan_redirect_map, &vlan_id);
	if (!ifindex) {
		register_drop(vlan_id);
		return XDP_DROP;
	}

	// Lookup global redirect interface (if any)
	__u32 global_vlan_key = 4095; // Key for global redirection
	__u32 *global_ifindex = bpf_map_lookup_elem(&vlan_redirect_map, &global_vlan_key);

	if (global_ifindex && *global_ifindex != 0) {
		if (bpf_redirect(*global_ifindex, 0) == XDP_REDIRECT) {
			// Update statistics for specific VLAN
			update_statistics(vlan_id, data_end - data);

			// Update statistics for global redirection
			update_statistics(global_vlan_key, data_end - data);
			return XDP_REDIRECT;
		}
	}

	// Redirect the packet to the specified interface
	if (ifindex && *ifindex != 0) {
		if (bpf_redirect(*ifindex, 0) == XDP_REDIRECT) {
			// Update statistics for specific VLAN
			update_statistics(vlan_id, data_end - data);
			return XDP_REDIRECT;
		}
	}

	// No redirection criteria matched or interface index is 0, drop the packet
	register_drop(vlan_id);
	return XDP_DROP;
}

char _license[] SEC("license") = "GPL";