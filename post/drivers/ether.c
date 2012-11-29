/*
 * (C) Copyright 2007
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Author: Igor Lisitsin <igor@emcraft.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <post.h>
#include <common.h>
#include <net.h>

#if CONFIG_POST & CONFIG_SYS_POST_ETHER

#include "mvCommon.h"
#include "eth/mvEth.h"
#include "mvBoardEnvLib.h"

DECLARE_GLOBAL_DATA_PTR;

#define ETH_DEBUG_DUMP

#define MAX_PACKET_LENGTH		256

char tx_buf[MAX_PACKET_LENGTH];
char rx_buf[MAX_PACKET_LENGTH];
int tx_len, rx_len;

static struct eth_device *ether_init(int port)
{
	struct eth_device *dev;
	MV_8 name[NAMESIZE + 1];
	bd_t *bd = gd->bd;

	sprintf(name, "egiga%d", port);
	dev = eth_get_dev_by_name(name);
	if (dev == NULL)
		post_log("GE: %s open failed\n", name);
	else
		printf("GE: name = %s, enetaddr = %pM, state = 0x%x\n",
			dev->name, dev->enetaddr, dev->state);

	if (!dev->init(dev, bd)) {
		post_log("GE: %s init failed\n", name);
		return NULL;
	}

	return dev;
}

#ifdef ETH_DEBUG_DUMP
static void dump_packet(char *packet, int len)
{
	int i;

	printf("GE: packet data(%d):", len);
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			printf("\n");
		printf("%02x ", packet[i]);
	}
	printf("\n");
}
#endif

static int packet_check()
{
	int i;

	if (tx_len != rx_len)
		return -1;

	for (i = 14; i < tx_len; i++) {
		if (tx_buf[i] != rx_buf[i])
			return -1;
	}

	return 0;
}

static void recv_packet(char *packet, int dest, int src, int len)
{
	int i;

	if (len < MAX_PACKET_LENGTH) {
		rx_len = len;
		for (i = 0; i < rx_len; i++) {
			rx_buf[i] = packet[i];
		}

#ifdef ETH_DEBUG_DUMP
		dump_packet(rx_buf, rx_len);
#endif
	}
	else {
		post_log("GE: failed to reveice data(%d)\n", len);
	}
}

static int send_packet(struct eth_device *dev)
{
	int i;

	/* set up ethernet header */
	memset(tx_buf, 0xff, 14);

	for (i = 0; i < tx_len; i++) {
		if (i >= 14)
			tx_buf[i] = tx_len + i;
	}

#ifdef ETH_DEBUG_DUMP
	dump_packet(tx_buf, tx_len);
#endif

	return dev->send(dev, tx_buf, tx_len);
}

static int ether_auto_test(int port)
{
	int speed;
	MV_U32 value;

	value = MV_REG_READ(NETA_GMAC_STATUS_REG(port));

	if (!(value & NETA_GMAC_LINK_UP_MASK)) {
		post_log("GE: egiga%d link was down\n", port);
		return -1;
	}

	if (value & NETA_GMAC_SPEED_1000_MASK)
		speed = 1000;
	else if (value & NETA_GMAC_SPEED_100_MASK)
		speed = 100;
	else
		speed = 10;

	/* Link, Speed, Duplex */
	printf("GE: egiga%d Link = %s, Speed = %d, Duplex = %s\n",
		port, (value & NETA_GMAC_LINK_UP_MASK) ? "up" : "down", speed,
		(value & NETA_GMAC_FULL_DUPLEX_MASK) ? "full" : "half");

	return 0;
}

static int ether_manual_test(int port)
{
	struct eth_device *dev;
	int res = 0;

	dev = ether_init(port);
	if (dev == NULL)
		return -1;

	tx_len = MAX_PACKET_LENGTH;
	rx_len = 0;

	/* send test packet */
	res = send_packet(dev);

	dev->halt(dev);

	if (res != 0) {
		post_log("GE: egiga%d test failed\n", port);
	}

	return res;
}

int ether_post_test(int flags)
{
	int i, ports, res = 0;

	printf("%s: flags=0x%X\n", __func__, flags);

	/* get port number */
	ports = mvCtrlEthMaxPortGet();
	if (ports > MV_ETH_MAX_PORTS) {
		post_log("GE: illegal port number: %d\n", ports);
		return -1;
	}

	for (i = 0; i < ports && res == 0; i++) {
		/* check power status of prts */
		if (mvCtrlPwrClckGet(ETH_GIG_UNIT_ID, i) == MV_FALSE) {
			post_log("GE: port%d power test failed\n", i);
			continue;
		}

		if (i == MV_PON_PORT_ID)
			continue;

		if (flags & POST_POWERON)
			res = ether_auto_test(i);
		else if (flags & POST_MANUAL)
			res = ether_manual_test(i);
		else {
			post_log("GE: illegal test flags: 0x%x\n", flags);
			return -1;
		}
	}

	return res;
}

#endif /* CONFIG_POST & CONFIG_SYS_POST_ETHER */
