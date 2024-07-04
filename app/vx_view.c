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
        lv_obj_del(input_header);
        return -1;
    }
    lv_label_set_text(title, "  "VX_TITLE" "VX_VERSION);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, VX_WHITE_COLOR, 0);

    lv_obj_t* output_header = lv_obj_create(scr);
    if (!output_header) {
        perror("lv_obj_create allocation failed");
        lv_obj_del(title);
        lv_obj_del(input_header);
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
        lv_obj_del(output_header);
        lv_obj_del(title);
        lv_obj_del(input_header);
        return -1;
    }
    lv_obj_set_size(footer, 800, 32);
    lv_obj_set_pos(footer, 0, 568);
    lv_obj_set_style_bg_color(footer, VX_BLUE_PALETTE, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    return 0;
}

int  interfaces_chart_change_visibility();
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
            exit(EXIT_FAILURE);
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
                        Interface_set_focus(vlan->parent, true, selector.display_mode);
                        interface = (vlan->parent->next ? vlan->parent->next : vlan->parent->parent->output_head);
                        break;
                    }
                    selector.selected = (void*)interface;
                    if (interfaces_chart_change_visibility() < 0)
                        exit(EXIT_FAILURE);
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
                        Interface_set_focus(vlan->parent, true, selector.display_mode);
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
                    if (interfaces_chart_change_visibility() < 0)
                        exit(EXIT_FAILURE);
                    break;
                case KEY_UP:
                    if (vlan->type == VX_CLASS_VLAN) {
                        if (vlan->prev) {
                            selector.selected = (void*)vlan->prev;
                        } else {
                            selector.selected = (void*)vlan->parent;
                        }
                        if (interfaces_chart_change_visibility() < 0)
                            exit(EXIT_FAILURE);
                    }
                    break;
                case KEY_DOWN:
                    if (vlan->type == VX_CLASS_VLAN) {
                        if (vlan->next) {
                            selector.selected = (void*)vlan->next;
                            if (interfaces_chart_change_visibility() < 0)
                                exit(EXIT_FAILURE);
                        }
                    } else if (interface->type == VX_CLASS_INPUT_INTERFACE) {
                        if (interface->vlan_stats) {
                            selector.selected = (void*)interface->vlan_stats;
                            if (interfaces_chart_change_visibility() < 0)
                                exit(EXIT_FAILURE);
                        }
                    }
                    break;
                case KEY_B:
                    if (selector.display_mode != VX_DISPLAY_BYTES) {
                        selector.display_mode = VX_DISPLAY_BYTES;
                        if (interfaces_chart_change_visibility() < 0)
                            exit(EXIT_FAILURE);
                    }
                    break;
                case KEY_P:
                    if (selector.display_mode != VX_DISPLAY_PACKETS) {
                        selector.display_mode = VX_DISPLAY_PACKETS;
                        if (interfaces_chart_change_visibility() < 0)
                            exit(EXIT_FAILURE);
                    }
                }
                pthread_mutex_unlock(&main_mutex);
            }
        }
    }
    ioctl(fd, EVIOCGRAB, (void*)0);
}


