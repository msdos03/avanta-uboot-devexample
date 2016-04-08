/*
 * Copyright 2009(C) Marvell International Ltd. and its affiliates
 * Prafulla Wadaskar <prafulla@marvell.com>
 *
 * Based on drivers/mtd/spi/stmicro.c
 *
 * Copyright 2008, Network Appliance Inc.
 * Jason McMullan <mcmullan@netapp.com>
 *
 * Copyright (C) 2004-2007 Freescale Semiconductor, Inc.
 * TsiChung Liew (Tsi-Chung.Liew@freescale.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <common.h>
#include <malloc.h>
#include <spi_flash.h>

#include "spi_flash_internal.h"
/*#define debug(fmt,args...)	printf (fmt ,##args)*/
/* MX25xx-specific commands */
#define CMD_MX25XX_WREN		0x06	/* Write Enable */
#define CMD_MX25XX_WRDI		0x04	/* Write Disable */
#define CMD_MX25XX_RDSR		0x05	/* Read Status Register */
#define CMD_MX25XX_WRSR		0x01	/* Write Status Register */
#define CMD_MX25XX_READ		0x03	/* Read Data Bytes */
#define CMD_MX25XX_FAST_READ	0x0b	/* Read Data Bytes at Higher Speed */
#define CMD_MX25XX_PP		0x02	/* Page Program */
#define CMD_MX25XX_SE		0x20	/* Sector Erase */
#define CMD_MX25XX_BE		0xD8	/* Block Erase */
#define CMD_MX25XX_CE		0xc7	/* Chip Erase */
#define CMD_MX25XX_DP		0xb9	/* Deep Power-down */
#define CMD_MX25XX_RES		0xab	/* Release from DP, and Read Signature */
#define CMD_MX25XX_EN4B		0xb7	/* Enter 4-byte mode */

#define MACRONIX_SR_WIP		(1 << 0)	/* Write-in-Progress */

#define MX_PROTECT_ALL		0x3C
#define MX_SRWD			0x80

struct macronix_spi_flash_params {
	u16 idcode;
	u16 page_size;
	u16 pages_per_sector;
	u16 sectors_per_block;
	u16 nr_blocks;
	u8  addr_cycles;
	const char *name;
};

struct macronix_spi_flash {
	struct spi_flash flash;
	const struct macronix_spi_flash_params *params;
};

static inline struct macronix_spi_flash *to_macronix_spi_flash(struct spi_flash
							       *flash)
{
	return container_of(flash, struct macronix_spi_flash, flash);
}

static struct macronix_spi_flash_params macronix_spi_flash_table[] = {
	{
		.idcode = 0x2015,
		.page_size = 256,
		.pages_per_sector = 16,
		.sectors_per_block = 16,
		.nr_blocks = 32,
		.addr_cycles = 3,
		.name = "MX25L1605D",
	},
	{
		.idcode = 0x2016,
		.page_size = 256,
		.pages_per_sector = 16,
		.sectors_per_block = 16,
		.nr_blocks = 64,
		.addr_cycles = 3,
		.name = "MX25L3205D",
	},
	{
		.idcode = 0x2017,
		.page_size = 256,
		.pages_per_sector = 16,
		.sectors_per_block = 16,
		.nr_blocks = 128,
		.addr_cycles = 3,
		.name = "MX25L6405D",
	},
	{
		.idcode = 0x2018,
		.page_size = 256,
		.pages_per_sector = 16,
		.sectors_per_block = 16,
		.nr_blocks = 256,
		.addr_cycles = 3,
		.name = "MX25L12805D",
	},
	{
		.idcode = 0x2019,
		.page_size = 256,
		.pages_per_sector = 16,
		.sectors_per_block = 16,
		.nr_blocks = 512,
		.addr_cycles = 3,
		.name = "MX25L25635E",
	},
	{
		.idcode = 0x2618,
		.page_size = 256,
		.pages_per_sector = 16,
		.sectors_per_block = 16,
		.nr_blocks = 256,
		.addr_cycles = 3,
		.name = "MX25L12855E",
	},
};

