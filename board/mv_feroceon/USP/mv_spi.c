/*
 * (C) Copyright 2009
 * Marvell Semiconductor <www.marvell.com>
 * Written-by: Prafulla Wadaskar <prafulla@marvell.com>
 *
 * Derived from drivers/spi/mpc8xxx_spi.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */
#include <common.h>
#if defined(CONFIG_CMD_SPI) || defined(CONFIG_CMD_SF)
#include <malloc.h>
#include <spi.h>
#include "mvSysSpiApi.h"
#include "mvOs.h"
#include "spi/mvSpi.h"
#include "spi/mvSpiSpec.h"

void spi_init (void)
{
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
				unsigned int max_hz, unsigned int mode)
{
	struct spi_slave *slave;
//	u32 data;

	if (!spi_cs_is_valid(bus, cs))
		return NULL;

	slave = malloc(sizeof(struct spi_slave));
	if (!slave)
		return NULL;

	slave->bus = bus;
	slave->cs = cs;

// 	writel(~KWSPI_CSN_ACT | KWSPI_SMEMRDY, &spireg->ctrl);
// 
// 	/* calculate spi clock prescaller using max_hz */
// 	data = ((CONFIG_SYS_TCLK / 2) / max_hz) & KWSPI_CLKPRESCL_MASK;
// 	data |= 0x10;
// 
// 	/* program spi clock prescaller using max_hz */
// 	writel(KWSPI_ADRLEN_3BYTE | data, &spireg->cfg);
// 	debug("data = 0x%08x \n", data);
// 
// 	writel(KWSPI_SMEMRDIRQ, &spireg->irq_cause);
// 	writel(KWSPI_IRQMASK, spireg->irq_mask);

	mvSysSpiInit(0,max_hz);

	return slave;
}

void spi_free_slave(struct spi_slave *slave)
{
	free(slave);
}

int spi_claim_bus(struct spi_slave *slave)
{
	return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
}

#ifndef CONFIG_SPI_CS_IS_VALID
/*
 * you can define this function board specific
 * define above CONFIG in board specific config file and
 * provide the function in board specific src file
 */
int spi_cs_is_valid(unsigned int bus, unsigned int cs)
{
	return ((bus >= 0) && (bus <= 1) && (cs >= 0) && (cs <= 7));
}
#endif

void spi_cs_activate(struct spi_slave *slave)
{
	mvSpiCsAssert(0);
	//writel(readl(&spireg->ctrl) | KWSPI_IRQUNMASK, &spireg->ctrl);
}

