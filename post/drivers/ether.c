/*
 * (C) Copyright 2007
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Author: Igor Lisitsin <igor@emcraft.com>
 *
 * Used post/cpu/ppc4xx/ether.c as the template and rewrote based on
 * Marvell 88F6601 chip.
 *
 * Author: Xiaofeng Wei <weixiaofeng@google.com>
 *         Alice Wang <alicejwang@google.com>
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
#if defined(CONFIG_ARM) && defined(CONFIG_MARVELL)

#include "mvCommon.h"
#if defined(CONFIG_MV_ETH_LEGACY)
#include "eth/mvEth.h"
#include "mvBoardEnvLib.h"
#else
#include "neta/gbe/mvNeta.h"
#endif /*  CONFIG_MV_ETH_LEGACY */

#include "prism_gbe.h"

DECLARE_GLOBAL_DATA_PTR;

int ether_post_test(int flags)
{
	int i, ports, res = 0;

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
			res = gbe_loopback_test(i, MV_TRUE);
		else {
			post_log("GE: illegal test flags: 0x%x\n", flags);
			return -1;
		}
	}
	return res;
}

#endif /* defined(CONFIG_ARM) && defined(CONFIG_MARVELL) */
#endif /* CONFIG_POST & CONFIG_SYS_POST_ETHER */
