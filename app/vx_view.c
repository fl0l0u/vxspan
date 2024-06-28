#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <pthread.h>
#include <netlink/route/link.h>

#include "vx_config.h"
#include "vx_models.h"
#include "vx_stats.h"
#include "vx_utils.h"
#include "vx_view.h"

extern Selector selector; // main
extern pthread_mutex_t main_mutex;
extern InterfaceCollection* interface_collection;

int create_background() {
    // -- 1
    lv_obj_t* scr = lv_scr_act();
    if (!scr) {
        perror("lv_scr_act");
        return -1;
    }
    lv_obj_set_style_bg_color(scr, VX_GREY_COLOR, 0);
    lv_obj_move_background(scr);

    lv_obj_t* input_header = lv_obj_create(scr);
    if (!input_header) {
        perror("lv_obj_create allocation failed");
        return -1;
    }
    lv_obj_set_size(input_header, 800, 48);
    lv_obj_set_style_bg_color(input_header, VX_BLUE_PALETTE, 0);
    lv_obj_set_style_radius(input_header, 0, 0);
    lv_obj_set_style_border_width(input_header, 0, 0);

    lv_obj_t* title = lv_label_create(scr);
    if (!title) {
        perror("lv_label_create allocation failed");
        return -1;
    }
    lv_label_set_text(title, "  "VX_TITLE" "VX_VERSION);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, VX_WHITE_COLOR, 0);

    lv_obj_t* output_header = lv_obj_create(scr);
    if (!output_header) {
        perror("lv_obj_create allocation failed");
        return -1;
    }
    lv_obj_set_size(output_header, 800, 16);
    lv_obj_set_pos(output_header, 0, 184);
    lv_obj_set_style_bg_color(output_header, VX_BLUE_PALETTE, 0);
    lv_obj_set_style_radius(output_header, 0, 0);
    lv_obj_set_style_border_width(output_header, 0, 0);

    lv_obj_t* footer = lv_obj_create(scr);
    if (!footer) {
        perror("lv_obj_create allocation failed");
        return -1;
    }
    lv_obj_set_size(footer, 800, 32);
    lv_obj_set_pos(footer, 0, 568);
    lv_obj_set_style_bg_color(footer, VX_BLUE_PALETTE, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    return 0;
}

void interfaces_chart_change_visibility();
/*
 * <-<input>-<input>-<output>-<output>->
 *      |       |
 *   <vlan>  <vlan>
 *      |       |
 *   <vlan>  <vlan>
 */
