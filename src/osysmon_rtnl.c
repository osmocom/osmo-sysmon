/* Simple Osmocom System Monitor (osysmon): Support for monitoring net-devices */

/* (C) 2018 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>

#include <libmnl/libmnl.h>
#include <talloc.h>

#include "value_node.h"
#include "osysmon.h"

struct rtnl_client_state {
	struct mnl_socket *nl;
};


struct netdev {
	struct llist_head list;
	struct {
		const char *name;
	} cfg;
};

static struct netdev *netdev_find(struct osysmon_state *os, const char *name)
{
	struct netdev *nd;
	llist_for_each_entry(nd, &os->netdevs, list) {
		if (!strcmp(name, nd->cfg.name))
			return nd;
	}
	return NULL;
}

static struct netdev *netdev_create(struct osysmon_state *os, const char *name)
{
	struct netdev *nd;

	if (netdev_find(os, name))
		return NULL;

	nd = talloc_zero(os, struct netdev);
	if (!nd)
		return NULL;
	nd->cfg.name = talloc_strdup(os, name);
	llist_add_tail(&nd->list, &os->netdevs);
	return nd;
}

static void netdev_destroy(struct netdev *nd)
{
	llist_del(&nd->list);
	talloc_free(nd);
}

/***********************************************************************
 * VTY
 ***********************************************************************/

static struct cmd_node netdev_node = {
	NETDEV_NODE,
	"%s(config-netdev)# ",
	1,
};

int osysmon_netdev_go_parent(struct vty *vty)
{
	switch (vty->node) {
	case NETDEV_NODE:
		vty->node = CONFIG_NODE;
		vty->index = NULL;
		break;
	default:
		break;
	}
	return vty->node;
}

DEFUN(cfg_netdev, cfg_netdev_cmd,
	"netdev NAME",
	"")
{
	struct netdev *nd;
	nd = netdev_find(g_oss, argv[0]);
	if (!nd)
		nd = netdev_create(g_oss, argv[0]);
	OSMO_ASSERT(nd);

	return CMD_SUCCESS;
}