void spi_cs_deactivate(struct spi_slave *slave)
{
	mvSpiCsDeassert(0);
	//writel(readl(&spireg->ctrl) & KWSPI_IRQMASK, &spireg->ctrl);
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen, const void *dout,
	     void *din, unsigned long flags)
{
	MV_STATUS ret;
	MV_U8* pdout = (MV_U8*)dout;
	MV_U8* pdin = (MV_U8*)din;
#if 0
	int tmp_bitlen = bitlen;
	unsigned int tmpdout, tmpdin;
	int tm, isread = 0;

	debug("spi_xfer: slave %u:%u dout %08X din %08X bitlen %u\n",
	      slave->bus, slave->cs, dout, din, bitlen);

#endif

	//printf("spi_xfer: slave %u:%u dout %08X din %08X bitlen %u\n",
	  //    slave->bus, slave->cs, dout, din, bitlen);

	if (flags & SPI_XFER_BEGIN)
		spi_cs_activate(slave);

#if 0
	MV_STATUS ret;
	MV_U32 bytesLeft = buffSize;
	MV_U16* txPtr = (MV_U16*)pTxBuff;


	/* Check that the buffer pointer and the buffer size are 16bit aligned */
	if (((bitlen & 1) == 0) && ((dout & 1) == 0))
	{
	/* Verify that the SPI mode is in 16bit mode */
	MV_REG_BIT_SET(MV_SPI_IF_CONFIG_REG, MV_SPI_BYTE_LENGTH_MASK);

	/* TX/RX as long we have complete 16bit chunks */
	while (bitlen >= MV_SPI_16_BIT_CHUNK_SIZE)
	{
        /* Transmitted and wait for the transfer to be completed */
		if ((ret = mvSpi16bitDataTxRx(*dout, *din)) != MV_OK)
			return ret;

		/* increment the pointers */
		txPtr++;
		bitlen -= MV_SPI_16_BIT_CHUNK_SIZE;
	}
    }
    else
    {
#endif
	
	/* Verify that the SPI mode is in 8bit mode */
	MV_REG_BIT_RESET(MV_SPI_IF_CONFIG_REG(0), MV_SPI_BYTE_LENGTH_MASK);

#if 0
	/* TX/RX in 8bit chanks */
	
	while (tmp_bitlen > 0)
	{
		/* Transmitted and wait for the transfer to be completed */
		if ((ret = mvSpi8bitDataTxRx(0,*pdout, pdin)) != MV_OK)
			return ret;
		
		/* increment the pointers */
		//printf("in=[0x%x]",*pdin);
		if (pdin)
			pdin++;
		//printf("out=[0x%x]",*pdout);
		if (pdout)
			pdout++;

		tmp_bitlen-=8;
	}
#else
	if (dout && din)
		ret = mvSpiReadWrite(0, din, dout, (bitlen + 7) / 8);
	else if (dout)
		ret = mvSpiWrite(0, dout, (bitlen + 7) / 8);
	else if (din)
		ret = mvSpiRead(0, din, (bitlen + 7) / 8);
	else
		ret = MV_OK;

	if (ret)
		return ret;
#endif

#if 0
    }
#endif
	
#if 0
/*
	 * handle data in 8-bit chunks
	 * TBD: 2byte xfer mode to be enabled
	 */
	writel(((readl(&spireg->cfg) & ~KWSPI_XFERLEN_MASK) |
		KWSPI_XFERLEN_1BYTE), &spireg->cfg);

	while (bitlen > 4) {
		debug("loopstart bitlen %d\n", bitlen);
		tmpdout = 0;

		/* Shift data so it's msb-justified */
		if (dout)
			tmpdout = *(u32 *) dout & 0x0ff;

		writel(~KWSPI_SMEMRDIRQ, &spireg->irq_cause);
		writel(tmpdout, &spireg->dout);	/* Write the data out */
		debug("*** spi_xfer: ... %08x written, bitlen %d\n",
		      tmpdout, bitlen);

		/*
		 * Wait for SPI transmit to get out
		 * or time out (1 second = 1000 ms)
		 * The NE event must be read and cleared first
		 */
		for (tm = 0, isread = 0; tm < KWSPI_TIMEOUT; ++tm) {
			if (readl(&spireg->irq_cause) & KWSPI_SMEMRDIRQ) {
				isread = 1;
				tmpdin = readl(&spireg->din);
				debug
					("spi_xfer: din %08x..%08x read\n",
					din, tmpdin);

				if (din) {
					*((u8 *) din) = (u8) tmpdin;
					din += 1;
				}
				if (dout)
					dout += 1;
				bitlen -= 8;
			}
			if (isread)
				break;
		}
		if (tm >= KWSPI_TIMEOUT)
			printf("*** spi_xfer: Time out during SPI transfer\n");

		debug("loopend bitlen %d\n", bitlen);
	}



#endif

	if (flags & SPI_XFER_END)
		spi_cs_deactivate(slave);

	return 0;
}

#if defined(MV88F6601)
void spi_init_done(unsigned int size)
{
	u32 config;

	/* Enable SPI direct read/write mode */
	if (size > 0x1000000) {
		config = MV_REG_READ(MV_SPI_IF_CONFIG_REG(0));
		config |= (0x3 << 8);
		MV_REG_WRITE(MV_SPI_IF_CONFIG_REG(0), config);
	}
}
#endif /* MV88F6601 */

#endif