void* select_interface(void* args) {
    int evdev_fd;
    if ((evdev_fd = open("/dev/input/event0", O_RDONLY)) < 0) {
        perror("evdev file descriptor open()");
        exit(EXIT_FAILURE);
    }
    struct input_event ev[64];
    int i, rd, fd;
    fd_set rdfs;
    fd = evdev_fd;

    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);

    while (1) {
        select(fd + 1, &rdfs, NULL, NULL, NULL);
        rd = read(fd, ev, sizeof(ev));

        if (rd < (int) sizeof(struct input_event)) {
            perror("evtest: error reading");
            return NULL;
        }

        for (i = 0; i < rd / sizeof(struct input_event); i++) {
            unsigned int type  = ev[i].type;
            unsigned int code  = ev[i].code;
            unsigned int value = ev[i].value;

            if (type == EV_KEY && value == 0) {
                pthread_mutex_lock(&main_mutex);
                Interface* interface = (Interface*)selector.selected;
                Vlan* vlan = (Vlan*)selector.selected;
                switch (code) {

                case KEY_RIGHT:
                    switch (interface->type) {

                    case VX_CLASS_INPUT_INTERFACE:
                        interface = (interface->next ? interface->next : interface->parent->output_head);
                        break;
                    case VX_CLASS_OUTPUT_INTERFACE:
                        interface = (interface->next ? interface->next : interface->parent->input_head);
                        break;
                    case VX_CLASS_VLAN:
                        interface = (vlan->parent->next ? vlan->parent->next : vlan->parent->parent->output_head);
                        break;
                    }
                    selector.selected = (void*)interface;
                    interfaces_chart_change_visibility();
                    break;
                case KEY_LEFT:
                    switch (interface->type) {

                    case VX_CLASS_INPUT_INTERFACE:
                        if (interface->prev)
                            interface = interface->prev;
                        else {
                            interface = interface->parent->output_head;
                            while(interface->next)
                                interface = interface->next;
                        }
                        break;
                    case VX_CLASS_OUTPUT_INTERFACE:
                        if (interface->prev)
                            interface = interface->prev;
                        else {
                            interface = interface->parent->input_head;
                            while(interface->next)
                                interface = interface->next;
                        }
                        break;
                    case VX_CLASS_VLAN:
                        if (vlan->parent->prev)
                            interface = vlan->parent->prev;
                        else {
                            interface = vlan->parent->parent->output_head;
                            while(interface->next)
                                interface = interface->next;
                        }
                        break;
                    }
                    selector.selected = (void*)interface;
                    interfaces_chart_change_visibility();
                    break;
                case KEY_UP:
                    if (vlan->type == VX_CLASS_VLAN) {
                        if (vlan->prev) {
                            selector.selected = (void*)vlan->prev;
                        } else {
                            selector.selected = (void*)vlan->parent;
                        }
                        interfaces_chart_change_visibility();
                    }
                    break;
                case KEY_DOWN:
                    if (vlan->type == VX_CLASS_VLAN) {
                        if (vlan->next) {
                            selector.selected = (void*)vlan->next;
                            interfaces_chart_change_visibility();
                        }
                    } else if (interface->type == VX_CLASS_INPUT_INTERFACE) {
                        if (interface->vlan_stats) {
                            selector.selected = (void*)interface->vlan_stats;
                            interfaces_chart_change_visibility();
                        }
                    }
                    break;
                case KEY_B:
                    if (selector.display_mode != VX_DISPLAY_BYTES) {
                        selector.display_mode = VX_DISPLAY_BYTES;
                        interfaces_chart_change_visibility();
                    }
                    break;
                case KEY_P:
                    if (selector.display_mode != VX_DISPLAY_PACKETS) {
                        selector.display_mode = VX_DISPLAY_PACKETS;
                        interfaces_chart_change_visibility();
                    }
                    break;
                case KEY_D:
                    if (selector.display_mode != VX_DISPLAY_DROPPED) {
                        selector.display_mode = VX_DISPLAY_DROPPED;
                        interfaces_chart_change_visibility();
                    }
                }
                pthread_mutex_unlock(&main_mutex);
            }
        }
    }
    ioctl(fd, EVIOCGRAB, (void*)0);
}


void interfaces_refresh() {
    Interface* iface = NULL;
    Vlan*      vlan  = NULL;

    iface = interface_collection->input_head;
    while (iface) {
        vlan = iface->vlan_stats;
        while (vlan) {
            Interface_refresh(vlan->parent);
            Interface_refresh(vlan->redirection);
            Vlan_refresh(vlan);
            vlan = vlan->next;
        }
        iface = iface->next;
    }
}