static int macronix_rdsr(struct spi_flash *flash)
{
	struct spi_slave *spi = flash->spi;
	int ret;
	u8 status;
	u8 buf[1];
	u8 cmd[1];

	cmd[0] = CMD_MX25XX_RDSR;
	spi_flash_read_common(flash, cmd, 1, buf, 1);

	printf("RDSR:%x \n", buf[0]);

	return 0;
}

static int macronix_wait_ready(struct spi_flash *flash, unsigned long timeout)
{
	struct spi_slave *spi = flash->spi;
	unsigned long timebase;
	int ret;
	u8 status;
	u8 cmd = CMD_MX25XX_RDSR;

	ret = spi_xfer(spi, 8, &cmd, NULL, SPI_XFER_BEGIN);
	if (ret) {
		debug("SF: Failed to send command %02x: %d\n", cmd, ret);
		return ret;
	}

	timebase = get_timer(0);
	do {
		ret = spi_xfer(spi, 8, NULL, &status, 0);
		if (ret)
			return -1;

		if ((status & MACRONIX_SR_WIP) == 0)
			break;

	} while (get_timer(timebase) < timeout);

	spi_xfer(spi, 0, NULL, NULL, SPI_XFER_END);

	if ((status & MACRONIX_SR_WIP) == 0)
		return 0;

	/* Timed out */
	return -1;
}

static int macronix_read_fast(struct spi_flash *flash,
			      u32 offset, size_t len, void *buf)
{
	struct macronix_spi_flash *mcx = to_macronix_spi_flash(flash);
	unsigned long page_addr;
	unsigned long page_size;
	u8 cmd[6];

	page_size = mcx->params->page_size;
	page_addr = offset / page_size;

	cmd[0] = CMD_READ_ARRAY_FAST;
	switch  (mcx->params->addr_cycles) {
		case 4:
			cmd[1] = page_addr >> 16;
			cmd[2] = page_addr >> 8;
			cmd[3] = page_addr;
			cmd[4] = offset % page_size;
			cmd[5] = 0x00;
			break;
		case 3:
		default:
			cmd[1] = page_addr >> 8;
			cmd[2] = page_addr;
			cmd[3] = offset % page_size;
			cmd[4] = 0x00;
			break;
	}

	return spi_flash_read_common(flash, cmd, mcx->params->addr_cycles + 2, buf, len);
}

static int macronix_write(struct spi_flash *flash,
			  u32 offset, size_t len, const void *buf)
{
	struct macronix_spi_flash *mcx = to_macronix_spi_flash(flash);
	unsigned long page_addr;
	unsigned long byte_addr;
	unsigned long page_size;
	size_t chunk_len;
	size_t actual;
	int ret;
	u8 cmd[5];

	page_size = mcx->params->page_size;
	page_addr = offset / page_size;
	byte_addr = offset % page_size;

	ret = spi_claim_bus(flash->spi);
	if (ret) {
		debug("SF: Unable to claim SPI bus\n");
		return ret;
	}

	ret = 0;
	for (actual = 0; actual < len; actual += chunk_len) {
		chunk_len = min(len - actual, page_size - byte_addr);

		cmd[0] = CMD_MX25XX_PP;
		switch  (mcx->params->addr_cycles) {
			case 4:
				cmd[1] = page_addr >> 16;
				cmd[2] = page_addr >> 8;
				cmd[3] = page_addr;
				cmd[4] = byte_addr;
				break;
			case 3:
			default:
				cmd[1] = page_addr >> 8;
				cmd[2] = page_addr;
				cmd[3] = byte_addr;
				break;
		}

		debug
		    ("PP: 0x%p => cmd = { 0x%02x 0x%02x%02x%02x%02x } chunk_len = %d\n",
		     buf + actual, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], chunk_len);

		ret = spi_flash_cmd(flash->spi, CMD_MX25XX_WREN, NULL, 0);
		if (ret < 0) {
			debug("SF: Enabling Write failed\n");
			break;
		}

		ret = spi_flash_cmd_write(flash->spi, cmd, mcx->params->addr_cycles + 1,
					  buf + actual, chunk_len);
		if (ret < 0) {
			debug("SF: Macronix Page Program failed\n");
			break;
		}

		ret = macronix_wait_ready(flash, SPI_FLASH_PROG_TIMEOUT);
		if (ret < 0) {
			debug("SF: Macronix page programming timed out\n");
			break;
		}

		page_addr++;
		byte_addr = 0;
	}

	debug("SF: Macronix: Successfully programmed %u bytes @ 0x%x\n",
	      len, offset);

	spi_release_bus(flash->spi);
	return ret;
}

