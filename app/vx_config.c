#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <net/if.h>

#include <linux/types.h>
#include <linux/if_link.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <signal.h>

#include "vx_config.h"
#include "vx_models.h"
#include "vx_network.h"

extern InterfaceCollection* interface_collection;

int load_configuration();
struct bpf_object *load_bpf_object(Interface* interface);
int setup_redirections(struct bpf_object *bpf_obj, cJSON *redirect_map, Interface* interface);

/*
{
    "interfaces": {
        "eth0": {
            "redirect_map": {
                "none": "eth2",
            }
        },
        "eth1": {
            "redirect_map": {
                "10": "eth3",
                "11": "eth3",
                "12": "eth2"
            }
        }
    }
}
*/
int load_configuration() {
	char *json_config = NULL;
	FILE *file;
	long length;

	// Read JSON configuration file
	file = fopen(VX_CONFIG_FILE, "r");
	if (!file) {
		perror("Error: opening config file failed");
		return -1;
	}

	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	json_config = malloc(length + 1);
	if (json_config) {
		fread(json_config, 1, length, file);
		json_config[length] = '\0';
	}
	fclose(file);

	if (!json_config) {
		perror("Error: reading config file failed");
		return -1;
	}

	cJSON *root = cJSON_Parse(json_config);
	if (!root) {
		perror("Error: parsing JSON configuration failed");
		free(json_config);
		return -1;
	}

	cJSON *interfaces = cJSON_GetObjectItem(root, "interfaces");
	if (!interfaces) {
		perror("Error: getting interfaces from JSON configuration failed");
		cJSON_Delete(root);
		free(json_config);
		return -1;
	}

	cJSON *json_interface;
	cJSON_ArrayForEach(json_interface, interfaces) {
		const char *interface_name = json_interface->string;
		int if_index = if_nametoindex(interface_name);
		Interface* interface = NULL;
		struct bpf_object *bpf_obj;

		if (interface_collection->input_count >= VX_MAX_INPUT_INTERFACES) {
			perror("Too many output ports defined");
			return -1;
		}
		// Setup input interface
		if(prepare_input_interface(interface_name) < 0)
			return -1;
		printf("add_input_interface(%d: %s)\n", if_index, interface_name);
		interface = add_input_interface(interface_collection, if_index, interface_name);
		if (!interface) {
			return -1;
		}

		// Load and attach BPF object file
		bpf_obj = load_bpf_object(interface);
		if (!bpf_obj) {
			cJSON_Delete(root);
			free(json_config);
			return -1;
		}

		// Configure the redirect map
		cJSON *redirect_map = cJSON_GetObjectItem(json_interface, "redirect_map");
		if (setup_redirections(bpf_obj, redirect_map, interface)) {
			bpf_object__close(bpf_obj);
			cJSON_Delete(root);
			free(json_config);
			return -1;
		}
	}
	if (interface_collection->input_count < 1) {
		perror("No input interface defined");
		return -1;
	}
	if (interface_collection->output_count < 1) {
		perror("No output interface defined");
		return -1;
	}

	printf("XDP programs successfully loaded and attached\n");

	Interface* input = interface_collection->input_head;
	while (input) {
		Vlan* vlan = input->vlan_stats;
		while (vlan) {
			Vlan_reposition(vlan);
			vlan = vlan->next;
		}
		input = input->next;
	}

	cJSON_Delete(root);
	free(json_config);
	return 0;
}

struct bpf_object *load_bpf_object(Interface* interface) {
	struct bpf_program *prog;
	int prog_fd;

	interface->bpf_prog = bpf_object__open_file(VX_XDP_FILE, NULL);
	if (libbpf_get_error(interface->bpf_prog)) {
		perror("Error: opening BPF object file failed");
		return NULL;
	}

	if (bpf_object__load(interface->bpf_prog)) {
		perror("Error: loading BPF object file failed");
		bpf_object__close(interface->bpf_prog);
		return NULL;
	}

	prog = bpf_object__find_program_by_name(interface->bpf_prog, VX_XDP_PROG_SECTION);
	if (!prog) {
		perror("Error: finding BPF program in object file failed");
		return NULL;
	}

