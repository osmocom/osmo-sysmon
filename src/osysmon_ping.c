/* Simple Osmocom System Monitor (osysmon): Support for ping probe */

/* (C) 2018 by sysmocom - s.f.m.c. GmbH.
 * Author: Max Suraev
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301, USA.
 */

#include <stdbool.h>
#include <errno.h>

#include <oping.h>

#include <osmocom/core/utils.h>
#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>

#include "osysmon.h"
#include "value_node.h"

/***********************************************************************
 * Data model
 ***********************************************************************/

#define BUFLEN 128

struct ping_state {
       pingobj_t *ping_handle;
};

/* FIXME: replace with ping_iterator_count() once we bump requirements to liboping 1.10.0+ */
static unsigned iterator_count(pingobj_t *p)
{
	unsigned count = 0;
	pingobj_iter_t *iter;
	for (iter = ping_iterator_get(p); iter; iter = ping_iterator_next(iter))
		count++;
	return count;
}

/* Workaround for liboping issue https://github.com/octo/liboping/issues/43 */
static int add_host(pingobj_t *pinger, const char *host)
{
	int num_host = iterator_count(pinger),
		rc = ping_host_add(pinger, host);

	if (rc < 0) {
		if (num_host != iterator_count(pinger))
			ping_host_remove(pinger, host);
	}

	return rc;
}


static bool add_drop(pingobj_iter_t *iter, struct value_node *vn_host)
{
	char *s = NULL;
	uint32_t drop, seq;
	size_t len = sizeof(drop);
	int rc = ping_iterator_get_info(iter, PING_INFO_DROPPED, &drop, &len);
	if (rc)
		return false;

	len = sizeof(seq);
	rc = ping_iterator_get_info(iter, PING_INFO_SEQUENCE, &seq, &len);
	if (rc)
		return false;

	osmo_talloc_asprintf(vn_host, s, "%u/%u", drop, seq);
	value_node_add(vn_host, "dropped", s);

	return true;
}

static bool add_ttl(pingobj_iter_t *iter, struct value_node *vn_host)
{
	int ttl, rc;
	size_t len = sizeof(ttl);

	rc = ping_iterator_get_info(iter, PING_INFO_RECV_TTL, &ttl, &len);
	if (rc)
		return false;

	if (ttl > -1) {
		char *s = NULL;
		osmo_talloc_asprintf(vn_host, s, "%d", ttl);
		value_node_add(vn_host, "TTL", s);
	}

	return true;
}

static bool add_latency(pingobj_iter_t *iter, struct value_node *vn_host)
{
	double latency;
	size_t len = sizeof(latency);
	int rc = ping_iterator_get_info(iter, PING_INFO_LATENCY, &latency, &len);
	if (rc)
		return false;

	if (latency > -1) {
		char *s = NULL;
		osmo_talloc_asprintf(vn_host, s, "%.1lf ms", latency);
		value_node_add(vn_host, "latency", s);
	}

	return true;
}


/***********************************************************************
 * VTY
 ***********************************************************************/

static struct cmd_node ping_node = {
	PING_NODE,
	"%s(config-ping)# ",
	1,
};

int osysmon_ping_go_parent(struct vty *vty)
{
	switch (vty->node) {
	case PING_NODE:
		vty->node = CONFIG_NODE;
		vty->index = NULL;
		break;
	default:
		break;
	}
	return vty->node;
}

#define PING_STR "Configure a host to be monitored/pinged\n"

DEFUN(cfg_ping, cfg_ping_cmd,
      "ping HOST",
      PING_STR "Name of the host to ping\n")
{
	int rc = add_host(g_oss->pings->ping_handle, argv[0]);
	if (rc < 0) {
		vty_out(vty, "[%d] Couldn't add pinger for %s: %s%s",
			iterator_count(g_oss->pings->ping_handle), argv[0],
			ping_get_error(g_oss->pings->ping_handle), VTY_NEWLINE);

		return CMD_WARNING;
	}

	return CMD_SUCCESS;
}

DEFUN(cfg_no_ping, cfg_no_ping_cmd,
      "no ping HOST",
      NO_STR PING_STR "Name of the host to ping\n")
{
	int rc = ping_host_remove(g_oss->pings->ping_handle, argv[0]);
	if (rc < 0) {
		vty_out(vty, "[%d] Couldn't remove %s pinger: %s%s",
			iterator_count(g_oss->pings->ping_handle), argv[0],
			ping_get_error(g_oss->pings->ping_handle), VTY_NEWLINE);

		return CMD_WARNING;
	}

	return CMD_SUCCESS;
}

static int config_write_ping(struct vty *vty)
{
	char buf[BUFLEN];
	pingobj_iter_t *iter;
	for (iter = ping_iterator_get(g_oss->pings->ping_handle); iter; iter = ping_iterator_next(iter)) {
		size_t len = BUFLEN;
		/* hostname as it was supplied via vty 'ping' entry */
		if (ping_iterator_get_info(iter, PING_INFO_USERNAME, buf, &len))
			return CMD_WARNING;

		vty_out(vty, "ping %s%s", buf, VTY_NEWLINE);
	}

	return CMD_SUCCESS;
}


/***********************************************************************
 * Runtime Code
 ***********************************************************************/

/* called once on startup before config file parsing */
int osysmon_ping_init()
{
	install_element(CONFIG_NODE, &cfg_ping_cmd);
	install_element(CONFIG_NODE, &cfg_no_ping_cmd);
	install_node(&ping_node, config_write_ping);

	g_oss->pings = talloc_zero(NULL, struct ping_state);
	if (!g_oss->pings)
		return -ENOMEM;

	g_oss->pings->ping_handle = ping_construct();

	if (g_oss->pings->ping_handle)
		return 0;

	return -EINVAL;
}

/* called periodically */
int osysmon_ping_poll(struct value_node *parent)
{
	char buf[BUFLEN];
	struct value_node *vn_host;
	int num_host = iterator_count(g_oss->pings->ping_handle);
	pingobj_iter_t *iter;

	if (!num_host)
		return 0;

	struct value_node *vn_ping = value_node_add(parent, "ping", NULL);
	if (!vn_ping)
		return -ENOMEM;

	for (iter = ping_iterator_get(g_oss->pings->ping_handle); iter; iter = ping_iterator_next(iter)) {
		size_t len = BUFLEN;
		int rc = ping_iterator_get_info(iter, PING_INFO_USERNAME, buf, &len);
		if (rc)
			return -EINVAL;

		vn_host = value_node_find_or_add(vn_ping, talloc_strdup(vn_ping, buf));
		if (!vn_host)
			return -ENOMEM;

		len = BUFLEN; /* IP address is looked up on-call, even 40 bytes should be enough */
		rc = ping_iterator_get_info(iter, PING_INFO_ADDRESS, buf, &len);
		if (rc)
			return -EINVAL;

		value_node_add(vn_host, "IP", buf);

		add_drop(iter, vn_host);

		/* Parameters below might be absent from output depending on the host reachability: */
		add_latency(iter, vn_host);
		add_ttl(iter, vn_host);
	}

	return ping_send(g_oss->pings->ping_handle);
}