void interfaces_refresh() {
    Interface* iface = interface_collection->input_head;
    Vlan*      vlan  = NULL;

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

static int update_interface_label(lv_obj_t* label, Interface* iface, uint64_t val, vx_display flag) {
    char *str_sma, *str_val, *str_diff;
    uint64_t sma = 0, diff = 0, count = 0;
    switch (flag) {
    case VX_NONE:
        lv_label_set_text(label, "");
        return 0;
    case VX_RX_BYTES: sma = iface->diff_sma.rx_bytes; diff = iface->diff.rx_bytes; count = iface->buffer.count; break;
    case VX_TX_BYTES: sma = iface->diff_sma.tx_bytes; diff = iface->diff.tx_bytes; count = iface->buffer.count; break;
    case VX_RX_DROPPED_BYTES:  sma = iface->diff_sma.rx_dropped_bytes; diff = iface->diff.rx_dropped_bytes; count = iface->buffer.count; break;
    case VX_RX_PACKETS: sma = iface->diff_sma.rx_packets; diff = iface->diff.rx_packets; count = iface->buffer.count; break;
    case VX_TX_PACKETS: sma = iface->diff_sma.tx_packets; diff = iface->diff.tx_packets; count = iface->buffer.count; break;
    case VX_RX_DROPPED: sma = iface->diff_sma.rx_dropped; diff = iface->diff.rx_dropped; count = iface->buffer.count; break;
    case VX_TX_DROPPED: sma = iface->diff_sma.tx_dropped; diff = iface->diff.tx_dropped; count = iface->buffer.count; break;
    }
    switch (flag) {
    case VX_RX_BYTES:
    case VX_TX_BYTES:
    case VX_RX_DROPPED_BYTES:
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
    case VX_RX_BYTES:
        lv_label_set_text_fmt(label, "Total Rx: %s | Rx byte/s: %s/s | Rx byte/s (avg. last %lds) %s/s", str_val, str_diff, count - 1, str_sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_RX_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_RX_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_RX_COLOR, 0); break;
        }
        break;
    case VX_TX_BYTES:
        lv_label_set_text_fmt(label, "Total Tx: %s | Tx byte/s: %s/s | Tx byte/s (avg. last %lds) %s/s", str_val, str_diff, count - 1, str_sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_TX_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_TX_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_TX_COLOR, 0); break;
        }
        break;
    case VX_RX_DROPPED_BYTES:
        lv_label_set_text_fmt(label, "Dropped Rx: %s | Rx byte/s: %s/s | Rx byte/s (avg. last %lds) %s/s", str_val, str_diff, count - 1, str_sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_RXD_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_RXD_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_RXD_COLOR, 0); break;
        }
        break;
    // Packets
    case VX_RX_PACKETS:
        lv_label_set_text_fmt(label, "Total Rx pkt: %"PRIu64" | Rx pkt/s: %"PRIu64"/s | Rx pkt/s (avg. last %lds) %"PRIu64"/s", val, diff, count - 1, sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_RX_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_RX_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_RX_COLOR, 0); break;
        }
        break;
    case VX_TX_PACKETS:
        lv_label_set_text_fmt(label, "Total Tx pkt: %"PRIu64" | Tx pkt/s: %"PRIu64"/s | Tx pkt/s (avg. last %lds) %"PRIu64"/s", val, diff, count - 1, sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_TX_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_TX_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_TX_COLOR, 0); break;
        }
        break;
    case VX_RX_DROPPED:
        lv_label_set_text_fmt(label, "Total Rx drop: %"PRIu64" | Rx drop/s: %"PRIu64"/s | Rx drop/s (avg. last %lds) %"PRIu64"/s", val, diff, count - 1, sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_RXD_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_RXD_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_RXD_COLOR, 0); break;
        }
        break;
    case VX_TX_DROPPED:
        lv_label_set_text_fmt(label, "Total Tx drop: %"PRIu64" | Tx drop/s: %"PRIu64"/s | Tx drop/s (avg. last %lds) %"PRIu64"/s", val, diff, count - 1, sma);
        switch (iface->type) {
        case VX_CLASS_INPUT_INTERFACE:  lv_obj_set_style_text_color(label, VX_INPUT_TXD_COLOR, 0); break;
        case VX_CLASS_OUTPUT_INTERFACE: lv_obj_set_style_text_color(label, VX_OUTPUT_TXD_COLOR, 0); break;
        case VX_CLASS_VLAN:             lv_obj_set_style_text_color(label, VX_VLAN_TXD_COLOR, 0); break;
        }
        break;
    }
    switch (flag) {
    case VX_RX_BYTES:
    case VX_TX_BYTES:
        free(str_sma);
        free(str_val);
        free(str_diff);
        break;
    }
    return 0;
}