	prog_fd = bpf_program__fd(prog);
	if (prog_fd < 0) {
		perror("Error: getting BPF program file descriptor failed");
		bpf_object__close(interface->bpf_prog);
		return NULL;
	}

	if (bpf_xdp_attach(interface->if_index, prog_fd, XDP_FLAGS_UPDATE_IF_NOEXIST|VX_XDP_MODE, NULL) < 0) {
		perror("Error: attaching BPF program to the interface failed");
		bpf_object__close(interface->bpf_prog);
		return NULL;
	}
	lv_label_set_text(interface->xdp_mode, "SKB");
	
	return interface->bpf_prog;
}

int setup_redirections(struct bpf_object *bpf_obj, cJSON *redirect_map, Interface* interface) {
	int vlan_redirect_map_fd, vlan_stats_fd;

	vlan_redirect_map_fd = bpf_object__find_map_fd_by_name(bpf_obj, "vlan_redirect_map");
	if(vlan_redirect_map_fd < 0) {
		perror("Error: getting vlan_redirect_map BPF map file descriptor failed");
		return -1;
	}
	interface->vlan_redirect_map_fd = vlan_redirect_map_fd;
	vlan_stats_fd = bpf_object__find_map_fd_by_name(bpf_obj, "vlan_stats");
	if(vlan_stats_fd < 0) {
		perror("Error: getting vlan_stats BPF map file descriptor failed");
		return -1;
	}
	interface->vlan_stats_fd = vlan_stats_fd;

	cJSON *item;
	cJSON_ArrayForEach(item, redirect_map) {
		Interface* redirection = NULL;
		const char *vlan_str = item->string;
		const char *interface_name = cJSON_GetStringValue(item);

		__u32 vlan_id;
		__u32 if_index = if_nametoindex(interface_name);

		if (if_index < 1) {
			perror("Error: Input Interface not found");
			return -1;
		}

		// Create output
		bool exists = false;
		redirection = interface->parent->output_head;
		while (redirection) {
			if (redirection->if_index == if_index) {
				exists = true;
				break;
			}
			redirection = redirection->next;
		}
		if (!exists) {
			if (interface->parent->output_count >= VX_MAX_OUTPUT_INTERFACES) {
				perror("Too many output ports defined");
				return -1;
			}
			if (prepare_output_interface(interface_name) < 0)
				return -1;
			printf("add_output_interface(%u:%s))\n", if_index, interface_name);
			redirection = add_output_interface(interface->parent, if_index, interface_name);
			if (!redirection) {
				return -1;
			}
		}

		// Parse and update vlan config for input
		if (strcmp(vlan_str, "none") == 0) {
			vlan_id = 0;
		} else if (strcmp(vlan_str, "any") == 0) {
			vlan_id = 4095;
		} else {
			vlan_id = (__u32)atoi(vlan_str);
			if (vlan_id == 0) {
				perror("Error: Incorrect VLAN identifier");
				return -1;
			}
		}
        printf("Adding VLAN XDP redirection %s (%u) to interface %s (%d)\n", vlan_str, vlan_id, interface_name, if_index);
		if (bpf_map_update_elem(vlan_redirect_map_fd, &vlan_id, &if_index, BPF_ANY)) {
			perror("Error: updating BPF map element failed");
			return -1;
		}

		// Create VLAN
		exists = false;
		Vlan* vlan = interface->vlan_stats;
		while (vlan) {
			if (vlan->vlan_id == vlan_id) {
				exists = true;
				break;
			}
			vlan = vlan->next;
		}
		if (!exists) {
	        printf("Adding VLAN %s (%u) to interface %s (%d)\n", vlan_str, vlan_id, interface_name, if_index);
			vlan = add_or_update_vlan(interface, vlan_id);
			if (!vlan) {
				return -1;
			}
		}
	}

	return 0;
}

void xdp_cleanup() {
	if (!interface_collection)
		return;
	Interface* interface = interface_collection->input_head;
	while (interface) {
		if (bpf_xdp_detach(interface->if_index, VX_XDP_MODE, NULL) < 0) {
			perror("xdp_program__detach");
		}
		bpf_object__close(interface->bpf_prog);
		interface = interface->next;
	}
}