/* Simple Osmocom System Monitor (osysmon): Support for CTRL monitoring */

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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301, USA.
 */

#include <string.h>

#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>

#include "osysmon.h"
#include "simple_ctrl.h"
#include "value_node.h"

/***********************************************************************
 * Data Model
 ***********************************************************************/

/* a single CTRL client */
struct ctrl_client {
	/* links to osysmon.ctrl_clients */
	struct llist_head list;
	struct {
		/* name of this CTRL client */
		const char *name;
		/* remote host/IP */
		const char *remote_host;
		/* remote CTRL port */
		uint16_t remote_port;
	} cfg;
	struct simple_ctrl_handle *sch;
	/* list of ctrl_client_get_var objects */
	struct llist_head get_vars;
};

/* a variable we are GETing via a ctrl_client */
struct ctrl_client_get_var {
	/* links to ctrl_client.get_vars */
	struct llist_head list;
	/* back-link to ctrl_client */
	struct ctrl_client *cc;
	struct {
		/* CTRL variable name */
		const char *name;
		/* display name, if any */
		const char *display_name;
	} cfg;
};

static struct ctrl_client *ctrl_client_find(struct osysmon_state *os, const char *name)
{
	struct ctrl_client *cc;
	llist_for_each_entry(cc, &os->ctrl_clients, list) {
		if (!strcmp(name, cc->cfg.name))
			return cc;
	}
	return NULL;
}

static struct ctrl_client *ctrl_client_create(struct osysmon_state *os, const char *name,
					      const char *host, uint16_t port)
{
	struct ctrl_client *cc;

	if (ctrl_client_find(os, name))
		return NULL;

	cc = talloc_zero(os, struct ctrl_client);
	if (!cc)
		return NULL;
	cc->cfg.name = talloc_strdup(cc, name);
	cc->cfg.remote_host = talloc_strdup(cc, host);
	cc->cfg.remote_port = port;
	INIT_LLIST_HEAD(&cc->get_vars);
	llist_add_tail(&cc->list, &os->ctrl_clients);
	/* FIXME */
	return cc;
}

static void ctrl_client_destroy(struct ctrl_client *cc)
{
	/* FIXME */
	llist_del(&cc->list);
	talloc_free(cc);
}

static struct ctrl_client_get_var *
ctrl_client_get_var_find_or_create(struct ctrl_client *cc, const char *name)
{
	struct ctrl_client_get_var *gv;
	llist_for_each_entry(gv, &cc->get_vars, list) {
		if (!strcmp(name, gv->cfg.name))
			return gv;
	}
	gv = talloc_zero(cc, struct ctrl_client_get_var);
	if (!gv)
		return NULL;
	gv->cc = cc;
	gv->cfg.name = talloc_strdup(gv, name);
	llist_add_tail(&gv->list, &cc->get_vars);
	return gv;
}

/***********************************************************************
 * VTY
 ***********************************************************************/

static struct cmd_node ctrl_client_node = {
	CTRL_CLIENT_NODE,
	"%s(config-ctrlclient)# ",
	1,
};

static struct cmd_node ctrl_client_getvar_node = {
	CTRL_CLIENT_GETVAR_NODE,
	"%s(config-ctrlclient-getvar)# ",
	1,
};

int osysmon_ctrl_go_parent(struct vty *vty)
{
	switch (vty->node) {
	case CTRL_CLIENT_NODE:
		vty->node = CONFIG_NODE;
		vty->index = NULL;
		break;
	case CTRL_CLIENT_GETVAR_NODE:
		vty->node = CTRL_CLIENT_NODE;
		{
			struct ctrl_client_get_var *gv = vty->index;
			vty->index = gv->cc;
		}
		break;
	default:
		break;
	}
	return vty->node;
}


DEFUN(cfg_ctrl_client, cfg_ctrl_client_cmd,
	"ctrl-client NAME A.B.C.D <1-65535>",
	"")
{
	struct ctrl_client *cc;
	cc = ctrl_client_find(g_oss, argv[0]);
	if (cc) {
		if ((strcmp(cc->cfg.remote_host, argv[1])) ||
		    (cc->cfg.remote_port != atoi(argv[2]))) {
			vty_out(vty, "Client %s has different IP/port, please remove it first%s",
				cc->cfg.name, VTY_NEWLINE);
			return CMD_WARNING;
		}
	} else
		cc = ctrl_client_create(g_oss, argv[0], argv[1], atoi(argv[2]));
	OSMO_ASSERT(cc);

	vty->node = CTRL_CLIENT_NODE;
	vty->index = cc;
	return CMD_SUCCESS;
}

