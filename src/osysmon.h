#pragma once

#include <osmocom/core/select.h>
#include <osmocom/core/utils.h>
#include <osmocom/vty/command.h>

#include <stdbool.h>

#include "value_node.h"

struct rtnl_client_state;
struct ping_state;

struct osysmon_state {
	struct rtnl_client_state *rcs;
	/* list of 'struct ctrl client' */
	struct llist_head ctrl_clients;
	/* list of 'struct netdev' */
	struct llist_head netdevs;
	/* list of 'struct osysmon_file' */
	struct llist_head files;
	struct ping_state *pings;
};

extern struct osysmon_state *g_oss;



enum osysmon_vty_node {
	CTRL_CLIENT_NODE = _LAST_OSMOVTY_NODE + 1,
	CTRL_CLIENT_GETVAR_NODE,
	NETDEV_NODE,
	PING_NODE,
};


int osysmon_ctrl_go_parent(struct vty *vty);
int osysmon_ctrl_init();
int osysmon_ctrl_poll(struct value_node *parent);

int osysmon_rtnl_go_parent(struct vty *vty);
int osysmon_rtnl_init();
int osysmon_rtnl_poll(struct value_node *parent);

int osysmon_sysinfo_init();
int osysmon_sysinfo_poll(struct value_node *parent);

int osysmon_ping_init();
int osysmon_ping_poll(struct value_node *parent);

int osysmon_file_init();
int osysmon_file_poll(struct value_node *parent);
