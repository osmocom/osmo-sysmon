/* Simple, blocking client API against the Osmocom CTRL interface */

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

#include <unistd.h>
#include <stdint.h>
#include <talloc.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <netinet/in.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/socket.h>
#include <osmocom/gsm/ipa.h>
#include <osmocom/gsm/protocol/ipaccess.h>

#include "simple_ctrl.h"

/***********************************************************************
 * blocking I/O with timeout helpers
 ***********************************************************************/

static struct timeval *timeval_from_msec(uint32_t tout_msec)
{
	static struct timeval tout;

	if (tout_msec == 0)
		return NULL;
	tout.tv_sec = tout_msec/1000;
	tout.tv_usec = (tout_msec%1000)*1000;

	return &tout;
}

static ssize_t read_timeout(int fd, void *buf, size_t count, uint32_t tout_msec)
{
	fd_set readset;
	int rc;

	FD_ZERO(&readset);
	FD_SET(fd, &readset);

	rc = select(fd+1, &readset, NULL, NULL, timeval_from_msec(tout_msec));
	if (rc < 0)
		return rc;

	if (FD_ISSET(fd, &readset))
		return read(fd, buf, count);

	return -ETIMEDOUT;
}

static ssize_t write_timeout(int fd, const void *buf, size_t count, uint32_t tout_msec)
{
	fd_set writeset;
	int rc;

	FD_ZERO(&writeset);
	FD_SET(fd, &writeset);

	rc = select(fd+1, NULL, &writeset, NULL, timeval_from_msec(tout_msec));
	if (rc < 0)
		return rc;

	if (FD_ISSET(fd, &writeset))
		return write(fd, buf, count);

	return -ETIMEDOUT;
}


/***********************************************************************
 * actual CTRL client API
 ***********************************************************************/

struct simple_ctrl_handle {
	int fd;
	uint32_t next_id;
	uint32_t tout_msec;
};

struct simple_ctrl_handle *simple_ctrl_open(void *ctx, const char *host, uint16_t dport,
					    uint32_t tout_msec)
{
	struct simple_ctrl_handle *sch;
	fd_set writeset;
	int off = 0;
	int rc, fd;

	fd = osmo_sock_init(AF_INET, SOCK_STREAM, IPPROTO_TCP, host, dport,
			    OSMO_SOCK_F_CONNECT | OSMO_SOCK_F_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "CTRL: error connecting socket: %s\n", strerror(errno));
		return NULL;
	}

	/* wait until connect (or timeout) happens */
	FD_ZERO(&writeset);
	FD_SET(fd, &writeset);
	rc = select(fd+1, NULL, &writeset, NULL, timeval_from_msec(tout_msec));
	if (rc == 0) {
		fprintf(stderr, "CTRL: timeout during connect\n");
		goto out_close;
	}
	if (rc < 0) {
		fprintf(stderr, "CTRL: error connecting socket: %s\n", strerror(errno));
		goto out_close;
	}

	/* set FD blocking again */
	if (ioctl(fd, FIONBIO, (unsigned char *)&off) < 0) {
		fprintf(stderr, "CTRL: cannot set socket blocking: %s\n", strerror(errno));
		goto out_close;
	}

	sch = talloc_zero(ctx, struct simple_ctrl_handle);
	if (!sch)
		goto out_close;
	sch->fd = fd;
	sch->tout_msec = tout_msec;
	return sch;

out_close:
	close(fd);
	return NULL;
}

void simple_ctrl_set_timeout(struct simple_ctrl_handle *sch, uint32_t tout_msec)
{
	sch->tout_msec = tout_msec;
}

void simple_ctrl_close(struct simple_ctrl_handle *sch)
{
	close(sch->fd);
	talloc_free(sch);
}

static struct msgb *simple_ipa_receive(struct simple_ctrl_handle *sch)
{
	struct ipaccess_head hh;
	struct msgb *resp;
	int rc, len;

	rc = read_timeout(sch->fd, (uint8_t *) &hh, sizeof(hh), sch->tout_msec);
	if (rc < 0) {
		fprintf(stderr, "CTRL: Error during read: %d\n", rc);
		return NULL;
	} else if (rc < sizeof(hh)) {
		fprintf(stderr, "CTRL: ERROR: short read (header)\n");
		return NULL;
	}
	len = ntohs(hh.len);

	resp = msgb_alloc(len+sizeof(hh), "CTRL Rx");
	if (!resp)
		return NULL;
	resp->l1h = msgb_put(resp, sizeof(hh));
	memcpy(resp->l1h, (uint8_t *) &hh, sizeof(hh));

