/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell 
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell Commercial License Option

If you received this File from Marvell and you have entered into a commercial
license agreement (a "Commercial License") with Marvell, the File is licensed
to you under the terms of the applicable Commercial License.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File in accordance with the terms and conditions of the General 
Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
available along with the File in the license.txt file or by writing to the Free 
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
on the worldwide web at http://www.gnu.org/licenses/gpl.txt. 

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
DISCLAIMED.  The GPL License provides additional details about this warranty 
disclaimer.
********************************************************************************
Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File under the following licensing terms. 
Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
	    this list of conditions and the following disclaimer. 

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution. 

    *   Neither the name of Marvell nor the names of its contributors may be 
        used to endorse or promote products derived from this software without 
        specific prior written permission. 
    
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "mvBoardEnvSpec.h"
#include "mvBoardEnvLib.h"
#include "mv_phy.h"
#include "mvSwitch.h"
#include "ctrlEnv/mvCtrlEnvRegs.h"
#include "mvSysEthPhyApi.h"

MV_VOID mvEthInternal3FEPhyBasicInit(MV_U32 port)
{
	int i;
	MV_U16 reg;

	//switchPhyRegWrite 0 1 0x1D 9
	mvEthSwitchPhyRegWrite(port, 1, 0x1D, 9);

	//switchPhyRegWrite 0 1 0x1F  bits[6:5] change to 0
	//switchPhyRegWrite 0 2 0x1F  bits[6:5] change to 0
	//switchPhyRegWrite 0 3 0x1F  bits[6:5] change to 0
	for (i=1; i<4; i++) {
		mvEthSwitchPhyRegRead(port, i, 0x1F, &reg);
		reg &= ~(BIT6 | BIT5);
		mvEthSwitchPhyRegWrite(port, i, 0x1F, reg);
	}

	//switchPhyRegWrite 0 1 0x1D 10
	mvEthSwitchPhyRegWrite(port, 1, 0x1D, 0x10);

	//switchPhyRegWrite 0 1 0x1E bit [13] change to 0
	//switchPhyRegWrite 0 2 0x1E bit [13] change to 0
	//switchPhyRegWrite 0 3 0x1E bit [13] change to 0
	for (i=1; i<4; i++) {
		mvEthSwitchPhyRegRead(port, i, 0x1E, &reg);
		reg &= ~(BIT13);
		mvEthSwitchPhyRegWrite(port, i, 0x1E, reg);
	}

	//switchPhyRegWrite 0 1 0x1D 0
	mvEthSwitchPhyRegWrite(port, 1, 0x1D, 0);

	/* Raise falltime configuration. */
	mvEthSwitchPhyRegWrite(port, 1, 0x1D, 0x8005);
	mvEthSwitchPhyRegWrite(port, 1, 0x1E, 0x2000);
	mvEthSwitchPhyRegWrite(port, 1, 0x1D, 0x9005);
	mvEthSwitchPhyRegWrite(port, 1, 0x1E, 0x2000);
	mvEthSwitchPhyRegWrite(port, 1, 0x1D, 0xA005);
	mvEthSwitchPhyRegWrite(port, 1, 0x1E, 0x2000);

	for (i=1; i<4; i++) {
		mvEthSwitchPhyRegWrite(port, i, 0x1C, 0xF03);

		/* soft reset the phy */
		mvEthSwitchPhyRegRead(port, i, 0, &reg);
		reg |= BIT15;
		mvEthSwitchPhyRegWrite(port, i, 0, reg);
	}
}