int macronix_erase(struct spi_flash *flash, u32 offset, size_t len)
{
	struct macronix_spi_flash *mcx = to_macronix_spi_flash(flash);
	unsigned long sector_size;
	size_t current;
	int ret;
	u8 cmd[5];

	/*
	 * This function currently uses sector erase only.
	 * probably speed things up by using bulk erase
	 * when possible.
	 */

	sector_size = mcx->params->page_size * mcx->params->pages_per_sector
			* mcx->params->sectors_per_block;

	if (offset % sector_size || len % sector_size) {
		printf("SF: Erase offset/length not multiple of sector size\n");
		return -1;
	}

	len /= sector_size;
	cmd[0] = CMD_MX25XX_BE;
	switch  (mcx->params->addr_cycles) {
		case 3:
		default:
			cmd[2] = 0x00;
		case 4:
			cmd[3] = 0x00;
			cmd[4] = 0x00;
			break;
	}

	ret = spi_claim_bus(flash->spi);
	if (ret) {
		debug("SF: Unable to claim SPI bus\n");
		return ret;
	}

	ret = 0;
	for (current = 0; current < len; current++) {
		switch (mcx->params->addr_cycles) {
			case 3:
			default:
				cmd[1] = (offset>>16) + ((sector_size>>16) * current);
				break;
			case 4:
				cmd[1] = (offset + (sector_size * current)) >> 24;
				cmd[2] = (offset + (sector_size * current)) >> 16;
				break;
		}

		debug ("Erase => cmd = { 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x } current sector = %d\n",
				 cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], current);

		ret = spi_flash_cmd(flash->spi, CMD_MX25XX_WREN, NULL, 0);
		if (ret < 0) {
			debug("SF: Enabling Write failed\n");
			break;
		}

		ret = spi_flash_cmd_write(flash->spi, cmd, mcx->params->addr_cycles + 1, NULL, 0);
		if (ret < 0) {
			debug("SF: Macronix page erase failed\n");
			break;
		}

		ret = macronix_wait_ready(flash, SPI_FLASH_PAGE_ERASE_TIMEOUT);
		if (ret < 0) {
			debug("SF: Macronix page erase timed out\n");
			break;
		}
	}

	debug("SF: Macronix: Successfully erased %u bytes @ 0x%x\n",
	      len * sector_size, offset);

	spi_release_bus(flash->spi);
	return ret;
}