DEFUN(cfg_no_ctrl_client, cfg_no_ctrl_client_cmd,
	"no ctrl-client NAME",
	NO_STR "")
{
	struct ctrl_client *cc;
	cc = ctrl_client_find(g_oss, argv[0]);
	if (!cc) {
		vty_out(vty, "Client %s doesn't exist%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}
	ctrl_client_destroy(cc);
	return CMD_SUCCESS;
}

DEFUN(cfg_ctrlc_get_var, cfg_ctrlc_get_var_cmd,
	"get-variable NAME",
	"")
{
	struct ctrl_client *cc = vty->index;
	struct ctrl_client_get_var *ccgv;

	ccgv = ctrl_client_get_var_find_or_create(cc, argv[0]);
	OSMO_ASSERT(ccgv);

	vty->node = CTRL_CLIENT_GETVAR_NODE;
	vty->index = ccgv;
	return CMD_SUCCESS;
}

DEFUN(cfg_ctrlc_no_get_var, cfg_ctrlc_no_get_var_cmd,
	"no get-variable NAME",
	NO_STR "")
{
	struct ctrl_client *cc = vty->index;
	struct ctrl_client_get_var *ccgv;

	ccgv = ctrl_client_get_var_find_or_create(cc, argv[0]);
	talloc_free(ccgv);
	return CMD_SUCCESS;
}

static void write_one_ctrl_client(struct vty *vty, struct ctrl_client *cc)
{
	struct ctrl_client_get_var *ccgv;
	vty_out(vty, "ctrl-client %s %s %u%s", cc->cfg.name,
		cc->cfg.remote_host, cc->cfg.remote_port, VTY_NEWLINE);
	llist_for_each_entry(ccgv, &cc->get_vars, list) {
		vty_out(vty, " get-variable %s%s", ccgv->cfg.name, VTY_NEWLINE);
		if (ccgv->cfg.display_name)
			vty_out(vty, " display-name %s%s", ccgv->cfg.display_name, VTY_NEWLINE);
	}
}

static int config_write_ctrl_client(struct vty *vty)
{
	struct ctrl_client *cc;

	llist_for_each_entry(cc, &g_oss->ctrl_clients, list)
		write_one_ctrl_client(vty, cc);
	return CMD_SUCCESS;
}

static void osysmon_ctrl_vty_init(void)
{
	install_element(CONFIG_NODE, &cfg_ctrl_client_cmd);
	install_element(CONFIG_NODE, &cfg_no_ctrl_client_cmd);
	install_node(&ctrl_client_node, config_write_ctrl_client);
	install_element(CTRL_CLIENT_NODE, &cfg_ctrlc_get_var_cmd);
	install_element(CTRL_CLIENT_NODE, &cfg_ctrlc_no_get_var_cmd);
	install_node(&ctrl_client_getvar_node, NULL);
	//install_element(CTRL_CLIENT_GETVAR_NODE, &cfg_getvar_disp_name_cmd);
}


/***********************************************************************
 * Runtime Code
 ***********************************************************************/

/* called once on startup before config file parsing */
int osysmon_ctrl_init()
{
	osysmon_ctrl_vty_init();
	return 0;
}

static int ctrl_client_poll(struct ctrl_client *cc, struct value_node *parent)
{
	struct ctrl_client_get_var *ccgv;
	struct value_node *vn_clnt = value_node_add(parent, parent, cc->cfg.name, NULL);

	/* attempt to re-connect */
	if (!cc->sch)
		cc->sch = simple_ctrl_open(cc, cc->cfg.remote_host, cc->cfg.remote_port, 1000);
	/* abort, if that failed */
	if (!cc->sch) {
		return -1;
	}

	llist_for_each_entry(ccgv, &cc->get_vars, list) {
		char *value = simple_ctrl_get(cc->sch, ccgv->cfg.name);

		/* FIXME: Distinguish between ERROR reply and
		 * connection issues */
		/* Close connection on error */
		if (!value) {
			simple_ctrl_close(cc->sch);
			cc->sch = NULL;
			return 0;
		}

		value_node_add(vn_clnt, vn_clnt, ccgv->cfg.name, value);
		free(value); /* no talloc, this is from sscanf() */
	}
	return 0;
}

/* called periodically */
int osysmon_ctrl_poll(struct value_node *parent)
{
	struct ctrl_client *cc;
	llist_for_each_entry(cc, &g_oss->ctrl_clients, list)
		ctrl_client_poll(cc, parent);
	return 0;
}