	resp->l2h = resp->tail;
	rc = read(sch->fd, resp->l2h, len);
	if (rc < len) {
		fprintf(stderr, "CTRL: ERROR: short read (payload)\n");
		msgb_free(resp);
		return NULL;
	}
	msgb_put(resp, rc);

	return resp;
}

struct msgb *simple_ctrl_receive(struct simple_ctrl_handle *sch)
{
	struct msgb *resp;
	struct ipaccess_head *ih;
	struct ipaccess_head_ext *ihe;

	/* loop until we've received a CTRL message */
	while (true) {
		resp = simple_ipa_receive(sch);
		if (!resp)
			return NULL;

		ih = (struct ipaccess_head *) resp->l1h;
		if (ih->proto == IPAC_PROTO_OSMO)
			resp->l2h = resp->l2h+1;
		ihe = (struct ipaccess_head_ext*) (resp->l1h + sizeof(*ih));
		if (ih->proto == IPAC_PROTO_OSMO && ihe->proto == IPAC_PROTO_EXT_CTRL)
			return resp;
		else {
			fprintf(stderr, "unknown IPA message %s\n", msgb_hexdump(resp));
			msgb_free(resp);
		}
	}
}

static int simple_ctrl_send(struct simple_ctrl_handle *sch, struct msgb *msg)
{
	int rc;

	ipa_prepend_header_ext(msg, IPAC_PROTO_EXT_CTRL);
	ipa_prepend_header(msg, IPAC_PROTO_OSMO);

	rc = write_timeout(sch->fd, msg->data, msg->len, sch->tout_msec);
	if (rc < 0) {
		fprintf(stderr, "CTRL: Error during write: %d\n", rc);
		return rc;
	} else if (rc < msg->len) {
		fprintf(stderr, "CTRL: ERROR: short write\n");
		msgb_free(msg);
		return -1;
	} else {
		msgb_free(msg);
		return 0;
	}
}

static struct msgb *simple_ctrl_xceive(struct simple_ctrl_handle *sch, struct msgb *msg)
{
	int rc;

	rc = simple_ctrl_send(sch, msg);
	if (rc < 0)
		return NULL;

	/* FIXME: ignore any TRAP */
	/* FIXME: check string is zero-terminated */
	return simple_ctrl_receive(sch);
}

char *simple_ctrl_get(struct simple_ctrl_handle *sch, const char *var)
{
	struct msgb *msg = msgb_alloc_headroom(512+8, 8, "CTRL GET");
	struct msgb *resp;
	unsigned int rx_id;
	char *rx_var, *rx_val;
	int rc;

	if (!msg)
		return NULL;

	rc = msgb_printf(msg, "GET %u %s", sch->next_id++, var);
	if (rc < 0) {
		msgb_free(msg);
		return NULL;
	}
	resp = simple_ctrl_xceive(sch, msg);
	if (!resp)
		return NULL;

	rc = sscanf(msgb_l2(resp), "GET_REPLY %u %ms %ms", &rx_id, &rx_var, &rx_val);
	if (rc == 3) {
		if (rx_id == sch->next_id-1 && !strcmp(var, rx_var)) {
			free(rx_var);
			msgb_free(resp);
			return rx_val;
		}
		free(rx_var);
		free(rx_val);
	} else {
		fprintf(stderr, "CTRL: ERROR: GET(%s) results in '%s'\n", var, (char *)msgb_l2(resp));
	}

	msgb_free(resp);
	return NULL;
}

int simple_ctrl_set(struct simple_ctrl_handle *sch, const char *var, const char *val)
{
	struct msgb *msg = msgb_alloc_headroom(512+8, 8, "CTRL SET");
	struct msgb *resp;
	unsigned int rx_id;
	char *rx_var, *rx_val;
	int rc;

	if (!msg)
		return -1;

	rc = msgb_printf(msg, "SET %u %s %s", sch->next_id++, var, val);
	if (rc < 0) {
		msgb_free(msg);
		return -1;
	}
	resp = simple_ctrl_xceive(sch, msg);
	if (!resp)
		return -1;

	if (sscanf(msgb_l2(resp), "SET_REPLY %u %ms %ms", &rx_id, &rx_var, &rx_val) == 3) {
		if (rx_id == sch->next_id-1 && !strcmp(var, rx_var) && !strcmp(val, rx_val)) {
			free(rx_val);
			free(rx_var);
			msgb_free(resp);
			return 0;
		} else {
			free(rx_val);
			free(rx_var);
		}
	} else {
		fprintf(stderr, "CTRL: ERROR: SET(%s=%s) results in '%s'\n", var, val, (char *) msgb_l2(resp));
	}

	msgb_free(resp);
	return -1;
}