#ifdef CONFIG_SPI_FLASH_PROTECTION
int macronix_protect(struct spi_flash *flash, int enable)
{
	//struct macronix_spi_flash *mcx = to_macronix_spi_flash(flash);
	int ret;
	u8 cmd[2];
	u8 buf[1];


	debug("marcronix_protect: %d\n", enable);
	ret = spi_claim_bus(flash->spi);
	if (ret) {
		debug("SF: Unable to claim SPI bus\n");
		return ret;
	}
	cmd[0] = CMD_MX25XX_RDSR;
	spi_flash_read_common(flash, cmd, 1, buf, 1);


	debug("SF: RDSR value 0x%x\n", buf[1]);
	ret = 0;

	cmd[0] = CMD_MX25XX_WRSR;

	switch(enable) {
	case 1:
		cmd[1] = MX_SRWD | MX_PROTECT_ALL;
		break;
	case 0:
		cmd[1] = MX_SRWD;
		break;
	default:
	case -1:
		cmd[1] = 0;
	}

	ret = spi_flash_cmd(flash->spi, CMD_MX25XX_WREN, NULL, 0);
	if (ret < 0) {
		debug("SF: Enabling Write failed\n");
		return ret;
	}

	ret = spi_flash_cmd_write(flash->spi, cmd, 2, NULL, 0);
	if (ret < 0) {
		debug("SF: Macronix Write Status Register failed\n");
		return ret;
	}

	ret = macronix_wait_ready(flash, SPI_FLASH_PROG_TIMEOUT);
	if (ret < 0) {
		debug("SF: Macronix page programming timed out\n");
		return ret;
	}

/*	debug("SF: Macronix: Successfully Write Status Register %u bytes @ 0x%x\n",
	      len, offset);*/

	spi_release_bus(flash->spi);
	return ret;
}


int macronix_lock(struct spi_flash *flash, int enable)
{
	debug("macronix_lock\n");
	return 0;
}
#endif

struct spi_flash *spi_flash_probe_macronix(struct spi_slave *spi, u8 *idcode)
{
	struct macronix_spi_flash_params *params;
	struct macronix_spi_flash *mcx;
	unsigned int i;
	u16 id = idcode[2] | idcode[1] << 8;

	for (i = 0; i < ARRAY_SIZE(macronix_spi_flash_table); i++) {
		params = &macronix_spi_flash_table[i];
		if (params->idcode == id)
			break;
	}

	if (i == ARRAY_SIZE(macronix_spi_flash_table)) {
		debug("SF: Unsupported Macronix ID %04x\n", id);
		return NULL;
	}

	mcx = malloc(sizeof(*mcx));
	if (!mcx) {
		debug("SF: Failed to allocate memory\n");
		return NULL;
	}

	mcx->params = params;
	mcx->flash.spi = spi;
	mcx->flash.name = params->name;

	mcx->flash.write = macronix_write;
	mcx->flash.erase = macronix_erase;
	mcx->flash.read = macronix_read_fast;
#ifdef CONFIG_SPI_FLASH_PROTECTION
	mcx->flash.protect = macronix_protect;
	mcx->flash.lock = macronix_lock;
#endif
	mcx->flash.size = (params->page_size * params->pages_per_sector
	    * params->sectors_per_block * params->nr_blocks);

	macronix_rdsr(&mcx->flash);
	/* enable 4-byte addressing if the device exceeds 16MiB */
	if (mcx->flash.size > 0x1000000) {
		int ret;

		printf("SF: Enabling 4-Byte address mode\n");
		params->addr_cycles = 4;
		ret = spi_claim_bus(mcx->flash.spi);
		if (ret) {
			printf("SF: Unable to claim SPI bus\n");
			goto probe_done;
		}

		ret = spi_flash_cmd(mcx->flash.spi, CMD_MX25XX_EN4B, NULL, 0);
		if (ret < 0) {
			printf("SF: Enabling 4-Byte address mode failed\n");
			spi_release_bus(mcx->flash.spi);
			goto probe_done;
		}

		ret = macronix_wait_ready(&mcx->flash, SPI_FLASH_PROG_TIMEOUT);
		if (ret < 0)
			debug("SF: Macronix page programming timed out\n");

		spi_release_bus(mcx->flash.spi);
	}

probe_done:
	printf("SF: Detected %s with page size %u, total %u bytes\n",
	      params->name, params->page_size, mcx->flash.size);

	return &mcx->flash;
}
