/* Simple Osmocom System Monitor (osysmon): Name-Value tree */

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

#include <talloc.h>
#include <string.h>
#include <osmocom/core/utils.h>

#include "value_node.h"

struct value_node *value_node_add(void *ctx, struct value_node *parent,
				  const char *name, const char *value)
{
	struct value_node *vn = talloc_zero(parent, struct value_node);

	if (parent && value_node_find(parent, name)) {
		/* duplicate name not permitted! */
		return NULL;
	}

	vn = talloc_zero(parent, struct value_node);
	OSMO_ASSERT(vn);

	/* we assume the name is static/const and owned by caller */
	vn->name = name;
	if (value)
		vn->value = talloc_strdup(vn, value);
	INIT_LLIST_HEAD(&vn->children);
	if (parent)
		llist_add_tail(&vn->list, &parent->children);
	else
		INIT_LLIST_HEAD(&vn->list);
	return vn;
}

struct value_node *value_node_find(struct value_node *parent, const char *name)
{
	struct value_node *vn;
	llist_for_each_entry(vn, &parent->children, list) {
		if (!strcmp(name, vn->name))
			return vn;
	}
	return NULL;
}

struct value_node *value_node_find_or_add(struct value_node *parent, const char *name)
{
	struct value_node *vn;
	vn = value_node_find(parent, name);
	if (!vn)
		vn = value_node_add(parent, parent, name, NULL);
	return vn;
}

void value_node_del(struct value_node *node)
{
	/* remove ourselves from the parent */
	llist_del(&node->list);

#if 0	/* not actually needed, talloc should do this */
	struct value_node *ch, *ch2;
	llist_for_each_entry_safe(ch, ch2, &node->children, list)
		value_node_del(ch);
	/* "value" is a talloc child, and "name" is not owned by us */
#endif
	/* let talloc do its magic to delete all child nodes */
	talloc_free(node);
}
