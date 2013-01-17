/*
 * This file contains GBE related definitions
 */
#ifndef PRISM_GBE_H
#define PRISM_GBE_H

#include <config.h>
#include <common.h>
#include <command.h>
#include <malloc.h>
#include <pci.h>
#include <net.h>

#include "mvTypes.h"
#include "mvCtrlEnvLib.h"
#include "boardEnv/mvBoardEnvLib.h"

#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)
#include "eth-phy/mvEthPhy.h"
#endif
#if defined(CONFIG_MV_ETH_LEGACY)
#include "eth/mvEth.h"
#include "eth/gbe/mvEthDebug.h"
#else
#include "neta/gbe/mvNeta.h"
#endif /*  CONFIG_MV_ETH_LEGACY */

#if defined(GBE_CMD_DBG)
	#if defined(PRISM_DBG)
		#undef PRISM_DBG
	#endif
	#define PRISM_DBG(format, args...)  \
		printf("%s: "format"\n", __func__, ## args)
#elif !defined(PRISM_DBG)
	#define PRISM_DBG(format, args...)
#endif

#if (defined(CONFIG_POST) && defined(CONFIG_SYS_POST_ETHER))
	#define GBE_LOG(format, args...)
	#define GBE_ERR_LOG(format, args...) \
		post_log("GE: "format"\n", ## args)
#else
	#define GBE_LOG(format, args...)  printf("GE: "format"\n", ## args)
	#define GBE_ERR_LOG               GBE_LOG
#endif  /* end of CONFIG_POST & CONFIG_SYS_POST_ETHER */

#define MAX_PACKET_LENGTH    256				/* Exclude CRC field */
static uchar *prism_tx_buf = NULL;
static uchar *prism_rx_buf = NULL;

static int prism_tx_len, prism_rx_len;

#define GBE_DIAG_CONTINUE   1
#define GBE_DIAG_SUCCESS    2
#define GBE_DIAG_FAIL       3
static int	gbe_res = GBE_DIAG_CONTINUE;

#define GBE_ETH_REG(reg_addr, reg_name) \
	PRISM_DBG("%-32s: 0x%x = 0x%08x", reg_name, reg_addr, MV_REG_READ(reg_addr))

static void prism_dbgPrintEthRegs(int port)
{
#if defined(GBE_CMD_DUMP_REGS)
	GBE_ETH_REG(ETH_PORT_STATUS_REG(port),        "ETH_PORT_STATUS_REG");
	GBE_ETH_REG(ETH_PORT_SERIAL_CTRL_REG(port),   "ETH_PORT_SERIAL_CTRL_REG");
	GBE_ETH_REG(NETA_GMAC_CTRL_0_REG(port),       "NETA_GMAC_CTRL_0_REG");
	GBE_ETH_REG(NETA_GMAC_CTRL_1_REG(port),       "NETA_GMAC_CTRL_1_REG");
	GBE_ETH_REG(NETA_GMAC_CTRL_2_REG(port),       "NETA_GMAC_CTRL_2_REG");
	GBE_ETH_REG(NETA_GMAC_AN_CTRL_REG(port),      "NETA_GMAC_AN_CTRL_REG");
	GBE_ETH_REG(NETA_GMAC_STATUS_REG(port),       "NETA_GMAC_STATUS_REG");
	GBE_ETH_REG(NETA_GMAC_SERIAL_REG(port),       "NETA_GMAC_SERIAL_REG");
	GBE_ETH_REG(NETA_GMAC_FIFO_PARAM_0_REG(port), "NETA_GMAC_FIFO_PARAM_0_REG");
	GBE_ETH_REG(NETA_GMAC_FIFO_PARAM_1_REG(port), "NETA_GMAC_FIFO_PARAM_1_REG");
	GBE_ETH_REG(NETA_GMAC_CAUSE_REG(port),        "NETA_GMAC_CAUSE_REG");
	GBE_ETH_REG(NETA_GMAC_MASK_REG(port),         "NETA_GMAC_MASK_REG");
	GBE_ETH_REG(NETA_GMAC_MIB_CTRL_REG(port),     "NETA_GMAC_MIB_CTRL_REG");
	GBE_ETH_REG(0x188a0,     "NETA_GBE_PHY_CTRL_0_REG");
	GBE_ETH_REG(0x188a4,     "NETA_GBE_PHY_CTRL_2_REG");
	GBE_ETH_REG(0x188a8,     "NETA_GBE_PHY_STATUS_REG");
#endif
}

#define NAME_MAX_SIZE  (NAMESIZE + 1)
static struct eth_device *gbe_ether_init(int port)
{
	struct eth_device *dev = NULL;
	MV_8 name[NAME_MAX_SIZE];
	bd_t *bd = gd->bd;

	sprintf(name, "egiga%d", port);
	dev = eth_get_dev_by_name(name);
	if (dev == NULL) {
		GBE_ERR_LOG("%s open failed\n", name);
		return NULL;
	}
	else
		GBE_LOG("name = %s, enetaddr = %pM, state = 0x%x",
				dev->name, dev->enetaddr, dev->state);

	dev->halt(dev);
	if (!dev->init(dev, bd)) {
		GBE_ERR_LOG("%s init failed\n", name);
		return NULL;
	}

	return dev;
}

static int gbe_set_force_up_link(int port, MV_BOOL *isChanged)
{
	int res = 0;
	MV_U32 value;

	do {
		*isChanged = MV_FALSE;		/* default "force link up" is not changed */

		if (mvNetaLinkIsUp(port) == MV_TRUE)
			break;

		/* Check if the bits are set */
		value = MV_REG_READ(NETA_GMAC_AN_CTRL_REG(port));
		if (value & NETA_FORCE_LINK_PASS_MASK) {
			break;		/* The bits are set. exit */
		}

		/* Set "force link up" bit */
		if ((res = mvNetaForceLinkModeSet(port, 1, 0)) != 0) {
			PRISM_DBG("mvNetaForceLinkModeSet() failed");
			break;
		}
		*isChanged = MV_TRUE;	/* The "force link up" is changed */
	} while (0);

	return res;
}

static int gbe_clear_force_up_link(int port, MV_BOOL isChanged)
{
	int res = 0;

	if (isChanged == MV_TRUE) {
		/* Clear force link up */
		if ((res = mvNetaForceLinkModeSet(port, 0, 0)) != 0)
			PRISM_DBG("mvNetaForceLinkModeSet() failed");
	}
	return res;
}


static int mvNetaGmiiLoopback(int port, MV_BOOL isLpbk)
{
	MV_U32 value;
	int    res = 1;		/* default fail */

	if ((res = mvNetaPortCheck(port)) != 0)
		return res;

	value = MV_REG_READ(NETA_GMAC_CTRL_1_REG(port));
	if (isLpbk == MV_TRUE) {
		/* Enable GMII loopback */
		value |= NETA_GMAC_LOOPBACK_EN_MASK;
	} else {
		/* Disable GMII loopback */
		value &= ~NETA_GMAC_LOOPBACK_EN_MASK;
	}
	MV_REG_WRITE(NETA_GMAC_CTRL_1_REG(port), value);

	return res;
}

/* isLpbk: 1 - loopback mode; 0 - normal */
static int gbe_set_loopback_mode(struct eth_device *dev, int port, MV_BOOL isLpbk)
{
	int    res = 1;		/* default fail */
	MV_U32 value;
	MV_ETH_PORT_SPEED  config_speed;
	MV_ETH_PORT_DUPLEX config_duplex;
	MV_ETH_PORT_FC     config_fc;

	do {
		/* Disable Port */
		if ((res = mvNetaPortDisable(port)) != 0) {
			PRISM_DBG("mvNetaPortDisable() failed");
			break;
		}

		/* Force to link down */
		if ((res = mvNetaForceLinkModeSet(port, 0, 1)) != 0) {
			PRISM_DBG("mvNetaForceLinkModeSet() failed");
			break;
		}

		if (isLpbk == MV_TRUE) {
			/* Per Marvell,
			 * - Disable AutoNeg(Speed/FC/Duplex): Address 0x72C0C bit 7,11,13 (0x0)
			 * - Force FC Disable: Address 0x72C0C bit 8 (0x0)
			 * - Force Full Duplex: Address 0x72C0C bit 12 (0x1)
			 */
			config_speed  = MV_ETH_SPEED_1000;
			config_duplex = MV_ETH_DUPLEX_FULL;
			config_fc     = MV_ETH_FC_DISABLE;
		} else {
			/* Set to normal mode */
			config_speed  = MV_ETH_SPEED_AN;
			config_duplex = MV_ETH_DUPLEX_AN;
			config_fc     = MV_ETH_FC_AN_SYM;
		}

		if ((res = mvNetaSpeedDuplexSet(port, config_speed, config_duplex)) != 0) {
			PRISM_DBG("mvNetaForceLinkModeSet() failed");
			break;
		}

		if ((res = mvNetaFlowCtrlSet(port, config_fc)) != 0) {
			PRISM_DBG("mvNetaFlowCtrlSet() failed");
			break;
		}

		prism_dbgPrintEthRegs(port);

		if ((res = mvNetaGmiiLoopback(port, isLpbk)) != 0) {
			PRISM_DBG("mvNetaGmiiLoopback() failed");
			break;
		}

		/* Force link up */
		if ((res = mvNetaForceLinkModeSet(port, 1, 0)) != 0) {
			PRISM_DBG("mvNetaForceLinkModeSet() failed");
			break;
		}

		res = mvNetaPortEnable(port);
		if (res != 0) {
			PRISM_DBG("mvNetaPortEnable() failed");
			break;
		}

		PRISM_DBG ("looplback %s.....", (isLpbk == MV_TRUE)? "enabled": "disabled");
		prism_dbgPrintEthRegs(port);

		value = MV_REG_READ(NETA_GMAC_STATUS_REG(port));
		if (!(value & NETA_GMAC_LINK_UP_MASK)) {
			PRISM_DBG("egiga%d link is down (value=0x%x)", port, value);
			break;
		}

		prism_tx_len = MAX_PACKET_LENGTH;
		prism_rx_len = 0;
	} while (0);

	return res;
}

static void gbe_dbg_dump_packet(uchar *packet, int len, int off)
{
#ifdef ETH_DEBUG_DUMP
	int i;

	printf("packet data(len = %d, off=%d)\n", len, off);
	for (i = off; i < len; i++) {
		if (i % 16 == 0)
			printf("\n");
		printf("%02x ", packet[i]);
	}
	printf("\n");
#endif
}

#define GBE_LPKB_PKT_HDR_LEN	(ETHER_HDR_SIZE + IP_HDR_SIZE)
static int gbe_packet_check(void)
{
	int res = 1;
	int i;
	MV_BOOL isMiscompare = MV_FALSE;

	PRISM_DBG("%s is called", __func__);

	do {
		if (prism_tx_len != prism_rx_len) {
			GBE_ERR_LOG("mismatch packet lengths: prism_tx_len=%d, prism_rx_len=%d",
			            prism_tx_len, prism_rx_len);
			break;
		}

		for (i = GBE_LPKB_PKT_HDR_LEN; i < prism_tx_len; i++) {
			if (prism_tx_buf[i] != prism_rx_buf[i]) {
				isMiscompare = MV_TRUE;
				GBE_ERR_LOG("mismatch: off=0x%x, expected data=0x%02x, recv_data=0x%02x",
				            i, prism_tx_buf[i], prism_rx_buf[i]);
				break;
			}
		}

		if (isMiscompare == MV_FALSE)
			res = 0;
	} while (0);

	if (res != 0)
		GBE_ERR_LOG("rx/tx data miscompare error");

	return res;
}

static void gbe_recv_packet(uchar *packet, unsigned dest, unsigned src, unsigned len)
{
	int i, j;

	PRISM_DBG("%s is called (len=%d)", __func__, len);
	gbe_dbg_dump_packet(packet, len, 0);

	/* Note -
	 * Due to the loopback packet is an IP frame,
	 * 1) the input  packet len is IP payload data only
	 *    len = prism_tx_len - ETHER_HDR_SIZE - IP_HDR_SIZE
	 * 2) In prism_rx_len, there is no eth and IP header data
	 *    Copy data in "packet" to starting offset of payload in prism_rx_buf
	 */
	if ((len + GBE_LPKB_PKT_HDR_LEN) <= MAX_PACKET_LENGTH) {
		prism_rx_len = len + GBE_LPKB_PKT_HDR_LEN;

		for (i = 0, j = GBE_LPKB_PKT_HDR_LEN; i < len; i++, j++) {
			prism_rx_buf[j] = packet[i];
		}

		gbe_dbg_dump_packet(prism_rx_buf, prism_rx_len, GBE_LPKB_PKT_HDR_LEN);
		gbe_res = GBE_DIAG_SUCCESS;
	}	else {
		GBE_ERR_LOG("failed to reveice data(%d)", len);
		gbe_res = GBE_DIAG_FAIL;
	}
}

/* Unused port numbers for loopback test */
#define PORT_UNUSED_S  48222	 /* Unused port number */
#define PORT_UNUSED_C  48223   /* Unused port number */
#define MAC_ADDR_LEN   6
static int gbe_send_packet(struct eth_device *dev)
{
	int i;
	int rand_data = (int)get_timer(0);

	/* Set up ethernet header */
	/* To minimize the modification, setup the packet to
	 * be an IP protocol frame which is known by NetLoop()
	 */
	int payload_len;

	memcpy(NetOurEther, dev->enetaddr, MAC_ADDR_LEN);
	i = NetSetEther(prism_tx_buf, NetBcastAddr, PROT_IP);
	payload_len = prism_tx_len - i - IP_HDR_SIZE;
	NetSetIP((prism_tx_buf + i), 0xFFFFFFFFL,
	         PORT_UNUSED_S, PORT_UNUSED_C, payload_len);

	i += IP_HDR_SIZE;
	for ( ; i < prism_tx_len; i++) {
		if (i >= ETHER_HDR_SIZE)
			prism_tx_buf[i] = (char)(rand_data + i);
	}

	gbe_dbg_dump_packet(prism_tx_buf, prism_tx_len, 0);
	NetSetHandler(gbe_recv_packet);

	return dev->send(dev, prism_tx_buf, prism_tx_len);
}

static int gbe_loopback_init(struct eth_device **dev, int port)
{
	int res = 1;

	do {
		gbe_res = GBE_DIAG_CONTINUE;

		*dev = gbe_ether_init(port);
		if (*dev == NULL)
			break;

		prism_tx_buf = malloc(MAX_PACKET_LENGTH);
		prism_rx_buf = malloc(MAX_PACKET_LENGTH);
		if (!prism_tx_buf || !prism_rx_buf) {
			GBE_ERR_LOG("Failed to allocate packet buffers\n");
			break;
		}

		res = 0;
	} while (0);

	return res;
}

static void gbe_loopback_uninit(struct eth_device *dev, int port, MV_BOOL isLpbk)
{
	if (dev != NULL) {
		if (isLpbk == MV_TRUE) {
			/* set to normal mode */
			gbe_set_loopback_mode(dev, port, MV_FALSE);
		}
		dev->halt(dev);
	}

	if (prism_tx_buf != NULL) {
		free(prism_tx_buf);
		prism_tx_buf = NULL;
	}

	if (prism_rx_buf != NULL) {
		free(prism_rx_buf);
		prism_rx_buf = NULL;
	}

	/* Set the routine ptr to NULL */
	NetSetHandler(NULL);

	return;
}

static int gbe_loopback_test(int port, MV_BOOL isLpbk)
{
	struct eth_device *dev;
	int res = 1;
	int i;
	MV_BOOL isForceLinkUpChanged = MV_FALSE;

	do {
		PRISM_DBG("--> normal mode \n");
		prism_dbgPrintEthRegs(port);

		if (isLpbk == MV_TRUE) {
			/* For gbe loopback diag command, we would like to run w/o link also.
			 * Let set to "force link up"
			 */
			if ((res = gbe_set_force_up_link(port, &isForceLinkUpChanged)) != 0) {
				break;
			}
		}

		if ((res = gbe_loopback_init(&dev, port)) != 0)
			break;

		if (isLpbk == MV_TRUE) {
			res = gbe_set_loopback_mode(dev, port, MV_TRUE);
			if (res != 0)
				break;		/* failed. exit */

			/* send test packet */
			res = gbe_send_packet(dev);
			if (res != 0) {
				GBE_ERR_LOG("egiga%d test failed\n", port);
				break;
			}
		} else {
			/* test receive only */
			NetSetHandler(gbe_recv_packet);
		}

		/* Wait until either timeout or packet received */
		for (i = 30; (i > 0) && (gbe_res == GBE_DIAG_CONTINUE); i--) {
			/*
			 * Check the ethernet for a new packet. If a packet is received,
			 * the gbe_recv_packet() will be invoked.
			 */
			dev->recv(dev);
			mvOsSleep(5);		/* delay for 5ms */
		}

		if (gbe_res == GBE_DIAG_CONTINUE) {
			GBE_ERR_LOG("failed to receive timed out (gbe_res=%d)\n", gbe_res);
			res = 1;
		}

		if ((gbe_res == GBE_DIAG_SUCCESS) && (isLpbk == MV_TRUE)) {
			res = gbe_packet_check();
		}

	} while (0);

	gbe_loopback_uninit(dev, port, isLpbk);
	gbe_clear_force_up_link(port, isForceLinkUpChanged);

	return res;
}

#endif /* end of ifndef PRISM_GBE_H */