static int update_interface_label(lv_obj_t* label, uint64_t val, uint64_t diff, uint64_t count, uint64_t sma, int flag) {
    char *str_sma, *str_val, *str_diff;
    switch (flag) {
    case -1:
        lv_label_set_text(label, "No Tx for redirection");
        return 0;
    case RTNL_LINK_RX_BYTES:
    case RTNL_LINK_TX_BYTES:
        str_sma = calculate_size(sma);
        if (!str_sma) {
            perror("calculate_size malloc failed");
            return -1;
        }
        str_val = calculate_size(val);
        if (!str_val) {
            perror("calculate_size malloc failed");
            return -1;
        }
        str_diff = calculate_size(diff);
        if (!str_diff) {
            perror("calculate_size malloc failed");
            return -1;
        }
        break;
    }    
    switch (flag) {
    // Bytes
    case RTNL_LINK_RX_BYTES:
        lv_label_set_text_fmt(label, "Total Rx: %s | Rx byte/s: %s/s | Rx byte/s (avg. last %lds) %s/s", str_val, str_diff, count, str_sma);
        lv_obj_set_style_text_color(label, VX_GREEN_PALETTE, 0);
        break;
    case RTNL_LINK_TX_BYTES:
        lv_label_set_text_fmt(label, "Total Tx: %s | Tx byte/s: %s/s | Tx byte/s (avg. last %lds) %s/s", str_val, str_diff, count, str_sma);
        lv_obj_set_style_text_color(label, VX_ORANGE_PALETTE, 0);
        break;
    // Packets
    case RTNL_LINK_RX_PACKETS:
        lv_label_set_text_fmt(label, "Total Rx pkt: %"PRIu64" | Rx pkt/s: %"PRIu64"/s | Rx pkt/s (avg. last %lds) %"PRIu64"/s", val, diff, count, sma);
        lv_obj_set_style_text_color(label, VX_GREEN_PALETTE, 0);
        break;
    case RTNL_LINK_TX_PACKETS:
        lv_label_set_text_fmt(label, "Total Tx pkt: %"PRIu64" | Tx pkt/s: %"PRIu64"/s | Tx pkt/s (avg. last %lds) %"PRIu64"/s", val, diff, count, sma);
        lv_obj_set_style_text_color(label, VX_ORANGE_PALETTE, 0);
        break;
    // Dropped
    case RTNL_LINK_RX_DROPPED:
        lv_label_set_text_fmt(label, "Total Rx drop: %"PRIu64" | Rx drop/s: %"PRIu64"/s | Rx drop/s (avg. last %lds) %"PRIu64"/s", val, diff, count, sma);
        lv_obj_set_style_text_color(label, VX_ORANGE_PALETTE, 0);
        break;
    case RTNL_LINK_TX_DROPPED:
        lv_label_set_text_fmt(label, "Total Tx drop: %"PRIu64" | Tx drop/s: %"PRIu64"/s | Tx drop/s (avg. last %lds) %"PRIu64"/s", val, diff, count, sma);
        lv_obj_set_style_text_color(label, VX_RED_PALETTE, 0);
        break;
    }
    switch (flag) {
    case RTNL_LINK_RX_BYTES:
    case RTNL_LINK_TX_BYTES:
        free(str_sma);
        free(str_val);
        free(str_diff);
        break;
    }
    return 0;
}