MV_VOID mvSWE1116PhyBasicInit(MV_U32 port, MV_U32 ethComplex)
{
	MV_U16 reg;
	MV_U16 swPort;

	if (ethComplex & ESC_OPT_RGMIIA_SW_P5)
		swPort = 5;
	else if (ethComplex & ESC_OPT_RGMIIA_SW_P6)
		swPort = 6;
	else
		return;

	/* Leds link and activity*/
	mvEthSwitchPhyRegWrite(port, swPort, 22, 0x3);
	mvEthSwitchPhyRegRead(port, swPort, 16, &reg);
	reg &= ~0xf;
	reg	|= 0x1;
	mvEthSwitchPhyRegWrite(port, swPort, 16, reg);
	mvEthSwitchPhyRegWrite(port, swPort, 22, 0x0);

	/* Set RGMII delay */
	mvEthSwitchPhyRegWrite(port, swPort, 22, 2);
	mvEthSwitchPhyRegRead(port, swPort, 21, &reg);
	reg	|= (BIT5 | BIT4);
	mvEthSwitchPhyRegWrite(port, swPort, 21, reg);
	mvEthSwitchPhyRegWrite(port, swPort, 22, 0);

	/* reset the phy */
	mvEthSwitchPhyRegRead(port, swPort, 0, &reg);
	reg |= BIT15;
	mvEthSwitchPhyRegWrite(port, swPort, 0, reg);
}

MV_VOID mvEthSWInternalGEPhyBasicInit(MV_U32 port, MV_U32 ethComplex)
{
	MV_U16 value;
	MV_U16 swPort;

	if (ethComplex & ESC_OPT_GEPHY_SW_P0)
		swPort = 0;
	else if (ethComplex & ESC_OPT_GEPHY_SW_P5)
		swPort = 5;
	else
		return;

	/* Force 10/100 mode */
	//mvEthSwitchPhyRegRead(port, 4, 9, &value);
	//value &= ~(BIT8 | BIT9);
	//mvEthSwitchPhyRegWrite(port, 4, 9, value);

	if (mvCtrlRevGet() < 2) {
		mvEthSwitchPhyRegWrite(port, swPort, 0x16, 0x00FF);
		mvEthSwitchPhyRegWrite(port, swPort, 0x11, 0x0FD0);
		mvEthSwitchPhyRegWrite(port, swPort, 0x10, 0x214C);
		mvEthSwitchPhyRegWrite(port, swPort, 0x11, 0x0000);
		mvEthSwitchPhyRegWrite(port, swPort, 0x10, 0x2000);
		mvEthSwitchPhyRegWrite(port, swPort, 0x11, 0x0F16);
		mvEthSwitchPhyRegWrite(port, swPort, 0x10, 0x2146);
		mvEthSwitchPhyRegWrite(port, swPort, 0x16, 0x0);
	}

	/* reset the phy */
	mvEthSwitchPhyRegRead(port, swPort, 0, &value);
	value |= BIT15;
	mvEthSwitchPhyRegWrite(port, swPort, 0, value);
	mvOsDelay(10);
}

/*********************************************************** 
* Init the PHY or Switch of the board 			   *
 ***********************************************************/
