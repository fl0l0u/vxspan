#include <inttypes.h>
#include <linux/vt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "vx_utils.h"
#include "lvgl/lvgl.h"

#ifdef VX_DEV
bool active_tty() {
	FILE *file;
	char active_tty[100];
	char *current_tty;

	file = fopen("/sys/class/tty/tty0/active", "r");
	if (file == NULL) {
		perror("Error opening file /sys/class/tty/tty0/active");
		exit(EXIT_FAILURE);
	}
	if (fscanf(file, "%99s", active_tty) != 1) {
		perror("Error reading active TTY from file");
		fclose(file);
		exit(EXIT_FAILURE);
	}
	fclose(file);

	current_tty = ttyname(STDIN_FILENO);
	if (current_tty == NULL) {
		perror("Error getting current TTY");
		exit(EXIT_FAILURE);
	}

	char *current_tty_basename = strrchr(current_tty, '/');
	if (current_tty_basename) {
		current_tty_basename++;  // Move past the '/'
	} else {
		current_tty_basename = current_tty;  // No '/' found, use the whole string
	}

	return strcmp(current_tty_basename, active_tty) == 0;
}
#endif

#define DIM(x) (sizeof(x)/sizeof(*(x)))

static const char     *sizes[]   = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL *
                                   1024ULL * 1024ULL * 1024ULL;

char* calculate_size(uint64_t size) {   
	char* result = (char *) malloc(sizeof(char) * 20);
	if(!result)
		return NULL;
	uint64_t  multiplier = exbibytes;
	for (size_t i = 0; i < DIM(sizes); i++, multiplier /= 1024) {   
		if (size < multiplier)
			continue;
		if (size % multiplier == 0)
			sprintf(result, "%" PRIu64 " %s", size / multiplier, sizes[i]);
		else
			sprintf(result, "%.1f %s", (float) size / multiplier, sizes[i]);
		return result;
	}
	strcpy(result, "0");
	return result;
}

void interface_update_sma(Interface* interface) {
	InterfaceStats stats;
	if (interface->buffer.count < 2)
		return;
	double acc_rx_bytes, acc_rx_packets, acc_rx_dropped, acc_tx_bytes, acc_tx_packets, acc_tx_dropped;
	int start = (interface->buffer.head + VX_NETWORK_CHART_SIZE + 1 - interface->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
 	for(int i = 0; i < interface->buffer.count - 1; i++) {
 		InterfaceStats *curr = &interface->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
 		               *prev = &interface->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
		uint64_t diff_rx_bytes   = curr->rx_bytes   - prev->rx_bytes;
		uint64_t diff_rx_packets = curr->rx_packets - prev->rx_packets;
		uint64_t diff_rx_dropped = curr->rx_dropped - prev->rx_dropped;
		uint64_t diff_tx_bytes   = curr->tx_bytes   - prev->tx_bytes;
		uint64_t diff_tx_packets = curr->tx_packets - prev->tx_packets;
		uint64_t diff_tx_dropped = curr->tx_dropped - prev->tx_dropped;
		if (interface->diff_max.rx_bytes < diff_rx_bytes)
			interface->diff_max.rx_bytes = diff_rx_bytes;
		if (interface->diff_max.rx_packets < diff_rx_packets)
			interface->diff_max.rx_packets = diff_rx_packets;
		if (interface->diff_max.rx_dropped < diff_rx_dropped)
			interface->diff_max.rx_dropped = diff_rx_dropped;
		if (interface->diff_max.tx_bytes < diff_tx_bytes)
			interface->diff_max.tx_bytes = diff_tx_bytes;
		if (interface->diff_max.tx_packets < diff_tx_packets)
			interface->diff_max.tx_packets = diff_tx_packets;
		if (interface->diff_max.tx_dropped < diff_tx_dropped)
			interface->diff_max.tx_dropped = diff_tx_dropped;
		if (curr->rx_bytes >= prev->rx_bytes)
			acc_rx_bytes   += (double)diff_rx_bytes;
		if (curr->rx_packets >= prev->rx_packets)
			acc_rx_packets += (double)diff_rx_packets;
		if (curr->rx_dropped >= prev->rx_dropped)
			acc_rx_dropped += (double)diff_rx_dropped;
		if (curr->tx_bytes >= prev->tx_bytes)
			acc_tx_bytes   += (double)diff_tx_bytes;
		if (curr->tx_packets >= prev->tx_packets)
			acc_tx_packets += (double)diff_tx_packets;
		if (curr->tx_dropped >= prev->tx_dropped)
			acc_tx_dropped += (double)diff_tx_dropped;
	}
	interface->diff_sma.rx_bytes   = (uint64_t)(acc_rx_bytes   / (double)interface->buffer.count);
	interface->diff_sma.rx_packets = (uint64_t)(acc_rx_packets / (double)interface->buffer.count);
	interface->diff_sma.rx_dropped = (uint64_t)(acc_rx_dropped / (double)interface->buffer.count);
	interface->diff_sma.tx_bytes   = (uint64_t)(acc_tx_bytes   / (double)interface->buffer.count);
	interface->diff_sma.tx_packets = (uint64_t)(acc_tx_packets / (double)interface->buffer.count);
	interface->diff_sma.tx_dropped = (uint64_t)(acc_tx_dropped / (double)interface->buffer.count);
}
void vlan_update_sma(Vlan* vlan) {
	InterfaceStats stats;
	if (vlan->buffer.count < 2)
		return;
	double acc_rx_bytes, acc_rx_packets, acc_rx_dropped, acc_tx_bytes, acc_tx_packets, acc_tx_dropped;
	int start = (vlan->buffer.head + VX_NETWORK_CHART_SIZE + 1 - vlan->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
 	for(int i = 0; i < vlan->buffer.count - 1; i++) {
 		InterfaceStats *curr = &vlan->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
 		               *prev = &vlan->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
		uint64_t diff_rx_bytes   = curr->rx_bytes   - prev->rx_bytes;
		uint64_t diff_rx_packets = curr->rx_packets - prev->rx_packets;
		uint64_t diff_rx_dropped = curr->rx_dropped - prev->rx_dropped;
		if (vlan->diff_max.rx_bytes < diff_rx_bytes)
			vlan->diff_max.rx_bytes = diff_rx_bytes;
		if (vlan->diff_max.rx_packets < diff_rx_packets)
			vlan->diff_max.rx_packets = diff_rx_packets;
		if (vlan->diff_max.rx_dropped < diff_rx_dropped)
			vlan->diff_max.rx_dropped = diff_rx_dropped;

		acc_rx_bytes   += (double)diff_rx_bytes;
		acc_rx_packets += (double)diff_rx_packets;
		acc_rx_dropped += (double)diff_rx_dropped;
	}
	vlan->diff_sma.rx_bytes   = (uint64_t)(acc_rx_bytes   / (double)vlan->buffer.count);
	vlan->diff_sma.rx_packets = (uint64_t)(acc_rx_packets / (double)vlan->buffer.count);
	vlan->diff_sma.rx_dropped = (uint64_t)(acc_rx_dropped / (double)vlan->buffer.count);
}

// Function to find the highest set bit position in a value
int highest_set_bit_position(uint64_t value) {
	if(!value)
		return 0;
    int position = 0;
    while (value >>= 1) {
        position++;
    }
    return position;
}