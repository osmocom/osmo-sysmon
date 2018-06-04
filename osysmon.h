#pragma once

#include <osmocom/core/select.h>
#include <osmocom/core/utils.h>
#include <osmocom/vty/command.h>

#include <stdbool.h>
#include <linux/if.h>

#include "value_node.h"

/* information gathered from sysinfo(2) */
struct osysmon_state_sysinfo {
	unsigned long uptime_secs;
	unsigned long loads[3];
	unsigned long freeram;
};

/* one network interface */
struct osysmon_state_iface {
	char name[IFNAMSIZ+1];
	uint32_t flags;	/* enum net_device_flags IFF_* */
	uint8_t hwasddr[6];
	uint32_t ipaddr;
};

/* information gathered from RTNL via libmnl */
struct osysmon_state_rtnl {
	struct osysmon_state_iface iface[8];
	unsigned int num_iface;
	struct {
		char *ifname;
		uint32_t ipaddr;
	} default_route;
};

#if 0
struct osysmon_state_gbproxy {
	char ns_state[32];
	char bssgp_state[32];
};

struct osysmon_state_bts {
	bool rf_locked;
};

struct osysmon_state_bsc {
	struct osysmon_state_bts bts[8];
	unsigned int num_bts;
	char a_state[32];
};
#endif

struct osysmon_state {
	struct osysmon_state_sysinfo sysinfo;
	struct osysmon_state_rtnl rtnl;

	/* list of 'struct ctrl client' */
	struct llist_head ctrl_clients;
};

extern struct osysmon_state *g_oss;



enum osysmon_vty_node {
	CTRL_CLIENT_NODE = _LAST_OSMOVTY_NODE + 1,
	CTRL_CLIENT_GETVAR_NODE,
};


int osysmon_ctrl_go_parent(struct vty *vty);
int osysmon_ctrl_init();
int osysmon_ctrl_poll(struct value_node *parent);

int osysmon_sysinfo_init();
int osysmon_sysinfo_poll(struct value_node *parent);