int interfaces_chart_update() {
    if (collect_interfaces_data(interface_collection) < 0)
        return -1;

    // Input
    for (Interface* iface = interface_collection->input_head; iface != NULL; iface = iface->next) {
        int curr_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1),
            prev_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE - 1) % (VX_NETWORK_CHART_SIZE + 1);
        InterfaceStats *curr = &iface->buffer.data[curr_i],
                       *prev = &iface->buffer.data[prev_i],
                       *vcurr = NULL,
                       *vprev = NULL;
        // Bytes series
        uint64_t rx_diff = curr->rx_bytes - prev->rx_bytes;
        uint64_t tx_diff = curr->tx_bytes - prev->tx_bytes;
        uint64_t bytes_diff_max = (iface->diff_max.rx_bytes > iface->diff_max.tx_bytes ?
            iface->diff_max.rx_bytes :
            iface->diff_max.tx_bytes
        );

        uint64_t rxv_diff;

        int shift_amount = highest_set_bit_position(bytes_diff_max) - VX_NETWORK_CHART_RANGE_SHIFT_MAX + 1;
        if (shift_amount < 0)
            shift_amount = 0;

        // Scale changed, reset all series for interface
        if (shift_amount != iface->bytes_scale) {
            lv_chart_set_all_value(interface_collection->network_chart, iface->rx_bytes, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->tx_bytes, LV_CHART_POINT_NONE);

            int start = (iface->buffer.head + VX_NETWORK_CHART_SIZE + 1 - iface->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
            for (int i = 0; i < (iface->buffer.count - 1); i++) {
                InterfaceStats *curr = &iface->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
                               *prev = &iface->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                uint64_t rx_diff = curr->rx_bytes - prev->rx_bytes;
                uint64_t tx_diff = curr->tx_bytes - prev->tx_bytes;
            
                lv_chart_set_next_value(interface_collection->network_chart, iface->rx_bytes,  (rx_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->tx_bytes, -(tx_diff >> shift_amount));
            }

            // Vlans bytes/packets are always <= Interface ones
            for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
                lv_chart_set_all_value(interface_collection->network_chart, vlan->rx_bytes, LV_CHART_POINT_NONE);

                int start = (vlan->buffer.head + VX_NETWORK_CHART_SIZE + 1 - vlan->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
                for (int i = 0; i < (vlan->buffer.count - 1); i++) {
                    vcurr = &vlan->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)];
                    vprev = &vlan->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                    rxv_diff = vcurr->rx_bytes - vprev->rx_bytes;
                    lv_chart_set_next_value(interface_collection->network_chart, vlan->rx_bytes,  (rxv_diff >> shift_amount));
                }
            }
            iface->bytes_scale = shift_amount;
        // Same shift, simple insert
        } else {
            lv_chart_set_next_value(interface_collection->network_chart, iface->rx_bytes,  (rx_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->tx_bytes, -(tx_diff >> shift_amount));

            for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
                int vcurr_i = (vlan->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1),
                    vprev_i = (vlan->buffer.head + VX_NETWORK_CHART_SIZE - 1) % (VX_NETWORK_CHART_SIZE + 1);
                vcurr = &vlan->buffer.data[vcurr_i];
                vprev = &vlan->buffer.data[vprev_i];
                rxv_diff = vcurr->rx_bytes - vprev->rx_bytes;
                lv_chart_set_next_value(interface_collection->network_chart, vlan->rx_bytes,  (rxv_diff >> shift_amount));
            }
        }

        // Packets/dropped series
        uint64_t rxp_diff = curr->rx_packets - prev->rx_packets;
        uint64_t rxd_diff = curr->rx_dropped - prev->rx_dropped;
        uint64_t txp_diff = curr->tx_packets - prev->tx_packets;
        uint64_t txd_diff = curr->tx_dropped - prev->tx_dropped;
        uint64_t xxp_max = (iface->diff_max.rx_packets > iface->diff_max.tx_packets ? iface->diff_max.rx_packets : iface->diff_max.tx_packets);
        uint64_t xxd_max = (iface->diff_max.rx_dropped > iface->diff_max.tx_dropped ? iface->diff_max.rx_dropped : iface->diff_max.tx_dropped);
        uint64_t packets_diff_max = (xxd_max > xxp_max ? xxd_max : xxp_max);

        shift_amount = highest_set_bit_position(packets_diff_max) - VX_NETWORK_CHART_RANGE_SHIFT_MAX + 1;
        if (shift_amount < 0)
            shift_amount = 0;
        if (shift_amount != iface->packets_scale) {
            lv_chart_set_all_value(interface_collection->network_chart, iface->rx_packets, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->rx_dropped, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->tx_packets, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->tx_dropped, LV_CHART_POINT_NONE);
            
            int start = (iface->buffer.head + VX_NETWORK_CHART_SIZE + 1 - iface->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
            for (int i = 0; i < (iface->buffer.count - 1); i++) {
                InterfaceStats *curr = &iface->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
                               *prev = &iface->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                uint64_t rxp_diff = curr->rx_packets - prev->rx_packets;
                uint64_t rxd_diff = curr->rx_dropped - prev->rx_dropped;
                uint64_t txp_diff = curr->tx_packets - prev->tx_packets;
                uint64_t txd_diff = curr->tx_dropped - prev->tx_dropped;
                
                lv_chart_set_next_value(interface_collection->network_chart, iface->rx_packets,  (rxp_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->rx_dropped,  (rxd_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->tx_packets, -(txp_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->tx_dropped, -(txd_diff >> shift_amount));
            }
            // Vlans bytes/packets are always <= Interface ones
            for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
                lv_chart_set_all_value(interface_collection->network_chart, vlan->rx_packets, LV_CHART_POINT_NONE);
                lv_chart_set_all_value(interface_collection->network_chart, vlan->rx_dropped, LV_CHART_POINT_NONE);

                int start = (vlan->buffer.head + VX_NETWORK_CHART_SIZE + 1 - vlan->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
                for (int i = 0; i < (vlan->buffer.count - 1); i++) {
                    InterfaceStats *curr = &vlan->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
                                   *prev = &vlan->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                    rxp_diff = curr->rx_packets - prev->rx_packets;
                    rxd_diff = curr->rx_dropped - prev->rx_dropped;
                    lv_chart_set_next_value(interface_collection->network_chart, vlan->rx_packets, (rxp_diff >> shift_amount));
                    lv_chart_set_next_value(interface_collection->network_chart, vlan->rx_dropped, (rxd_diff >> shift_amount));
                }
            }
            iface->packets_scale = shift_amount;
        // Same shift, simple insert
        } else {
            lv_chart_set_next_value(interface_collection->network_chart, iface->rx_packets,  (rxp_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->rx_dropped,  (rxd_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->tx_packets, -(txp_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->tx_dropped, -(txd_diff >> shift_amount));

            for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
                InterfaceStats *curr = &vlan->buffer.data[vlan->buffer.head + 1],
                               *prev = &vlan->buffer.data[vlan->buffer.head];
                rxp_diff = curr->rx_packets - prev->rx_packets;
                rxd_diff = curr->rx_dropped - prev->rx_dropped;
                lv_chart_set_next_value(interface_collection->network_chart, vlan->rx_packets, (rxp_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, vlan->rx_dropped, (rxd_diff >> shift_amount));
            }
        }

        // Labels
        if (selector.selected == iface) {
            switch (selector.display_mode) {

            case VX_DISPLAY_BYTES:
                update_interface_label(interface_collection->network_rx_label, curr->rx_bytes, rx_diff, iface->buffer.count - 1, iface->diff_sma.rx_bytes, RTNL_LINK_RX_BYTES);
                update_interface_label(interface_collection->network_tx_label, curr->tx_bytes, tx_diff, iface->buffer.count - 1, iface->diff_sma.tx_bytes, RTNL_LINK_TX_BYTES);
                break;
            case VX_DISPLAY_PACKETS:
                update_interface_label(interface_collection->network_rx_label, curr->rx_packets, rxp_diff, iface->buffer.count - 1, iface->diff_sma.rx_packets, RTNL_LINK_RX_PACKETS);
                update_interface_label(interface_collection->network_tx_label, curr->tx_packets, txp_diff, iface->buffer.count - 1, iface->diff_sma.tx_packets, RTNL_LINK_TX_PACKETS);
                break;
            case VX_DISPLAY_DROPPED:
                update_interface_label(interface_collection->network_rx_label, curr->rx_dropped, rxd_diff, iface->buffer.count - 1, iface->diff_sma.rx_dropped, RTNL_LINK_RX_DROPPED);
                update_interface_label(interface_collection->network_tx_label, curr->tx_dropped, txd_diff, iface->buffer.count - 1, iface->diff_sma.tx_dropped, RTNL_LINK_TX_DROPPED);
            }
        } else {
            for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
                if (selector.selected == vlan) {
                    switch (selector.display_mode) {

                    case VX_DISPLAY_BYTES:
                        update_interface_label(interface_collection->network_rx_label, vlan->buffer.data[curr_i].rx_bytes, rx_diff, vlan->buffer.count - 1, vlan->diff_sma.rx_bytes, RTNL_LINK_RX_BYTES);
                        update_interface_label(interface_collection->network_tx_label, vlan->buffer.data[curr_i].tx_bytes, 0, 0, 0, -1);
                        break;
                    case VX_DISPLAY_PACKETS:
                        update_interface_label(interface_collection->network_rx_label, vlan->buffer.data[curr_i].rx_packets, rxp_diff, vlan->buffer.count - 1, vlan->diff_sma.rx_packets, RTNL_LINK_RX_PACKETS);
                        update_interface_label(interface_collection->network_tx_label, vlan->buffer.data[curr_i].tx_packets, 0, 0, 0, -1);
                        break;
                    case VX_DISPLAY_DROPPED:
                        update_interface_label(interface_collection->network_rx_label, vlan->buffer.data[curr_i].rx_dropped, rxd_diff, vlan->buffer.count - 1, vlan->diff_sma.rx_dropped, RTNL_LINK_RX_DROPPED);
                        update_interface_label(interface_collection->network_tx_label, vlan->buffer.data[curr_i].tx_dropped, 0, 0, 0, -1);
                    }
                }
            }
        }
    }

    // Output
    for (Interface* iface = interface_collection->output_head; iface != NULL; iface = iface->next) {
        int curr_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1),
            prev_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE - 1) % (VX_NETWORK_CHART_SIZE + 1);
        InterfaceStats *curr = &iface->buffer.data[curr_i],
                       *prev = &iface->buffer.data[prev_i],
                       *vcurr = NULL,
                       *vprev = NULL;
        // Bytes series
        uint64_t rx_diff = curr->rx_bytes - prev->rx_bytes;
        uint64_t tx_diff = curr->tx_bytes - prev->tx_bytes;
        uint64_t bytes_diff_max = (iface->diff_max.rx_bytes > iface->diff_max.tx_bytes ?
            iface->diff_max.rx_bytes :
            iface->diff_max.tx_bytes
        );

        uint64_t rxv_diff;

        int shift_amount = highest_set_bit_position(bytes_diff_max) - VX_NETWORK_CHART_RANGE_SHIFT_MAX + 1;
        if (shift_amount < 0)
            shift_amount = 0;

        // Scale changed, reset all series for interface
        if (shift_amount != iface->bytes_scale) {
            lv_chart_set_all_value(interface_collection->network_chart, iface->rx_bytes, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->tx_bytes, LV_CHART_POINT_NONE);

            int start = (iface->buffer.head + VX_NETWORK_CHART_SIZE + 1 - iface->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
            for (int i = 0; i < (iface->buffer.count - 1); i++) {
                InterfaceStats *curr = &iface->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
                               *prev = &iface->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                uint64_t rx_diff = curr->rx_bytes - prev->rx_bytes;
                uint64_t tx_diff = curr->tx_bytes - prev->tx_bytes;
            
                lv_chart_set_next_value(interface_collection->network_chart, iface->rx_bytes,  (rx_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->tx_bytes, -(tx_diff >> shift_amount));
            }
            iface->bytes_scale = shift_amount;
        // Same shift, simple insert
        } else {
            lv_chart_set_next_value(interface_collection->network_chart, iface->rx_bytes,  (rx_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->tx_bytes, -(tx_diff >> shift_amount));
        }

        // Packets/dropped series
        uint64_t rxp_diff = curr->rx_packets - prev->rx_packets;
        uint64_t rxd_diff = curr->rx_dropped - prev->rx_dropped;
        uint64_t txp_diff = curr->tx_packets - prev->tx_packets;
        uint64_t txd_diff = curr->tx_dropped - prev->tx_dropped;
        uint64_t xxp_max = (iface->diff_max.rx_packets > iface->diff_max.tx_packets ? iface->diff_max.rx_packets : iface->diff_max.tx_packets);
        uint64_t xxd_max = (iface->diff_max.rx_dropped > iface->diff_max.tx_dropped ? iface->diff_max.rx_dropped : iface->diff_max.tx_dropped);
        uint64_t packets_diff_max = (xxd_max > xxp_max ? xxd_max : xxp_max);

        shift_amount = highest_set_bit_position(packets_diff_max) - VX_NETWORK_CHART_RANGE_SHIFT_MAX + 1;
        if (shift_amount < 0)
            shift_amount = 0;
        if (shift_amount != iface->packets_scale) {
            lv_chart_set_all_value(interface_collection->network_chart, iface->rx_packets, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->rx_dropped, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->tx_packets, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, iface->tx_dropped, LV_CHART_POINT_NONE);
            
            int start = (iface->buffer.head + VX_NETWORK_CHART_SIZE + 1 - iface->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
            for (int i = 0; i < (iface->buffer.count - 1); i++) {
                InterfaceStats *curr = &iface->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
                               *prev = &iface->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                uint64_t rxp_diff = curr->rx_packets - prev->rx_packets;
                uint64_t rxd_diff = curr->rx_dropped - prev->rx_dropped;
                uint64_t txp_diff = curr->tx_packets - prev->tx_packets;
                uint64_t txd_diff = curr->tx_dropped - prev->tx_dropped;
                
                lv_chart_set_next_value(interface_collection->network_chart, iface->rx_packets,  (rxp_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->rx_dropped,  (rxd_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->tx_packets, -(txp_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, iface->tx_dropped, -(txd_diff >> shift_amount));
            }
            iface->packets_scale = shift_amount;
        // Same shift, simple insert
        } else {
            lv_chart_set_next_value(interface_collection->network_chart, iface->rx_packets,  (rxp_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->rx_dropped,  (rxd_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->tx_packets, -(txp_diff >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, iface->tx_dropped, -(txd_diff >> shift_amount));
        }

        // Labels
        if (selector.selected == iface) {
            switch (selector.display_mode) {

            case VX_DISPLAY_BYTES:
                update_interface_label(interface_collection->network_rx_label, curr->rx_bytes, rx_diff, iface->buffer.count, iface->diff_sma.rx_bytes, RTNL_LINK_RX_BYTES);
                update_interface_label(interface_collection->network_tx_label, curr->tx_bytes, tx_diff, iface->buffer.count, iface->diff_sma.tx_bytes, RTNL_LINK_TX_BYTES);
                break;
            case VX_DISPLAY_PACKETS:
                update_interface_label(interface_collection->network_rx_label, curr->rx_packets, rxp_diff, iface->buffer.count, iface->diff_sma.rx_packets, RTNL_LINK_RX_PACKETS);
                update_interface_label(interface_collection->network_tx_label, curr->tx_packets, txp_diff, iface->buffer.count, iface->diff_sma.tx_packets, RTNL_LINK_TX_PACKETS);
                break;
            case VX_DISPLAY_DROPPED:
                update_interface_label(interface_collection->network_rx_label, curr->rx_dropped, rxd_diff, iface->buffer.count, iface->diff_sma.rx_dropped, RTNL_LINK_RX_DROPPED);
                update_interface_label(interface_collection->network_tx_label, curr->tx_dropped, txd_diff, iface->buffer.count, iface->diff_sma.tx_dropped, RTNL_LINK_TX_DROPPED);
            }
        }
    }
    return 0;
}

void interfaces_chart_change_visibility() {
    // hide all
    Interface* interface = interface_collection->input_head;
    while (interface) {
        Interface_set_focus(interface, false, false);
        Vlan* vlan = interface->vlan_stats;
        while (vlan) {
            Vlan_set_focus(vlan, false, false);
            vlan = vlan->next;
        }
        interface = interface->next;
    }
    interface = interface_collection->output_head;
    while (interface) {
        Interface_set_focus(interface, false, false);
        interface = interface->next;
    }

    // show selected
    interface = (Interface*)selector.selected;
    Vlan* vlan = (Vlan*)selector.selected;
    switch (interface->type) {

    case VX_CLASS_INPUT_INTERFACE:
        Interface_set_focus(interface, true, selector.display_mode);
        vlan = interface->vlan_stats;
        while (vlan) {
            if (vlan->line)
                lv_obj_set_style_line_opa(vlan->line, LV_OPA_100, 0);
            if (vlan->redirection)
                lv_obj_set_style_image_opa(vlan->redirection->image, LV_OPA_100, 0);
            vlan = vlan->next;
        }
        lv_label_set_text_fmt(interface_collection->network_label, "\uf053 %s bandwidth \uf054", interface->interface_name);
        break;
    case VX_CLASS_OUTPUT_INTERFACE:
        Interface_set_focus(interface, true, selector.display_mode);
        Interface* iface = interface_collection->input_head;
        while (iface) {
            vlan = iface->vlan_stats;
            while (vlan) {
                if (vlan->redirection == interface) {
                    lv_obj_set_style_line_opa(vlan->line, LV_OPA_100, 0);
                    lv_obj_set_style_image_opa(vlan->parent->image, LV_OPA_100, 0);
                }
                vlan = vlan->next;
            }
            iface = iface->next;
        }
        lv_label_set_text_fmt(interface_collection->network_label, "\uf053 %s bandwidth \uf054", interface->interface_name);
        break;
    case VX_CLASS_VLAN:
        Vlan_set_focus(vlan, true, selector.display_mode);
        lv_obj_set_style_image_opa(vlan->parent->image, LV_OPA_100, 0);
        if (vlan->redirection)
            lv_obj_set_style_image_opa(vlan->redirection->image, LV_OPA_100, 0);
        lv_label_set_text_fmt(interface_collection->network_label, "\uf053 %s.%d bandwidth \uf054", vlan->parent->interface_name, vlan->vlan_id);
        break;
    }
}

int cpus_chart_update(CpuCollection* collection) {
    if (collect_cpus_data(collection) < 0)
        return -1;
    Cpu* cpu = NULL;
    cpu = collection->head;
    while (cpu) {
        lv_chart_set_next_value(collection->cpus_chart, cpu->cpu_load, cpu->buffer.data[(cpu->buffer.head + VX_CPU_CHART_SIZE) % (VX_CPU_CHART_SIZE + 1)]);
        lv_label_set_text_fmt(cpu->cpu_label, "CPU%d: %d%%", cpu->id, cpu->buffer.data[(cpu->buffer.head + VX_CPU_CHART_SIZE) % (VX_CPU_CHART_SIZE + 1)]);
        cpu = cpu->next;
    }
    return 0;
}

int memory_chart_update(MemoryCollection* collection) {
    if (collect_memory_data(collection) < 0)
        return -1;
    Memory* memory = NULL;
    memory = collection->head;
    while (memory) {
        uint64_t load = memory->buffer.data[(memory->buffer.head + VX_MEMORY_CHART_SIZE) % (VX_MEMORY_CHART_SIZE + 1)];
        double percent_load = (collection->total > 0 ? (double)load / collection->total * 100.0 : 0.0);
        char* str_load = calculate_size(load);
        lv_chart_set_next_value(collection->memory_chart, memory->memory_load, percent_load);
        lv_label_set_text_fmt(memory->memory_label, "%.32s: %s (%.2f%%, %lu)", memory->name, str_load, percent_load, load);
        free(str_load);
        memory = memory->next;
    }
    return 0;
}