void mvBoardEgigaPhyInit(void) 
{
	MV_U32 ethComplex = mvBoardEthComplexConfigGet();
	MV_U32 portEnabled = 0;
	MV_U32 reg;
	MV_U32 i;
	char *env;
	MV_BOARD_INFO *pBoardInfo;

	mvSysEthPhyInit();

	pBoardInfo = mvBoardInfoGet();
	if (pBoardInfo->pBoardEgigaPhyInit)
		pBoardInfo->pBoardEgigaPhyInit(pBoardInfo);

	if (ethComplex & (ESC_OPT_RGMIIA_MAC0 | ESC_OPT_RGMIIB_MAC0))
		mvEthPhyInit(0, MV_FALSE);

	if (ethComplex & (ESC_OPT_RGMIIA_MAC1 | ESC_OPT_GEPHY_MAC1))
		mvEthPhyInit(1, MV_FALSE);

	if (mvBoardIsInternalSwitchConnected(0) || mvBoardIsInternalSwitchConnected(1)) {
		if (ethComplex & ESC_OPT_RGMIIA_SW_P5)
			portEnabled |= BIT5;
		if (ethComplex & ESC_OPT_RGMIIA_SW_P6)
			portEnabled |= BIT6;		
		if (ethComplex & ESC_OPT_MAC0_2_SW_P4)
			portEnabled |= BIT4;
		if (ethComplex & ESC_OPT_MAC1_2_SW_P5)
			portEnabled |= BIT5;
		if (ethComplex & ESC_OPT_GEPHY_SW_P0)
			portEnabled |= BIT0;
		if (ethComplex & ESC_OPT_GEPHY_SW_P5)
			portEnabled |= BIT5;
		if (ethComplex & ESC_OPT_FE3PHY)
			portEnabled |= BIT1 | BIT2 | BIT3;
		if (ethComplex & ESC_OPT_QSGMII)
			portEnabled |= BIT0 | BIT1 | BIT2 | BIT3;
		if (ethComplex & ESC_OPT_SGMII_2_SW_P1)
			portEnabled |= BIT1;
		mvEthKW2SwitchBasicInit(portEnabled);

		if (ethComplex & ESC_OPT_QSGMII) {
			env = getenv("eeeEnable");
			if((!env) || (strcmp(env,"yes") != 0))
				mvEthPhyInit((MV_U32) -1, MV_FALSE);
			else
				mvEthPhyInit((MV_U32) -1, MV_TRUE);
		}

		if (ethComplex & ESC_OPT_FE3PHY) {
			if (mvBoardIsInternalSwitchConnected(0))
				mvEthInternal3FEPhyBasicInit(0);
			else
				mvEthInternal3FEPhyBasicInit(1);
		}

		if (ethComplex & (ESC_OPT_GEPHY_SW_P0 | ESC_OPT_GEPHY_SW_P5)) {
			if (mvBoardIsInternalSwitchConnected(0))
				mvEthSWInternalGEPhyBasicInit(0, ethComplex);
			else
				mvEthSWInternalGEPhyBasicInit(1, ethComplex);
		}

		if (mvBoardIdGet() == RD_88F6560_GW_ID) {
			/* Config LED Matrix. */
			reg = MV_REG_READ(LED_MATRIX_CTRL_REG(0));
			reg |= 0x3;
			MV_REG_WRITE(LED_MATRIX_CTRL_REG(0), reg);

			/* Set PHY led mode */
			for(i = 0; i < 4; i++) {
				mvEthPhyRegWrite(i, 0x16, 3);
				mvEthPhyRegWrite(i, 0x10, 0x1771);
				mvEthPhyRegWrite(i, 0x16, 0);
			}
			mvEthPhyRegWrite(9, 0x16, 3);
			mvEthPhyRegWrite(9, 0x10, 0x1771);
			mvEthPhyRegWrite(9, 0x16, 0);
		}

		if (ethComplex & (ESC_OPT_RGMIIA_SW_P5 | ESC_OPT_RGMIIA_SW_P6)) {
			if (mvBoardIsInternalSwitchConnected(0))
				mvSWE1116PhyBasicInit(0, ethComplex);
			else
				mvSWE1116PhyBasicInit(1, ethComplex);
		}
	}
    if (MV_6601_DEV_ID == mvCtrlModelGet()) {
		MV_BOOL eeeEnable = MV_TRUE;
		env = getenv("eeeEnable");
		if((!env) || (strcmp(env,"yes") != 0))
			eeeEnable = MV_FALSE;

		if (ethComplex & ESC_OPT_GEPHY_MAC0) {
			mvEthPhyInit(0, eeeEnable);
			if (eeeEnable == MV_TRUE)
				mvNetaGmacLpiSet(0, 1);
		}

		if (ethComplex & ESC_OPT_LP_SERDES_FE_GE_PHY) {
			int port = (ethComplex & ESC_OPT_GEPHY_MAC0) ? 1 : 0;
			mvEthPhyInit(port, eeeEnable);
			if (eeeEnable == MV_TRUE)
				mvNetaGmacLpiSet(port, 1);
		}
                #define A_MC_DEBUG
#ifdef A_MC_DEBUG
    printf("MTL: Configuring GMAC Mode. ethComplex: 0x%x\n", ethComplex);
#endif
    /* Disable SMI polling on the port using SGMII if it is also using GEPHY on MAC0*/
		if (((ethComplex & ESC_OPT_SGMII) || (ethComplex & ESC_OPT_SGMII_2_5)) && (ethComplex & ESC_OPT_GEPHY_MAC0))
		{
		  MV_U32 port = (ethComplex & ESC_OPT_GEPHY_MAC0) ? 1 : 0;
		  MV_GMAC_MODE mode = (ethComplex & ESC_OPT_SGMII) ? PHY_SGMII_1G: PHY_SGMII_2_5G;

#ifdef A_MC_DEBUG
	    printf("MTL: Configuring GMAC Mode. mode:%d port:%d\n", mode, port);
#endif
      mvBoardGMACModeSet(mode, port, MV_FALSE);

                }

	}
}

