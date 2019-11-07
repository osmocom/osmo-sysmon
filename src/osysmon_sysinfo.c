/* Simple Osmocom System Monitor (osysmon): Support for uptime/load/ram */

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
#include <sys/sysinfo.h>

#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>

#include "osysmon.h"
#include "value_node.h"

static float loadfac(unsigned long in) {
	return in/65536.0;
}

#define to_mbytes(in) ((in)/((1024*1024)/si.mem_unit))

#define SECS_PER_MIN	60UL
#define MINS_PER_HOUR	60UL
#define HOURS_PER_DAY	24UL

#define to_days(in)	((in)/(SECS_PER_MIN*MINS_PER_HOUR*HOURS_PER_DAY))
#define to_hours(in)	(((in)/(SECS_PER_MIN*MINS_PER_HOUR))%HOURS_PER_DAY)
#define to_minutes(in)	(((in)/(SECS_PER_MIN))%MINS_PER_HOUR)
#define to_seconds(in)	((in)%SECS_PER_MIN)

static bool sysinfo_enabled = 1;

/***********************************************************************
 * VTY
 ***********************************************************************/

#define CMD_STR "Display sysinfo\n"
DEFUN(cfg_sysinfo, cfg_sysinfo_cmd,
	"sysinfo",
	CMD_STR)
{
	sysinfo_enabled = true;

	return CMD_SUCCESS;
}

DEFUN(cfg_no_sysinfo, cfg_no_sysinfo_cmd,
	"no sysinfo",
	NO_STR CMD_STR)
{
	sysinfo_enabled = false;

	return CMD_SUCCESS;
}

static void osysmon_sysinfo_vty_init(void)
{
	install_element(CONFIG_NODE, &cfg_sysinfo_cmd);
	install_element(CONFIG_NODE, &cfg_no_sysinfo_cmd);
}

/***********************************************************************
 * Runtime Code
 ***********************************************************************/

/* called once on startup before config file parsing */
int osysmon_sysinfo_init()
{
	osysmon_sysinfo_vty_init();
	return 0;
}

/* called periodically */
int osysmon_sysinfo_poll(struct value_node *parent)
{
	struct sysinfo si;
	struct value_node *vn_sysinfo;
	char buf[32];
	int rc;

	if (!sysinfo_enabled)
		return 0;

	vn_sysinfo = value_node_add(parent, "sysinfo", NULL);

	rc = sysinfo(&si);
	if (rc < 0)
		return rc;

	/* Load Factor 1/5/15min */
	snprintf(buf, sizeof(buf), "%.2f/%.2f/%.2f",
		 loadfac(si.loads[0]), loadfac(si.loads[1]), loadfac(si.loads[2]));
	value_node_add(vn_sysinfo, "load", buf);

	/* RAM information (total/free/sared) in megabytes */
	snprintf(buf, sizeof(buf), "%lu/%lu/%lu",
		to_mbytes(si.totalram), to_mbytes(si.freeram), to_mbytes(si.sharedram));
	value_node_add(vn_sysinfo, "ram", buf);

	/* uptime in days/hours/minutes/seconds */
	snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", to_days(si.uptime),
		 to_hours(si.uptime), to_minutes(si.uptime), to_seconds(si.uptime));
	value_node_add(vn_sysinfo, "uptime", buf);

	return 0;
}