DEFUN(cfg_no_netdev, cfg_no_netdev_cmd,
	"no netdev NAME",
	"")
{
	struct netdev *nd;
	nd = netdev_find(g_oss, argv[0]);
	if (!nd) {
		vty_out(vty, "Netdev %s doesn't exist in configuration%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}
	netdev_destroy(nd);
	return CMD_SUCCESS;
}

static void write_one_netdev(struct vty *vty, struct netdev *nd)
{
	vty_out(vty, "netdev %s%s", nd->cfg.name, VTY_NEWLINE);
}

static int config_write_netdev(struct vty *vty)
{
	struct netdev *nd;

	llist_for_each_entry(nd, &g_oss->netdevs, list)
		write_one_netdev(vty, nd);
	return CMD_SUCCESS;
}

static void osysmon_rtnl_vty_init(void)
{
	install_element(CONFIG_NODE, &cfg_netdev_cmd);
	install_element(CONFIG_NODE, &cfg_no_netdev_cmd);
	install_node(&netdev_node, config_write_netdev);
}


/***********************************************************************
 * Interface Level
 ***********************************************************************/

static int if_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case IFLA_ADDRESS:
		if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case IFLA_MTU:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case IFLA_IFNAME:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
	struct value_node *parent = data;
	struct value_node *vn_if;
	const char *name;
	char buf[32];

	struct nlattr *tb[IFLA_MAX+1] = {};
	mnl_attr_parse(nlh, sizeof(*ifm), if_attr_cb, tb);
	if (!tb[IFLA_IFNAME])
		return MNL_CB_OK;
	name = mnl_attr_get_str(tb[IFLA_IFNAME]);

	/* skip any non-configured interface names */
	if (!netdev_find(g_oss, name))
		return MNL_CB_OK;

	vn_if = value_node_find_or_add(parent, talloc_strdup(parent, name));
	OSMO_ASSERT(vn_if);
	vn_if->idx = ifm->ifi_index;

	if (tb[IFLA_ADDRESS] && mnl_attr_get_payload_len(tb[IFLA_ADDRESS]) == 6) {
		uint8_t *hwaddr = mnl_attr_get_payload(tb[IFLA_ADDRESS]);
		snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
			 hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
		value_node_add(vn_if, "hwaddr", buf);
	}
	if (ifm->ifi_flags & IFF_RUNNING) 
		value_node_add(vn_if, "running", "true");
	else
		value_node_add(vn_if, "running", "false");

	if (ifm->ifi_flags & IFF_UP)
		value_node_add(vn_if, "up", "true");
	else
		value_node_add(vn_if, "up", "false");

	return MNL_CB_OK;
}

static int rtnl_update_link(struct rtnl_client_state *rcs, struct value_node *parent)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	int ret;
	unsigned int seq, portid;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;

	portid = mnl_socket_get_portid(rcs->nl);

	if (mnl_socket_sendto(rcs->nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_sendto");
		exit(EXIT_FAILURE);
	}

	ret = mnl_socket_recvfrom(rcs->nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, parent);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(rcs->nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		return -1;
	}
	return 0;
}



/***********************************************************************
 * L3 Address Level
 ***********************************************************************/

static int inet_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, IFA_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case IFA_ADDRESS:
		if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int inet_data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct ifaddrmsg *ifa = mnl_nlmsg_get_payload(nlh);
	struct nlattr *tb[IFA_MAX + 1] = {};
	struct value_node *parent = data;
	struct value_node *vn_if;

	vn_if = value_node_find_by_idx(parent, ifa->ifa_index);
	if (!vn_if)
		return MNL_CB_OK;

	if (ifa->ifa_family != AF_INET)
		return MNL_CB_OK;

	mnl_attr_parse(nlh, sizeof(*ifa), inet_attr_cb, tb);
	if (tb[IFA_ADDRESS]) {
		struct in_addr *addr = mnl_attr_get_payload(tb[IFA_ADDRESS]);
		char out[INET_ADDRSTRLEN+32];
		snprintf(out, sizeof(out), "%s/%u", inet_ntoa(*addr), ifa->ifa_prefixlen);
		value_node_add(vn_if, "ip", out);
	}

	return MNL_CB_OK;
}

static int rtnl_update_addr(struct rtnl_client_state *rcs, struct value_node *parent)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	int ret;
	unsigned int seq, portid;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_GETADDR;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_INET;

	portid = mnl_socket_get_portid(rcs->nl);

	if (mnl_socket_sendto(rcs->nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_sendto");
		exit(EXIT_FAILURE);
	}

	ret = mnl_socket_recvfrom(rcs->nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, inet_data_cb, parent);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(rcs->nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		return -1;
	}

	return 0;
}



/***********************************************************************
 * Common Code / API
 ***********************************************************************/

int osysmon_rtnl_init()
{
	osysmon_rtnl_vty_init();
	return 0;
}



struct rtnl_client_state *rtnl_init(void *ctx)
{
	struct mnl_socket *nl;
	struct rtnl_client_state *rcs;

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return NULL;
	}
	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		mnl_socket_close(nl);
		return NULL;
	}

	rcs = talloc_zero(ctx, struct rtnl_client_state);
	if (!rcs) {
		mnl_socket_close(nl);
		return NULL;
	}

	rcs->nl = nl;

	return rcs;
}

int osysmon_rtnl_poll(struct value_node *parent)
{
	struct value_node *vn_net;

	if (llist_empty(&g_oss->netdevs))
		return 0;

	if (!g_oss->rcs)
		g_oss->rcs = rtnl_init(NULL);

	vn_net = value_node_add(parent, "netdev", NULL);

	if (!g_oss->rcs)
		return -1;

	rtnl_update_link(g_oss->rcs, vn_net);
	rtnl_update_addr(g_oss->rcs, vn_net);

	return 0;
}
