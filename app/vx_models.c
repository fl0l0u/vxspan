#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <linux/types.h>
#include <net/if.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <errno.h>

#include "lvgl/lvgl.h"

#include "vx_models.h"
#include "vx_config.h"
#include "vx_network.h"
#include "vx_utils.h"

// Interfaces
InterfaceCollection* init_interfaces() {
    InterfaceCollection* collection = malloc(sizeof(InterfaceCollection));
    if (!collection) {
        perror("malloc failed");
        return NULL;
    }
    collection->input_head  = NULL;
    collection->output_head = NULL;
    collection->input_count  = 0;
    collection->output_count = 0;

    // Chart
    lv_obj_t *network_label = lv_label_create(lv_scr_act());
    if (!network_label) {
        perror("lv_label_create allocation failed");
        free(collection);
        return NULL;
    }
    lv_obj_set_size(network_label, 800, 16);
    lv_obj_set_style_text_align(network_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(network_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(network_label, -1, 0);
    lv_obj_set_style_text_color(network_label, VX_WHITE_COLOR, 0);
    lv_obj_set_pos(network_label, 0, 202);
    collection->network_label = network_label;

    lv_obj_t *network_chart = lv_chart_create(lv_scr_act());
    if (!network_chart) {
        perror("lv_chart_create allocation failed");
        lv_obj_del(network_label);
        free(collection);
        return NULL;
    }
    lv_obj_set_size(network_chart, 800, 192);
    lv_obj_align(network_chart, LV_ALIGN_TOP_MID, 0, 220);
    lv_chart_set_type(network_chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_line_width(network_chart, 4, LV_PART_INDICATOR);
    lv_chart_set_range(network_chart, LV_CHART_AXIS_PRIMARY_Y, -(int32_t)(1.25 * VX_NETWORK_CHART_RANGE), (int32_t)(1.25 * VX_NETWORK_CHART_RANGE));
    lv_chart_set_div_line_count(network_chart, 3, 0);
    lv_obj_set_style_size(network_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(network_chart, 0, 0);
    lv_obj_set_style_pad_all(network_chart, 0, 0);
    lv_chart_set_point_count(network_chart, VX_NETWORK_CHART_SIZE);
    collection->network_chart = network_chart;

    lv_obj_t *network_rx_label = lv_label_create(lv_scr_act());
    if (!network_rx_label) {
        perror("lv_label_create allocation failed");
        lv_obj_del(network_chart);
        lv_obj_del(network_label);
        free(collection);
        return NULL;
    }
    lv_obj_set_size(network_rx_label, 780, 16);
    lv_obj_set_style_text_align(network_rx_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(network_rx_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(network_rx_label, -1, 0);
    lv_obj_set_style_text_color(network_rx_label, VX_GREEN_PALETTE, 0);
    lv_obj_set_pos(network_rx_label, 8, 220 + 2);
    collection->network_rx_label = network_rx_label;

    lv_obj_t *network_tx_label = lv_label_create(lv_scr_act());
    if (!network_tx_label) {
        perror("lv_label_create allocation failed");
        lv_obj_del(network_rx_label);
        lv_obj_del(network_chart);
        lv_obj_del(network_label);
        free(collection);
        return NULL;
    }
    lv_obj_set_size(network_tx_label, 780, 16);
    lv_obj_set_style_text_align(network_tx_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(network_tx_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(network_tx_label, -1, 0);
    lv_obj_set_style_text_color(network_tx_label, VX_RED_PALETTE, 0);
    lv_obj_set_pos(network_tx_label, 8, 220 + 192 - 16 - 2);
    collection->network_tx_label = network_tx_label;

    return collection;
}

Interface* add_input_interface(InterfaceCollection* collection, int if_index, const char* interface_name) {
    Interface* new_interface = malloc(sizeof(Interface));
    if (!new_interface) {
        perror("malloc failed");
        return NULL;
    }
    new_interface->type = VX_CLASS_INPUT_INTERFACE;
    new_interface->parent = collection;
    new_interface->if_index = if_index;
    strncpy(new_interface->interface_name, interface_name, IFNAMSIZ);
    init_circular_buffer(&new_interface->buffer);
    new_interface->vlan_stats = NULL;
    new_interface->next = NULL;
    new_interface->prev = NULL;

    lv_obj_t* name = lv_label_create(lv_scr_act());
    if (!name) {
        perror("lv_label_create allocation failed");
        free(new_interface);
        return NULL;
    }
    lv_label_set_text(name, interface_name);
    lv_obj_set_size(name, 66, 16);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(name, -1, 0);
    lv_obj_set_style_text_color(name, VX_WHITE_COLOR, 0);
    new_interface->name = name;

    lv_obj_t* image = lv_img_create(lv_scr_act());
    if (!image) {
        perror("lv_img_create allocation failed");
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    lv_img_set_src(image, &png_image_dsc);
    lv_obj_set_style_border_post(image, true, 0);
    lv_obj_set_style_border_width(image, 4, 0);
    lv_obj_set_style_radius(image, 8, 0);
    new_interface->image = image;

    lv_obj_t* status = lv_label_create(lv_scr_act());
    if (!status) {
        perror("lv_label_create allocation failed");
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    lv_obj_set_size(status, 64, 16);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(status, -1, 0);
    new_interface->status = status;

    lv_obj_t* xdp_mode = lv_label_create(lv_scr_act());
    if (!xdp_mode) {
        perror("lv_chart_add_series allocation failed");
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    lv_obj_set_size(xdp_mode, 64, 16);
    lv_obj_set_style_text_align(xdp_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(xdp_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(xdp_mode, -1, 0);
    lv_obj_set_style_text_color(xdp_mode, VX_WHITE_COLOR, 0);
    lv_label_set_text(xdp_mode, "N/A");
    new_interface->xdp_mode = xdp_mode;

    lv_obj_set_pos(new_interface->name,     2 + collection->input_count * 68, 32);
    lv_obj_set_pos(new_interface->status,   2 + collection->input_count * 68, 48);
    lv_obj_set_pos(new_interface->image,   15 + collection->input_count * 68, 63);
    lv_obj_set_pos(new_interface->xdp_mode, 2 + collection->input_count * 68, 72);

    Interface_refresh(new_interface);

    lv_chart_series_t* rx_bytes = lv_chart_add_series(collection->network_chart, VX_GREEN_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!rx_bytes) {
        perror("lv_chart_add_series allocation failed");
        lv_obj_del(xdp_mode);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->rx_bytes = rx_bytes;

    lv_chart_series_t* rx_packets = lv_chart_add_series(collection->network_chart, VX_GREEN_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!rx_packets) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(xdp_mode);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->rx_packets = rx_packets;

    lv_chart_series_t* rx_dropped = lv_chart_add_series(collection->network_chart, VX_ORANGE_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!rx_dropped) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(xdp_mode);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->rx_dropped = rx_dropped;

    lv_chart_series_t* tx_bytes = lv_chart_add_series(collection->network_chart, VX_ORANGE_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!tx_bytes) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, rx_dropped);
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(xdp_mode);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->tx_bytes = tx_bytes;

    lv_chart_series_t* tx_packets = lv_chart_add_series(collection->network_chart, VX_ORANGE_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!tx_packets) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, tx_bytes);
        lv_chart_remove_series(collection->network_chart, rx_dropped);
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(xdp_mode);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->tx_packets = tx_packets;

    lv_chart_series_t* tx_dropped = lv_chart_add_series(collection->network_chart, VX_RED_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!tx_dropped) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, tx_packets);
        lv_chart_remove_series(collection->network_chart, tx_bytes);
        lv_chart_remove_series(collection->network_chart, rx_dropped);
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(xdp_mode);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->tx_dropped = tx_dropped;

    // Insert
    if (collection->input_head == NULL) {
        collection->input_head = new_interface;
    } else {
        Interface* current = collection->input_head;
        Interface* previous = NULL;

        while (current != NULL && current->if_index < if_index) {
            previous = current;
            current = current->next;
        }

        if (previous == NULL) {
            // Insert at the head
            new_interface->next = collection->input_head;
            collection->input_head->prev = new_interface;
            collection->input_head = new_interface;
        } else {
            // Insert in the middle or end
            new_interface->next = current;
            new_interface->prev = previous;
            previous->next = new_interface;
            if (current != NULL) {
                current->prev = new_interface;
            }
        }
    }

    if(collection->input_count > 0) {
        lv_chart_hide_series(collection->network_chart, rx_bytes, true);
        lv_chart_hide_series(collection->network_chart, rx_packets, true);
        lv_chart_hide_series(collection->network_chart, rx_dropped, true);
        lv_chart_hide_series(collection->network_chart, tx_bytes, true);
        lv_chart_hide_series(collection->network_chart, tx_packets, true);
        lv_chart_hide_series(collection->network_chart, tx_dropped, true);
    }

    collection->input_count++;
    return new_interface;
}

Interface* add_output_interface(InterfaceCollection* collection, int if_index, const char* interface_name) {
    Interface* new_interface = malloc(sizeof(Interface));
    if (!new_interface) {
        perror("malloc failed");
        return NULL;
    }
    new_interface->type = VX_CLASS_OUTPUT_INTERFACE;
    new_interface->parent = collection;
    new_interface->if_index = if_index;
    strncpy(new_interface->interface_name, interface_name, IFNAMSIZ);
    init_circular_buffer(&new_interface->buffer);
    new_interface->vlan_stats = NULL;
    new_interface->next = NULL;
    new_interface->prev = NULL;

    lv_obj_t* name = lv_label_create(lv_scr_act());
    if (!name) {
        perror("lv_label_create allocation failed");
        free(new_interface);
        return NULL;
    }
    lv_label_set_text(name, interface_name);
    lv_obj_set_size(name, 66, 16);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(name, -1, 0);
    lv_obj_set_style_text_color(name, VX_WHITE_COLOR, 0);
    new_interface->name = name;

    lv_obj_t* image = lv_img_create(lv_scr_act());
    if (!image) {
        perror("lv_img_create allocation failed");
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    lv_img_set_src(image, &png_image_dsc);
    lv_obj_set_style_border_post(image, true, 0);
    lv_obj_set_style_border_width(image, 4, 0);
    lv_obj_set_style_radius(image, 8, 0);
    new_interface->image = image;

    lv_obj_t* status = lv_label_create(lv_scr_act());
    if (!status) {
        perror("lv_label_create allocation failed");
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    lv_obj_set_size(status, 64, 16);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(status, -1, 0);
    new_interface->status = status;

    Interface_refresh(new_interface);

    lv_chart_series_t* rx_bytes = lv_chart_add_series(collection->network_chart, VX_GREEN_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!rx_bytes) {
        perror("lv_chart_add_series allocation failed");
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->rx_bytes = rx_bytes;

    lv_chart_series_t* rx_packets = lv_chart_add_series(collection->network_chart, VX_GREEN_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!rx_packets) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->rx_packets = rx_packets;

    lv_chart_series_t* rx_dropped = lv_chart_add_series(collection->network_chart, VX_RED_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!rx_dropped) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->rx_dropped = rx_dropped;

    lv_chart_series_t* tx_bytes = lv_chart_add_series(collection->network_chart, VX_ORANGE_PALETTE,   LV_CHART_AXIS_PRIMARY_Y);
    if (!tx_bytes) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, rx_dropped);
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->tx_bytes = tx_bytes;

    lv_chart_series_t* tx_packets = lv_chart_add_series(collection->network_chart, VX_ORANGE_PALETTE,   LV_CHART_AXIS_PRIMARY_Y);
    if (!tx_packets) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, tx_bytes);
        lv_chart_remove_series(collection->network_chart, rx_dropped);
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);
        return NULL;
    }
    new_interface->tx_packets = tx_packets;

    lv_chart_series_t* tx_dropped = lv_chart_add_series(collection->network_chart, VX_PURPLE_PALETTE,   LV_CHART_AXIS_PRIMARY_Y);
    if (!tx_dropped) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->network_chart, tx_packets);
        lv_chart_remove_series(collection->network_chart, tx_bytes);
        lv_chart_remove_series(collection->network_chart, rx_dropped);
        lv_chart_remove_series(collection->network_chart, rx_packets);
        lv_chart_remove_series(collection->network_chart, rx_bytes);
        lv_obj_del(status);
        lv_obj_del(image);
        lv_obj_del(name);
        free(new_interface);

        return NULL;
    }
    new_interface->tx_dropped = tx_dropped;
    if(collection->input_count > 0) {
        lv_chart_hide_series(collection->network_chart, rx_bytes, true);
        lv_chart_hide_series(collection->network_chart, rx_packets, true);
        lv_chart_hide_series(collection->network_chart, rx_dropped, true);
        lv_chart_hide_series(collection->network_chart, tx_bytes, true);
        lv_chart_hide_series(collection->network_chart, tx_packets, true);
        lv_chart_hide_series(collection->network_chart, tx_dropped, true);
    }

    // Insert
    if (collection->output_head == NULL) {
        collection->output_head = new_interface;
    } else {
        Interface* current = collection->output_head;
        Interface* previous = NULL;

        while (current != NULL && current->if_index < if_index) {
            previous = current;
            current = current->next;
        }

        if (previous == NULL) {
            // Insert at the head
            new_interface->next = collection->output_head;
            collection->output_head->prev = new_interface;
            collection->output_head = new_interface;
        } else {
            // Insert in the middle or end
            new_interface->next = current;
            new_interface->prev = previous;
            previous->next = new_interface;
            if (current != NULL) {
                current->prev = new_interface;
            }
        }
    }

    Interface* interface = collection->output_head;
    for (size_t i = 0; interface; i++) {
        OutputInterface_position(interface, collection->output_count - i);
        interface = interface->next;
    }

    collection->output_count++;
    return new_interface;
}

void Interface_up(Interface* interface) {
    lv_obj_set_style_border_color(interface->image, VX_GREEN_PALETTE, 0);
    lv_label_set_text(interface->status, "<UP>");
    lv_obj_set_style_text_color(interface->status, VX_GREEN_PALETTE, 0);
    interface->is_up = true;
}
void Interface_down(Interface* interface) {
    lv_obj_set_style_border_color(interface->image, VX_RED_PALETTE, 0);
    lv_label_set_text(interface->status, "<DOWN>");
    lv_obj_set_style_text_color(interface->status, VX_RED_PALETTE, 0);
    interface->is_up = false;
}
void Interface_refresh(Interface* interface) {
    if (interface)
        if (interface_is_up(interface->if_index)) {
            Interface_up(interface);
        } else {
            Interface_down(interface);
        }
}
void Interface_set_focus(Interface* interface, bool focus, vx_display_mode mode) {
    lv_obj_set_style_image_opa(interface->image, focus ? LV_OPA_100 : LV_OPA_50, 0);
    lv_obj_set_style_bg_opa(interface->name, !focus ? LV_OPA_0 : LV_OPA_50, 0);
    lv_chart_hide_series(interface->parent->network_chart, interface->rx_bytes,   !(focus && (mode == VX_DISPLAY_BYTES)));
    lv_chart_hide_series(interface->parent->network_chart, interface->tx_bytes,   !(focus && (mode == VX_DISPLAY_BYTES)));
    lv_chart_hide_series(interface->parent->network_chart, interface->rx_packets, !(focus && (mode == VX_DISPLAY_PACKETS)));
    lv_chart_hide_series(interface->parent->network_chart, interface->tx_packets, !(focus && (mode == VX_DISPLAY_PACKETS)));
    lv_chart_hide_series(interface->parent->network_chart, interface->rx_dropped, !(focus && (mode == VX_DISPLAY_DROPPED)));
    lv_chart_hide_series(interface->parent->network_chart, interface->tx_dropped, !(focus && (mode == VX_DISPLAY_DROPPED)));
}

void OutputInterface_position(Interface* interface, int i) {
    lv_obj_set_pos(interface->image,  745 - i * 68, 133);
    lv_obj_set_pos(interface->status, 732 - i * 68, 169);
    lv_obj_set_pos(interface->name,   732 - i * 68, 184);
}

void update_interface_data(Interface* interface, InterfaceStats interface_stats) {
    // Insert new values
    add_data_to_buffer(&interface->buffer, interface_stats);
    // Compute SMA
    interface_update_sma(interface);
}

Vlan* add_or_update_vlan(Interface* interface, int vlan_id) {
    Vlan* current = interface->vlan_stats;

    // Search for existing VLAN stats
    while (current != NULL) {
        if (current->vlan_id == vlan_id) {
            return current;
        }
        current = current->next;
    }

    // VLAN not found, create a new one
    Vlan* new_vlan = malloc(sizeof(Vlan));
    if (!new_vlan) {
        perror("malloc failed");
        return NULL;
    }
    new_vlan->type = VX_CLASS_VLAN;
    new_vlan->parent = interface;
    new_vlan->vlan_id = vlan_id;
    init_circular_buffer(&new_vlan->buffer);

    lv_chart_series_t* tmp_rx_bytes = lv_chart_add_series(interface->parent->network_chart, VX_GREEN_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!tmp_rx_bytes) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(interface->parent->network_chart, tmp_rx_bytes);
        free(new_vlan);
        return NULL;
    }
    new_vlan->rx_bytes = tmp_rx_bytes;

    lv_chart_series_t* tmp_rx_packets = lv_chart_add_series(interface->parent->network_chart, VX_GREEN_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!tmp_rx_packets) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(interface->parent->network_chart, tmp_rx_packets);
        lv_chart_remove_series(interface->parent->network_chart, tmp_rx_bytes);
        free(new_vlan);
        return NULL;
    }
    new_vlan->rx_packets = tmp_rx_packets;

    lv_chart_series_t* tmp_rx_dropped = lv_chart_add_series(interface->parent->network_chart, VX_ORANGE_PALETTE, LV_CHART_AXIS_PRIMARY_Y);
    if (!tmp_rx_dropped) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(interface->parent->network_chart, tmp_rx_dropped);
        lv_chart_remove_series(interface->parent->network_chart, tmp_rx_packets);
        lv_chart_remove_series(interface->parent->network_chart, tmp_rx_bytes);
        free(new_vlan);
        return NULL;
    }
    new_vlan->rx_dropped = tmp_rx_dropped;

    // Lookup redirection for VLAN on interface
    new_vlan->redirection = NULL;
    int redirection_index;
    if (bpf_map_lookup_elem(interface->vlan_redirect_map_fd, &vlan_id, &redirection_index) < 0)
        if (errno == ENOENT)
            redirection_index = -1;
        else {
            perror("bpf_map_lookup_elem");
            lv_chart_remove_series(interface->parent->network_chart, tmp_rx_dropped);
            lv_chart_remove_series(interface->parent->network_chart, tmp_rx_packets);
            lv_chart_remove_series(interface->parent->network_chart, tmp_rx_bytes);
            free(new_vlan);
            return NULL;
        }
    if (redirection_index > 0) {
        Interface* redirection = interface->parent->output_head;
        while (redirection != NULL) {
            if (redirection->if_index == redirection_index) {
                new_vlan->redirection = redirection;
                break;
            }
            redirection = redirection->next;
        }
    }

    if (new_vlan->parent && new_vlan->redirection) {
        new_vlan->line = lv_line_create(lv_scr_act());
        lv_obj_set_style_line_rounded(new_vlan->line, true, 0);
        lv_obj_set_style_line_width(new_vlan->line, 3, 0);

        Vlan_reposition(new_vlan);
        
        Vlan_refresh(new_vlan);
    }

    // Insert
    if (interface->vlan_stats == NULL) {
        interface->vlan_stats = new_vlan;
    } else {
        Vlan* current = interface->vlan_stats;
        Vlan* previous = NULL;

        while (current != NULL && current->vlan_id < vlan_id) {
            previous = current;
            current = current->next;
        }

        if (previous == NULL) {
            // Insert at the head
            new_vlan->next = interface->vlan_stats;
            interface->vlan_stats->prev = new_vlan;
            interface->vlan_stats = new_vlan;
        } else {
            // Insert in the middle or end
            new_vlan->next = current;
            new_vlan->prev = previous;
            previous->next = new_vlan;
            if (current != NULL) {
                current->prev = new_vlan;
            }
        }
    }

    return new_vlan;
}

void Vlan_reposition(Vlan* vlan) {
    if (vlan->parent && vlan->redirection) {
        lv_obj_update_layout(vlan->parent->image);
        lv_obj_update_layout(vlan->redirection->image);

        int input_x  = (lv_obj_get_x(vlan->parent->image)+lv_obj_get_x2(vlan->parent->image))/2;
        int output_x = (lv_obj_get_x(vlan->redirection->image)+lv_obj_get_x2(vlan->redirection->image))/2;
        int outindex = 0;
        Interface *output = vlan->redirection;
        while (output->next) {
            outindex++;
            output = output->next;
        }
        vlan->points[0].x = input_x;
        vlan->points[0].y = 98;
        vlan->points[1].x = input_x;
        vlan->points[1].y = 125 - outindex * 5;
        vlan->points[2].x = output_x;
        vlan->points[2].y = 125 - outindex * 5;
        vlan->points[3].x = output_x;
        vlan->points[3].y = 133;

        lv_line_set_points(vlan->line, vlan->points, 4);
        lv_obj_update_layout(vlan->line);
    }
}
void Vlan_refresh(Vlan* vlan) {
    if (vlan->parent && vlan->redirection) {
        if (vlan->parent->is_up && vlan->redirection->is_up)
            lv_obj_set_style_line_color(vlan->line, VX_GREEN_PALETTE, 0);
        else
            lv_obj_set_style_line_color(vlan->line, VX_ORANGE_PALETTE, 0);
    }
}
void Vlan_set_focus(Vlan* vlan, bool focus, vx_display_mode mode) {
    if (vlan->line)
        lv_obj_set_style_line_opa(vlan->line, focus ? LV_OPA_100 : LV_OPA_50, 0);
    // lv_obj_set_style_bg_opa(vlan->name, focus ? LV_OPA_100 : LV_OPA_50, 0);
    lv_chart_hide_series(vlan->parent->parent->network_chart, vlan->rx_bytes,   !(focus && (mode == VX_DISPLAY_BYTES)));
    lv_chart_hide_series(vlan->parent->parent->network_chart, vlan->rx_packets, !(focus && (mode == VX_DISPLAY_PACKETS)));
    lv_chart_hide_series(vlan->parent->parent->network_chart, vlan->rx_dropped, !(focus && (mode == VX_DISPLAY_DROPPED)));
}
void Vlan_visible(Vlan* vlan, const bool state) {
    if (vlan->line)
        if (state)
            lv_obj_add_flag(vlan->line, LV_OBJ_FLAG_HIDDEN);
        else {
            lv_obj_remove_flag(vlan->line, LV_OBJ_FLAG_HIDDEN);
            Vlan_refresh(vlan);
        }
}

void update_vlan_data(Vlan* vlan, InterfaceStats interface_stats) {
    add_data_to_buffer(&vlan->buffer, interface_stats);
    // Compute SMA
    vlan_update_sma(vlan);
}

int init_circular_buffer(InterfaceBuffer* buffer) {
    memset(buffer->data, 0, sizeof(buffer->data));
    buffer->head = 0;
    buffer->count = 0;
    return 0;
}

void add_data_to_buffer(InterfaceBuffer* buffer, InterfaceStats interface_stats) {
    buffer->data[buffer->head] = interface_stats;
    buffer->head = (buffer->head + 1) % (VX_NETWORK_CHART_SIZE + 1);
    if (buffer->count < (VX_NETWORK_CHART_SIZE + 1)) {
        buffer->count++;
    }
}

// Cpu
CpuCollection* init_cpus() {
    CpuCollection* collection = malloc(sizeof(CpuCollection));
    if (!collection) {
        perror("malloc failed");
        return NULL;
    }
    collection->head  = NULL;
    collection->count = 0;

    lv_obj_t* cpus_label = lv_label_create(lv_scr_act());
    if (!cpus_label) {
        perror("lv_label_create allocation failed");
        free(collection);
        return NULL;
    }
    lv_label_set_text(cpus_label, "CPU usage");
    lv_obj_set_size(cpus_label, 396, 16);
    lv_obj_set_style_text_align(cpus_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(cpus_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(cpus_label, -1, 0);
    lv_obj_set_style_text_color(cpus_label, VX_WHITE_COLOR, 0);
    lv_obj_set_pos(cpus_label, 0, 414);
    collection->cpus_label = cpus_label;

    lv_obj_t* cpus_chart = lv_chart_create(lv_scr_act());
    if (!cpus_chart) {
        perror("lv_obj_create allocation failed");
        lv_obj_del(cpus_label);
        free(collection);
        return NULL;
    }
    lv_obj_set_size(cpus_chart, 396, 128);
    lv_obj_align(cpus_chart, LV_ALIGN_TOP_LEFT, 0, 432);
    lv_chart_set_type(cpus_chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_line_width(cpus_chart, 4, LV_PART_INDICATOR);
    lv_chart_set_range(cpus_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 125);
    lv_chart_set_div_line_count(cpus_chart, 0, 0);
    lv_obj_set_style_size(cpus_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(cpus_chart, 0, 0);
    lv_obj_set_style_pad_all(cpus_chart, 0, 0);
    lv_chart_set_point_count(cpus_chart, VX_CPU_CHART_SIZE);
    collection->cpus_chart = cpus_chart;

    int cpus_count = sysconf(_SC_NPROCESSORS_ONLN);
    for (int i = 0; i < cpus_count; i++)
        if(!add_cpu(collection, i)) {
            lv_obj_del(cpus_chart);
            lv_obj_del(cpus_label);
            free(collection);
            return NULL;
        }

    return collection;
}
Cpu* add_cpu(CpuCollection* collection, int id) {
    static int i = 0;
    Cpu* new_cpu = malloc(sizeof(Cpu));
    if (!new_cpu) {
        perror("malloc failed");
        return NULL;
    }
    new_cpu->parent = collection;
    new_cpu->id = id;

    lv_obj_t* cpu_label = lv_label_create(lv_scr_act());
    if (!cpu_label) {
        perror("lv_label_create allocation failed");
        free(new_cpu);
        return NULL;
    }
    lv_obj_set_size(cpu_label, 95, 32);
    lv_obj_set_style_text_align(cpu_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(cpu_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(cpu_label, -1, 0);
    lv_obj_set_style_text_color(cpu_label, lv_palette_main(id * 3), 0);
    lv_obj_set_pos(cpu_label, 8 + 76 * (i % 4), 434 + 16 * (i / 4));
    new_cpu->cpu_label = cpu_label;

    lv_chart_series_t* cpu_load = lv_chart_add_series(collection->cpus_chart, lv_palette_main(i*3), LV_CHART_AXIS_PRIMARY_Y);
    if (!cpu_load) {
        perror("lv_chart_add_series allocation failed");
        lv_chart_remove_series(collection->cpus_chart, cpu_load);
        free(new_cpu);
        return NULL;
    }
    new_cpu->cpu_load = cpu_load;

    Cpu* tail = collection->head;
    if (tail) {
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = new_cpu;
    } else {
        collection->head = new_cpu;
    }

    i++;
    return new_cpu;
}
void update_cpu_data(Cpu* cpu, int load) {
    cpu->buffer.data[cpu->buffer.head] = load;
    cpu->buffer.head = (cpu->buffer.head + 1) % (VX_CPU_CHART_SIZE + 1);
    if (cpu->buffer.count < (VX_CPU_CHART_SIZE + 1)) {
        cpu->buffer.count++;
    }
}

// Memory
MemoryCollection* init_memory() {
    MemoryCollection* collection = malloc(sizeof(MemoryCollection));
    if(!collection) {
      perror("malloc failed");
      return NULL;
    }
    collection->head  = NULL;
    collection->count = 0;

    struct sysinfo info;
    if (sysinfo(&info) < 0) {
        perror("sysinfo");
        free(collection);
        return NULL;
    }
    uint64_t totalram;
    totalram = ((uint64_t) info.totalram * info.mem_unit);
    collection->total = totalram;

    lv_obj_t* memory_label = lv_label_create(lv_scr_act());
    if (!memory_label) {
        free(collection);
        perror("lv_label_create allocation failed");
        return NULL;
    }
    lv_label_set_text(memory_label, "Memory usage");
    lv_obj_set_size(memory_label, 396, 16);
    lv_obj_set_style_text_align(memory_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(memory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(memory_label, -1, 0);
    lv_obj_set_style_text_color(memory_label, VX_WHITE_COLOR, 0);
    lv_obj_set_pos(memory_label, 404, 414);

    lv_obj_t* memory_chart = lv_chart_create(lv_scr_act());
    if (!memory_chart) {
        lv_obj_del(memory_label);
        free(collection);
        perror("lv_chart_create allocation failed");
        return NULL;
    }
    lv_obj_set_size(memory_chart, 396, 128);
    lv_obj_align(memory_chart, LV_ALIGN_TOP_RIGHT, 0, 432);
    lv_chart_set_type(memory_chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_line_width(memory_chart, 4, LV_PART_INDICATOR);
    lv_chart_set_range(memory_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 125);
    lv_chart_set_div_line_count(memory_chart, 0, 0);
    lv_obj_set_style_size(memory_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(memory_chart, 0, 0);
    lv_obj_set_style_pad_all(memory_chart, 0, 0);
    lv_chart_set_point_count(memory_chart, VX_MEMORY_CHART_SIZE);
    collection->memory_chart = memory_chart;

    if (!add_memory(collection, "Main")) {
        lv_obj_del(memory_chart);
        lv_obj_del(memory_label);
        free(collection);
        return NULL;
    }
    if (!add_memory(collection, "Process")) {
        lv_obj_del(memory_chart);
        lv_obj_del(memory_label);
        free(collection);
        return NULL;
    }
    return collection;
}

Memory* add_memory(MemoryCollection* collection, const char* name) {
    static int i = 0;
    Memory* new_memory = malloc(sizeof(Memory));
    if (!new_memory) {
        perror("malloc failed");
        return NULL;
    }
    new_memory->parent = collection;
    strncpy(new_memory->name, name, 32);

    lv_obj_t* memory_label = lv_label_create(lv_scr_act());
    if (!memory_label) {
        perror("lv_label_create allocation failed");
        free(new_memory);
        return NULL;
    }
    lv_obj_set_size(memory_label, 256, 16);
    lv_obj_set_style_text_align(memory_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(memory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(memory_label, -1, 0);
    lv_obj_set_style_text_color(memory_label, (i ? VX_ORANGE_PALETTE:VX_RED_PALETTE), 0);
    lv_obj_set_pos(memory_label, 412, 434 + 16 * i);
    new_memory->memory_label = memory_label;

    lv_chart_series_t* memory_load = lv_chart_add_series(collection->memory_chart, (i ? VX_ORANGE_PALETTE:VX_RED_PALETTE), LV_CHART_AXIS_PRIMARY_Y);
    if (!memory_load) {
        perror("lv_chart_add_series allocation failed");
        lv_obj_del(memory_label);
        free(new_memory);
        return NULL;
    }
    new_memory->memory_load = memory_load;

    Memory* tail = collection->head;
    if (tail) {
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = new_memory;
    } else {
        collection->head = new_memory;
    }

    i++;
    return new_memory;
}

void update_memory_data(Memory* memory, uint64_t load) {
    memory->buffer.data[memory->buffer.head] = load;
    memory->buffer.head = (memory->buffer.head + 1) % (VX_MEMORY_CHART_SIZE + 1);
    if (memory->buffer.count < (VX_MEMORY_CHART_SIZE + 1)) {
        memory->buffer.count++;
    }
}
