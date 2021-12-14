/* Generic client structure and related helpers */

/* (C) 2018 by Harald Welte <laforge@gnumonks.org>
 * (C) 2019 by sysmocom - s.f.m.c. GmbH.
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

#include <stdbool.h>
#include <string.h>
#include <talloc.h>

#include <osmocom/core/utils.h>
#include <osmocom/netif/stream.h>

#include "client.h"

bool match_config(const struct host_cfg *cfg, const char *match, enum match_kind k)
{
	bool m_name = (strcmp(match, cfg->name) == 0),
	     m_host = (strcmp(match, cfg->remote_host) == 0);

	switch (k) {
	case MATCH_NAME:
		return m_name;
	case MATCH_HOST:
		return m_host;
	case MATCH_EITHER:
		return m_name | m_host;
	case MATCH_BOTH:
		return m_name & m_host;
	default:
		return false;
	}

	return false;
}

struct host_cfg *host_cfg_alloc(void *ctx, const char *name, const char *host, uint16_t port)
{
	struct host_cfg *cfg = talloc_zero(ctx, struct host_cfg);
	if (!cfg)
		return NULL;

	cfg->name = talloc_strdup(cfg, name);
	cfg->remote_host = talloc_strdup(cfg, host);
	cfg->remote_port = port;

	return cfg;
}

char *make_authority(void *ctx, const struct host_cfg *cfg)
{
	if (!cfg->remote_host)
		return NULL;

	return talloc_asprintf(ctx, "%s:%u", cfg->remote_host, cfg->remote_port);
}

struct osmo_stream_cli *make_tcp_client(struct host_cfg *cfg)
{
	struct osmo_stream_cli *cl = osmo_stream_cli_create(cfg);
	if (cl) {
		osmo_stream_cli_set_addr(cl, cfg->remote_host);
		osmo_stream_cli_set_port(cl, cfg->remote_port);
	}

	return cl;
}

void update_name(struct host_cfg *cfg, const char *new_name)
{
	osmo_talloc_replace_string(cfg, (char **)&cfg->name, new_name);
}

void update_host(struct host_cfg *cfg, const char *new_host)
{
	osmo_talloc_replace_string(cfg, (char **)&cfg->remote_host, new_host);
}
