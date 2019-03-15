/* Simple Osmocom System Monitor (osysmon): Support for OpenVPN monitoring */

/* (C) 2019 by sysmocom - s.f.m.c. GmbH.
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

#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

#include <osmocom/core/utils.h>
#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>
#include <osmocom/core/msgb.h>
#include <osmocom/netif/stream.h>

#include "osysmon.h"
#include "client.h"
#include "value_node.h"

/***********************************************************************
 * Data model
 ***********************************************************************/

#define OVPN_LOG(ctx, vpn, fmt, args...)				\
	fprintf(stderr, "OpenVPN [%s]: " fmt, make_authority(ctx, vpn->cfg), ##args)

/* max number of csv in response */
#define MAX_RESP_COMPONENTS 6

/* a single OpenVPN management interface client */
struct openvpn_client {
	/* links to osysmon.openvpn_clients */
	struct llist_head list;
	struct host_cfg *cfg;
	struct osmo_stream_cli *mgmt;
	/* fields below are parsed from response to 'state' command on mgmt interface */
	struct host_cfg *rem_cfg;
	char *tun_ip;
	bool connected;
};

static char *parse_state(struct msgb *msg, struct openvpn_client *vpn)
{
	char tmp[128];
	char *tok;
	unsigned int i = 0;
	uint8_t *m = msgb_data(msg);
	unsigned int truncated_len = OSMO_MIN(sizeof(tmp) - 1, msgb_length(msg));

	if (msgb_length(msg) > truncated_len)
		OVPN_LOG(msg, vpn, "received message too long (%d >= %u), truncating...\n", msgb_length(msg), truncated_len);

	if (msgb_length(msg) > 0) {
		if (!isdigit(m[0])) /* skip OpenVPN greetings and alike */
			return NULL;
	} else {
		OVPN_LOG(msg, vpn, "received message is empty, ignoring...\n");
		return NULL;
	}

	memcpy(tmp, m, truncated_len);
	tmp[truncated_len] = '\0';

	for (tok = strtok(tmp, ","); tok && i < MAX_RESP_COMPONENTS; tok = strtok(NULL, ",")) {
		/* The string format is documented in https://openvpn.net/community-resources/management-interface/ */
		if (tok) { /* Parse csv string and pick interesting tokens while ignoring the rest. */
			switch (i++) {
			case 1:
				update_name(vpn->rem_cfg, tok);
				break;
			case 3:
				osmo_talloc_replace_string(vpn->rem_cfg, &vpn->tun_ip, tok);
				break;
			case 4:
				update_host(vpn->rem_cfg, tok);
				break;
			case 5:
				vpn->rem_cfg->remote_port = atoi(tok);
				break;
			}
		}
	}
	return NULL;
}

static struct openvpn_client *openvpn_client_find_or_make(const struct osysmon_state *os,
							  const char *host, uint16_t port)
{
	struct openvpn_client *vpn;
	llist_for_each_entry(vpn, &os->openvpn_clients, list) {
		if (match_config(vpn->cfg, host, MATCH_HOST) && vpn->cfg->remote_port == port)
			return vpn;
	}

	return NULL;
}

static int disconnect_cb(struct osmo_stream_cli *conn)
{
	struct openvpn_client *vpn = osmo_stream_cli_get_data(conn);

	update_name(vpn->rem_cfg, "disconnected from openvpn management socket");
	vpn->connected = false;
	talloc_free(vpn->tun_ip);
	talloc_free(vpn->rem_cfg->remote_host);
	vpn->tun_ip = NULL;
	vpn->rem_cfg->remote_host = NULL;

	return 0;
}

static int connect_cb(struct osmo_stream_cli *conn)
{
	struct openvpn_client *vpn = osmo_stream_cli_get_data(conn);

	update_name(vpn->rem_cfg, "connected to openvpn management socket");
	vpn->connected = true;

	return 0;
}

static int read_cb(struct osmo_stream_cli *conn)
{
	int bytes;
	struct openvpn_client *vpn = osmo_stream_cli_get_data(conn);
	struct msgb *msg = msgb_alloc(1024, "OpenVPN");
	if (!msg) {
		OVPN_LOG(conn, vpn, "unable to allocate message in callback\n");
		return 0;
	}

	bytes = osmo_stream_cli_recv(conn, msg);
	if (bytes < 0)
		OVPN_LOG(msg, vpn, "read on openvpn management socket failed (%d)\n", bytes);
	else
		parse_state(msg, vpn);

	msgb_free(msg);

	return 0;
}

static bool openvpn_client_create(struct osysmon_state *os, const char *name, const char *host, uint16_t port)
{
	struct openvpn_client *vpn = openvpn_client_find_or_make(os, host, port);
	if (vpn)
		return true;

	vpn = talloc_zero(os, struct openvpn_client);
	if (!vpn)
		return false;

	vpn->connected = false;

	vpn->cfg = host_cfg_alloc(vpn, name, host, port);
	if (!vpn->cfg)
		goto dealloc;

	vpn->rem_cfg = host_cfg_alloc(vpn, "connecting to openvpn management socket", NULL, 0);
	if (!vpn->rem_cfg)
		goto dealloc;

	vpn->mgmt = make_tcp_client(vpn->cfg);
	if (!vpn->mgmt)	{
		OVPN_LOG(vpn->rem_cfg, vpn, "failed to create TCP client\n");
		goto dealloc;
	}

	/* Wait for 1 minute before attempting to reconnect to management interface */
	osmo_stream_cli_set_reconnect_timeout(vpn->mgmt, 5);
	osmo_stream_cli_set_read_cb(vpn->mgmt, read_cb);
	osmo_stream_cli_set_connect_cb(vpn->mgmt, connect_cb);
	osmo_stream_cli_set_disconnect_cb(vpn->mgmt, disconnect_cb);

	if (osmo_stream_cli_open(vpn->mgmt) < 0) {
		OVPN_LOG(vpn->rem_cfg, vpn, "failed to connect to management socket\n");
		goto dealloc;
	}

	osmo_stream_cli_set_data(vpn->mgmt, vpn);
	llist_add_tail(&vpn->list, &os->openvpn_clients);

	return true;

dealloc:
	talloc_free(vpn);
	return false;
}

static void openvpn_client_destroy(struct openvpn_client *vpn)
{
	if (!vpn)
		return;

	osmo_stream_cli_destroy(vpn->mgmt);
	llist_del(&vpn->list);
	talloc_free(vpn);
}


/***********************************************************************
 * VTY
 ***********************************************************************/

#define OPENVPN_STR "Configure OpenVPN management interface address\n"

DEFUN(cfg_openvpn, cfg_openvpn_cmd,
      "openvpn HOST <1-65535>",
      OPENVPN_STR "Name of the host to connect to\n" "Management interface port\n")
{
	uint16_t port = atoi(argv[1]);

	if (!openvpn_client_create(g_oss, "OpenVPN", argv[0], port)) {
		vty_out(vty, "Failed to create OpenVPN client for %s:%u%s", argv[0], port, VTY_NEWLINE);
		return CMD_WARNING;
	}

	return CMD_SUCCESS;
}

DEFUN(cfg_no_openvpn, cfg_no_openvpn_cmd,
      "no openvpn HOST <1-65535>",
      NO_STR OPENVPN_STR "Name of the host to connect to\n" "Management interface port\n")
{
	uint16_t port = atoi(argv[1]);
	struct openvpn_client *vpn = openvpn_client_find_or_make(g_oss, argv[0], port);
	if (!vpn) {
		vty_out(vty, "OpenVPN client %s:%u doesn't exist%s", argv[0], port, VTY_NEWLINE);
		return CMD_WARNING;
	}

	openvpn_client_destroy(vpn);

	return CMD_SUCCESS;
}


/***********************************************************************
 * Runtime Code
 ***********************************************************************/

static int openvpn_client_poll(struct openvpn_client *vpn, struct value_node *parent)
{
	char *remote = make_authority(parent, vpn->rem_cfg);
	struct value_node *vn_host = value_node_find_or_add(parent, make_authority(parent, vpn->cfg));
	struct msgb *msg = msgb_alloc(128, "state");
	if (!msg) {
		value_node_add(vn_host, "msgb", "memory allocation failure");
		return 0;
	}

	if (vpn->rem_cfg->name)
		value_node_add(vn_host, "status", vpn->rem_cfg->name);

	if (vpn->tun_ip)
		value_node_add(vn_host, "tunnel", vpn->tun_ip);

	if (remote)
		value_node_add(vn_host, "remote", remote);

	if (vpn->connected) { /* re-trigger state command */
		msgb_printf(msg, "state\n");
		osmo_stream_cli_send(vpn->mgmt, msg);
	}

	return 0;
}

/* called once on startup before config file parsing */
int osysmon_openvpn_init()
{
	install_element(CONFIG_NODE, &cfg_openvpn_cmd);
	install_element(CONFIG_NODE, &cfg_no_openvpn_cmd);

	return 0;
}

/* called periodically */
int osysmon_openvpn_poll(struct value_node *parent)
{
	int num_vpns = llist_count(&g_oss->openvpn_clients);
	if (num_vpns) {
		struct value_node *vn_vpn = value_node_add(parent, "OpenVPN", NULL);
		struct openvpn_client *vpn;
		llist_for_each_entry(vpn, &g_oss->openvpn_clients, list)
			openvpn_client_poll(vpn, vn_vpn);
	}

	return num_vpns;
}