int interface_series_update(Interface* iface, int type) {
    int curr_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1),
        prev_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE - 1) % (VX_NETWORK_CHART_SIZE + 1);
    InterfaceStats *curr = &iface->buffer.data[curr_i],
                   *prev = &iface->buffer.data[prev_i],
                   *vcurr = NULL,
                   *vprev = NULL;

    uint64_t max = 0;
    int* scale;

    // Interfaces
    lv_chart_series_t *r_series,  *t_series,  *rd_series,  *td_series;
    switch (type) {
    case VX_DISPLAY_BYTES:
        max = (iface->diff_max.rx_bytes > iface->diff_max.tx_bytes ?
            iface->diff_max.rx_bytes : iface->diff_max.tx_bytes
        );
        scale = &iface->bytes_scale;
        r_series  = iface->rx_bytes;
        t_series  = iface->tx_bytes;
        rd_series = NULL;
        td_series = NULL;
        break;
    case VX_DISPLAY_PACKETS:
        max = (iface->diff_max.rx_packets > iface->diff_max.tx_packets ?
            iface->diff_max.rx_packets > iface->diff_max.rx_dropped ?
                iface->diff_max.rx_packets : iface->diff_max.rx_dropped
        :   iface->diff_max.tx_packets > iface->diff_max.rx_dropped ?
                iface->diff_max.tx_packets : iface->diff_max.rx_dropped
        );
        scale = &iface->packets_scale;
        r_series  = iface->rx_packets;
        t_series  = iface->tx_packets;
        rd_series = iface->rx_dropped;
        td_series = iface->tx_dropped;
        break;
    }

    int shift_amount = highest_set_bit_position(max) - VX_NETWORK_CHART_RANGE_SHIFT_MAX + 1;
    if (shift_amount < 0)
        shift_amount = 0;

    // Scale changed, reset all series for interface
    if (shift_amount != *scale) {
        lv_chart_set_all_value(interface_collection->network_chart, r_series, LV_CHART_POINT_NONE);
        lv_chart_set_all_value(interface_collection->network_chart, t_series, LV_CHART_POINT_NONE);

        int start = (iface->buffer.head + VX_NETWORK_CHART_SIZE + 1 - iface->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
        for (int i = 0; i < (iface->buffer.count - 1); i++) {
            InterfaceStats *curr = &iface->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)],
                           *prev = &iface->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];

            uint64_t r_diff;
            uint64_t t_diff;
            switch (type) {
            case VX_DISPLAY_BYTES:
                r_diff = curr->rx_bytes - prev->rx_bytes;
                t_diff = curr->tx_bytes - prev->tx_bytes;
                lv_chart_set_next_value(interface_collection->network_chart, r_series,  (r_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, t_series, -(t_diff >> shift_amount));
                break;
            case VX_DISPLAY_PACKETS:
                r_diff = curr->rx_packets - prev->rx_packets;
                t_diff = curr->tx_packets - prev->tx_packets;
                lv_chart_set_next_value(interface_collection->network_chart, r_series,  (r_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, t_series, -(t_diff >> shift_amount));
                r_diff = curr->rx_dropped - prev->rx_dropped;
                t_diff = curr->tx_dropped - prev->tx_dropped;
                lv_chart_set_next_value(interface_collection->network_chart, rd_series,  (r_diff >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, td_series, -(t_diff >> shift_amount));
                break;
            }
        }

        *scale = shift_amount;
    // Same shift, simple insert
    } else {
        switch (type) {
        case VX_DISPLAY_BYTES:
            lv_chart_set_next_value(interface_collection->network_chart, r_series,  (iface->diff.rx_bytes >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, t_series, -(iface->diff.tx_bytes >> shift_amount));
            break;
        case VX_DISPLAY_PACKETS:
            lv_chart_set_next_value(interface_collection->network_chart, r_series,  (iface->diff.rx_packets >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, t_series, -(iface->diff.tx_packets >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, rd_series,  (iface->diff.rx_dropped >> shift_amount));
            lv_chart_set_next_value(interface_collection->network_chart, td_series, -(iface->diff.tx_dropped >> shift_amount));
            break;
        }
    }

    // Vlans
    for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
        lv_chart_series_t *vr_series, *vrd_series;
        switch (type) {
        case VX_DISPLAY_BYTES:
            max = (vlan->diff_max.rx_bytes > vlan->diff_max.rx_dropped_bytes ?
                vlan->diff_max.rx_bytes : vlan->diff_max.rx_dropped_bytes
            );
            scale = &vlan->bytes_scale; 
            vr_series  = vlan->rx_bytes;
            vrd_series = vlan->rx_dropped_bytes;
            break;
        case VX_DISPLAY_PACKETS:
            max = (vlan->diff_max.rx_packets > vlan->diff_max.rx_dropped ?
                vlan->diff_max.rx_packets : vlan->diff_max.rx_dropped
            );
            scale = &vlan->packets_scale; 
            vr_series  = vlan->rx_packets;
            vrd_series = vlan->rx_dropped;
            break;
        }

        int shift_amount = highest_set_bit_position(max) - VX_NETWORK_CHART_RANGE_SHIFT_MAX + 1;
        if (shift_amount < 0)
            shift_amount = 0;

        // Scale changed, reset all series for interface
        if (shift_amount != *scale) {
            lv_chart_set_all_value(interface_collection->network_chart, vr_series, LV_CHART_POINT_NONE);
            lv_chart_set_all_value(interface_collection->network_chart, vrd_series, LV_CHART_POINT_NONE);

            int start = (vlan->buffer.head + VX_NETWORK_CHART_SIZE + 1 - vlan->buffer.count) % (VX_NETWORK_CHART_SIZE + 1);
            int vcurr_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1),
                vprev_i = (iface->buffer.head + VX_NETWORK_CHART_SIZE - 1) % (VX_NETWORK_CHART_SIZE + 1);
            for (int i = 0; i < (vlan->buffer.count - 1); i++) {
                vcurr = &vlan->buffer.data[(start + i + 1) % (VX_NETWORK_CHART_SIZE + 1)];
                vprev = &vlan->buffer.data[(start + i) % (VX_NETWORK_CHART_SIZE + 1)];
                uint64_t vr_diff;
                switch (type) {
                case VX_DISPLAY_BYTES:
                    vr_diff = vcurr->rx_bytes - vprev->rx_bytes;
                    lv_chart_set_next_value(interface_collection->network_chart, vr_series, (vr_diff >> shift_amount));
                    vr_diff = vcurr->rx_dropped_bytes - vprev->rx_dropped_bytes;
                    lv_chart_set_next_value(interface_collection->network_chart, vrd_series, (vr_diff >> shift_amount));
                    break;
                case VX_DISPLAY_PACKETS:
                    vr_diff = vcurr->rx_packets - vprev->rx_packets;
                    lv_chart_set_next_value(interface_collection->network_chart, vr_series, (vr_diff >> shift_amount));
                    vr_diff = vcurr->rx_dropped - vprev->rx_dropped;
                    lv_chart_set_next_value(interface_collection->network_chart, vrd_series, (vr_diff >> shift_amount));
                    break;
                }
            }
        } else {
            switch (type) {
            case VX_DISPLAY_BYTES:
                lv_chart_set_next_value(interface_collection->network_chart, vr_series, (vlan->diff.rx_bytes >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, vrd_series, (vlan->diff.rx_dropped_bytes >> shift_amount));
                break;
            case VX_DISPLAY_PACKETS:
                lv_chart_set_next_value(interface_collection->network_chart, vr_series, (vlan->diff.rx_packets >> shift_amount));
                lv_chart_set_next_value(interface_collection->network_chart, vrd_series, (vlan->diff.rx_dropped >> shift_amount));
                break;
            }
        }
    }
    return curr_i;
}

int interfaces_chart_update() {
    if (collect_interfaces_data(interface_collection) < 0)
        return -1;

    // Input
    int curr_i = 0;
    for (Interface* iface = interface_collection->input_head; iface != NULL; iface = iface->next) {
        interface_series_update(iface, VX_DISPLAY_BYTES);
        curr_i = interface_series_update(iface, VX_DISPLAY_PACKETS);
        InterfaceStats *curr = &iface->buffer.data[curr_i];

        // Labels
        if (selector.selected == iface) {
            switch (selector.display_mode) {
            case VX_DISPLAY_BYTES:
                update_interface_label(interface_collection->network_rx_label, iface, curr->rx_bytes, VX_RX_BYTES);
                update_interface_label(interface_collection->network_tx_label, iface, curr->tx_bytes, VX_TX_BYTES);
                update_interface_label(interface_collection->network_rxd_label, iface, 0, VX_NONE);
                update_interface_label(interface_collection->network_txd_label, iface, 0, VX_NONE);
                break;
            case VX_DISPLAY_PACKETS:
                update_interface_label(interface_collection->network_rx_label, iface, curr->rx_packets, VX_RX_PACKETS);
                update_interface_label(interface_collection->network_tx_label, iface, curr->tx_packets, VX_TX_PACKETS);
                update_interface_label(interface_collection->network_rxd_label, iface, curr->rx_dropped, VX_RX_DROPPED);
                update_interface_label(interface_collection->network_txd_label, iface, curr->tx_dropped, VX_TX_DROPPED);
            }
        } else {
            for (Vlan* vlan = iface->vlan_stats; vlan != NULL; vlan = vlan->next) {
                int curr_i = (vlan->buffer.head + VX_NETWORK_CHART_SIZE) % (VX_NETWORK_CHART_SIZE + 1);
                if (selector.selected == vlan) {
                    switch (selector.display_mode) {
                    case VX_DISPLAY_BYTES:
                        update_interface_label(interface_collection->network_rx_label, (Interface*)vlan, vlan->buffer.data[curr_i].rx_bytes, VX_RX_BYTES);
                        update_interface_label(interface_collection->network_tx_label, (Interface*)vlan, 0, VX_NONE);
                        update_interface_label(interface_collection->network_rxd_label, (Interface*)vlan, vlan->buffer.data[curr_i].rx_dropped_bytes, VX_RX_DROPPED_BYTES);
                        update_interface_label(interface_collection->network_txd_label, (Interface*)vlan, 0, VX_NONE);
                        break;
                    case VX_DISPLAY_PACKETS:
                        update_interface_label(interface_collection->network_rx_label, (Interface*)vlan, vlan->buffer.data[curr_i].rx_packets, VX_RX_PACKETS);
                        update_interface_label(interface_collection->network_tx_label, (Interface*)vlan, 0, VX_NONE);
                        update_interface_label(interface_collection->network_rxd_label, (Interface*)vlan, vlan->buffer.data[curr_i].rx_dropped, VX_RX_DROPPED);
                        update_interface_label(interface_collection->network_txd_label, (Interface*)vlan, 0, VX_NONE);
                    }
                }
            }
        }
    }

    // Output
    for (Interface* iface = interface_collection->output_head; iface != NULL; iface = iface->next) {
        interface_series_update(iface, VX_DISPLAY_BYTES);
        curr_i = interface_series_update(iface, VX_DISPLAY_PACKETS);
        InterfaceStats *curr = &iface->buffer.data[curr_i];

        // Labels
        if (selector.selected == iface) {
            switch (selector.display_mode) {
            case VX_DISPLAY_BYTES:
                update_interface_label(interface_collection->network_rx_label, iface, curr->rx_bytes, VX_RX_BYTES);
                update_interface_label(interface_collection->network_tx_label, iface, curr->tx_bytes, VX_TX_BYTES);
                update_interface_label(interface_collection->network_rxd_label, iface, 0, VX_NONE);
                update_interface_label(interface_collection->network_txd_label, iface, 0, VX_NONE);
                break;
            case VX_DISPLAY_PACKETS:
                update_interface_label(interface_collection->network_rx_label, iface, curr->rx_packets, VX_RX_PACKETS);
                update_interface_label(interface_collection->network_tx_label, iface, curr->tx_packets, VX_TX_PACKETS);
                update_interface_label(interface_collection->network_rxd_label, iface, curr->rx_dropped, VX_RX_DROPPED);
                update_interface_label(interface_collection->network_txd_label, iface, curr->tx_dropped, VX_TX_DROPPED);
            }
        }
    }
    return 0;
}

int interfaces_chart_change_visibility() {
    // hide all
    Interface* interface = interface_collection->input_head;
    while (interface) {
        if(Interface_set_focus(interface, false, false) < 0)
            return -1;
        Vlan* vlan = interface->vlan_stats;
        while (vlan) {
            if (Vlan_set_focus(vlan, false, false) < 0)
                return -1;
            vlan = vlan->next;
        }
        interface = interface->next;
    }
    interface = interface_collection->output_head;
    while (interface) {
        if (Interface_set_focus(interface, false, false) < 0)
            return -1;
        interface = interface->next;
    }

    // show selected
    interface = (Interface*)selector.selected;
    Vlan* vlan = (Vlan*)selector.selected;
    switch (interface->type) {

    case VX_CLASS_INPUT_INTERFACE:
        if (Interface_set_focus(interface, true, selector.display_mode) < 0)
            return -1;
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
        if (Interface_set_focus(interface, true, selector.display_mode) < 0)
            return -1;
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
        if (Vlan_set_focus(vlan, true, selector.display_mode) < 0)
            return -1;
        lv_obj_set_style_image_opa(vlan->parent->image, LV_OPA_100, 0);
        if (vlan->redirection)
            lv_obj_set_style_image_opa(vlan->redirection->image, LV_OPA_100, 0);
        lv_label_set_text_fmt(interface_collection->network_label, "\uf053 %s.%d bandwidth \uf054", vlan->parent->interface_name, vlan->vlan_id);
        break;
    }
    return 0;
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