/***********************************************************
 * GMAC mode configuration			   *
 ***********************************************************/
void mvBoardGMACModeSet(MV_GMAC_MODE mode, int port, MV_BOOL polarityInv)
{
  MV_U32 reg;
  MV_U32 i;

  if (mode == PHY_1000BASE_X_1G)
  {
    /* for 1000base-X link */
    printf("MTL: Init 1000base-X@1G on MAC %d..\n", port);

    // disable port
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg &= ~(1 << 0));

    // disable SMI polling
    reg = MV_REG_READ(ETH_UNIT_CTRL_REG(port));
    MV_REG_WRITE(ETH_UNIT_CTRL_REG(port), reg &= ~(1 << 1));

    // enable SGMII AutoNeg clock
    reg = MV_REG_READ(ONEMS_CLK_DIVIDER_CTRL_REG(port));
    MV_REG_WRITE(ONEMS_CLK_DIVIDER_CTRL_REG(port), reg |= (1 << 31));

    // PCS enable
    reg = MV_REG_READ(PORT_MAC_CTRL_REG2(port));
    MV_REG_WRITE(PORT_MAC_CTRL_REG2(port), reg |= (1 << 3));

    // AN reg: set InBandAnEn, clear InBandAnByPassEn
    MV_REG_WRITE(PORT_AUTO_NEG_CTRL_REG(port), 0x9044);

    if (polarityInv == MV_TRUE)
    {
      // Invert TXD_INV
      reg = MV_REG_READ(SYNC_PATTERN_REG);
      MV_REG_WRITE(SYNC_PATTERN_REG, reg |= (1 << 10));
    }

    // Enable 1000base-X AN
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg |= (1 << 1));

    // Recalibrate SERDES
    reg = MV_REG_READ(KVCO_CALIBRATION_CTRL_REG);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg |= (1 << 15));
    mvOsDelay(2);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg &= (1 << 15));

    // enable port
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg |= (1 << 0));

  }
  else if (mode == PHY_1000BASE_X_2_5G)
  {
    /* for 1000base-X link */
    printf("MTL: Init 1000base-X@2,5G on MAC %d..\n", port);

    // disable port
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg &= ~(1 << 0));

    // PCS enable
    reg = MV_REG_READ(PORT_MAC_CTRL_REG2(port));
    MV_REG_WRITE(PORT_MAC_CTRL_REG2(port), reg |= (1 << 3));

    // enable SGMII AutoNeg clock
    reg = MV_REG_READ(ONEMS_CLK_DIVIDER_CTRL_REG(port));
    MV_REG_WRITE(ONEMS_CLK_DIVIDER_CTRL_REG(port), reg |= (1 << 31));

    // Reset LP serdes
    reg = MV_REG_READ(SOFTWARE_RESET_CTRL_REG);
    MV_REG_WRITE(SOFTWARE_RESET_CTRL_REG, reg |= (1 << 24));
    MV_REG_WRITE(SOFTWARE_RESET_CTRL_REG, reg &= ~(1 << 24));

    // Clear SERDES config data
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0);

    // Reserved bits
    reg = MV_REG_READ(ETHERNET_COMPLEX_CTRL_REG_0);
    MV_REG_WRITE(ETHERNET_COMPLEX_CTRL_REG_0, reg &= ~(1 << 3));

    reg = MV_REG_READ(0x18804);
    MV_REG_WRITE(0x18804, reg |= (1 << 25));

    // Config Power and PLL
    MV_REG_WRITE(POWER_PLL_CTRL_REG, 0xF880);

    // SERDES config data
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0xCC0);

    // Generation 1 register 0
    MV_REG_WRITE(GENERATION_1_SETTING_0_REG, 0x8F9);
    // Generation 1 register 1
    MV_REG_WRITE(GENERATION_1_SETTING_1_REG, 0x9055);

    // digital loopback register
    MV_REG_WRITE(DIGITAL_LOOPBACK_ENABLE_REG, 0x430);

    // PHY isolation mode
    MV_REG_WRITE(PHY_ISOLATION_MODE_CTRL_REG, 0x0566);
    // PHY isolation mode
    MV_REG_WRITE(PHY_ISOLATION_MODE_CTRL_REG, 0x0166);

    // digital loopback register
    MV_REG_WRITE(DIGITAL_LOOPBACK_ENABLE_REG, 0x0072);

    //Polling on PLL ready - register 0xF10724A4 bit 2 should be 1
    MV_REG_READ(SERDES_STATUS_REG(port));
    // sleep 1
    mvOsDelay(1);
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0xCD0);

    //Polling on RX Done - register 0xF10724A4 bit 0 should be 1
    MV_REG_READ(SERDES_STATUS_REG(port));
    // sleep 1
    mvOsDelay(1);
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0xCC0);

    // Recalibrate SERDES
    reg = MV_REG_READ(KVCO_CALIBRATION_CTRL_REG);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg |= (1 << 15));
    mvOsDelay(2);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg &= (1 << 15));

    // Enable port, set port type, Set frame size limit to 767** Check this!
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), 0x8BFF);

    if (polarityInv == MV_TRUE)
    {
      // Invert tx polarity
      reg = MV_REG_READ(SYNC_PATTERN_REG);
      MV_REG_WRITE(SYNC_PATTERN_REG, reg |= (1 << 10));
    }

    // Set Impedance calibration values
    MV_REG_WRITE(IMPEDANCE_CALIBRATION_CTRL_REG, 0x9044);

    // Set Eth limit to 3G
    MV_REG_WRITE(PORT_BUCKET_REFILL_REG(port), 0x100B9B);
    for (i = 0; i < 8; i++)
    {
      MV_REG_WRITE(QUEUE_BUCKET_REFILL_REG(port, i), 0x100B9B);
    }
  }
  else if (mode == PHY_SGMII_1G)
  {
    printf("MTL: Init SGMII@1G on MAC %d..\n", port);

    //Set 1G and 100M modes adn full duplex
    MV_REG_WRITE(PORT_AUTO_NEG_CTRL_REG(port), 0x9062);

    //disable port, set packet size
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), 0x8B9C);

    // Recalibrate SERDES
    reg = MV_REG_READ(KVCO_CALIBRATION_CTRL_REG);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg |= (1 << 15));
    mvOsDelay(2);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg &= (1 << 15));

    //enable port
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg |= 1);

    if (polarityInv == MV_TRUE)
    {
      // Invert tx polarity
      reg = MV_REG_READ(SYNC_PATTERN_REG);
      MV_REG_WRITE(SYNC_PATTERN_REG, reg |= (1 << 10));
    }

  }
  else if (mode == PHY_SGMII_2_5G)
  {
    printf("MTL: Init SGMII@2,5G on MAC %d..\n", port);
    // disable port
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg &= ~(0x1));
    // working with PCS
    reg = MV_REG_READ(PORT_MAC_CTRL_REG2(port));
    MV_REG_WRITE(PORT_MAC_CTRL_REG2(port), reg |= (1 << 3));
    //Enable 1ms clock generation
    reg = MV_REG_READ(ONEMS_CLK_DIVIDER_CTRL_REG(port));
    MV_REG_WRITE(ONEMS_CLK_DIVIDER_CTRL_REG(port), reg |= (1 << 31));
    //LP Serdes reset
    reg = MV_REG_READ(SOFTWARE_RESET_CTRL_REG);
    MV_REG_WRITE(SOFTWARE_RESET_CTRL_REG, reg |= (1 << 24));
    MV_REG_WRITE(SOFTWARE_RESET_CTRL_REG, reg &= ~(1 << 24));
    //Serdes config data
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0);
    // Reserved bits
    reg = MV_REG_READ(ETHERNET_COMPLEX_CTRL_REG_0);
    MV_REG_WRITE(ETHERNET_COMPLEX_CTRL_REG_0, reg &= ~(1 << 3));
    // ?? Unknown register
    reg = MV_REG_READ(0x18804);
    MV_REG_WRITE(0x18804, reg |= (1 << 31));
    // PHY mode and power up
    MV_REG_WRITE(POWER_PLL_CTRL_REG, 0xF880);
    // SERDES configuration
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0xCC0);
    // SERDES configuration
    MV_REG_WRITE(GENERATION_1_SETTING_0_REG, 0x8F9);
    // SERDES configuration
    MV_REG_WRITE(GENERATION_1_SETTING_1_REG, 0x9055);
    // Digital loopback enable
    MV_REG_WRITE(DIGITAL_LOOPBACK_ENABLE_REG, 0x430);
    // PHY isolation mode
    MV_REG_WRITE(PHY_ISOLATION_MODE_CTRL_REG, 0x0566);
    // PHY RX initialization
    MV_REG_WRITE(PHY_ISOLATION_MODE_CTRL_REG, 0x0166);
    // Digital loopback enable
    MV_REG_WRITE(DIGITAL_LOOPBACK_ENABLE_REG, 0x0072);

    // Polling on PLL ready - register 0xF10724A4 bit 2 should be 1
    // SERDES status
    MV_REG_READ(SERDES_STATUS_REG(port));
    mvOsDelay(1);
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0xCD0);

    // Polling on RX Done - register 0xF10724A4 bit 0 should be 1
    MV_REG_READ(SERDES_STATUS_REG(port));
    mvOsDelay(1);
    MV_REG_WRITE(SERDES_CONFIG_REG(port), 0xCC0);

    // Disable autoneg. set speed and full duplex
    MV_REG_WRITE(PORT_AUTO_NEG_CTRL_REG(port), 0x9062);
    // Disable port, set packet size
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), 0x8B9C);

    // Recalibrate SERDES
    reg = MV_REG_READ(KVCO_CALIBRATION_CTRL_REG);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg |= (1 << 15));
    mvOsDelay(2);
    MV_REG_WRITE(KVCO_CALIBRATION_CTRL_REG, reg &= (1 << 15));

    // Enable port
    reg = MV_REG_READ(GE_MAC_CTRL_REG(port));
    MV_REG_WRITE(GE_MAC_CTRL_REG(port), reg |= (1));

    if (polarityInv == MV_TRUE)
    {
      // Invert Tx polarity
      reg = MV_REG_READ(SYNC_PATTERN_REG);
      MV_REG_WRITE(SYNC_PATTERN_REG, reg |= (1 << 10));
    }

    // Set Eth limit to 3G
    MV_REG_WRITE(PORT_BUCKET_REFILL_REG(port), 0x100B9B);
    for (i = 0; i < 8; i++)
    {
      MV_REG_WRITE(QUEUE_BUCKET_REFILL_REG(port, i), 0x100B9B);
    }
  }
}
