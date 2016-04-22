/*
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
/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

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

*******************************************************************************/

#include <common.h>
#include "mvTypes.h"
#include "mvBoardEnvLib.h"
#include "mvCpuIf.h"
#include "mvCtrlEnvLib.h"
#include "mv_mon_init.h"
#include "mvDebug.h"
#include "device/mvDevice.h"
#include "twsi/mvTwsi.h"
#if defined(CONFIG_MV_ETH_LEGACY)
#include "eth/mvEth.h"
#else
#include "neta/gbe/mvNeta.h"
#endif
#include "pex/mvPex.h"
#include "gpp/mvGpp.h"
#include "gpp/mvGppRegs.h"
#include "mvSysHwConfig.h"
#include "mv_phy.h"
#include "ctrlEnv/mvCtrlEthCompLib.h"

#ifdef MV_INCLUDE_RTC
#include "rtc/integ_rtc/mvRtc.h"
#elif CONFIG_RTC_DS1338_DS1339
#include "rtc/ext_rtc/mvDS133x.h"
#endif
#include "rtc.h"

#if defined(MV_INCLUDE_XOR)
#include "xor/mvXor.h"
#include "mvSysXorApi.h"
#endif
#if defined(MV_INCLUDE_IDMA)
#include "sys/mvSysIdma.h"
#include "idma/mvIdma.h"
#endif
#if defined(MV_INCLUDE_USB)
#include "usb/mvUsb.h"
#include "mv_hal_if/mvSysUsbApi.h"
#endif

#include "cpu/mvCpu.h"
#include "nand.h"
#include "spi_flash.h"
#ifdef CONFIG_PCI
	#include <pci.h>
#endif
//#include "pci/mvPciRegs.h"

#include <asm/arch-arm926ejs/vfpinstr.h>
#include <asm/arch-arm926ejs/vfp.h>
//#include <asm/arch/vfpinstr.h>
//#include <asm/arch/vfp.h>

#include <net.h>
#include <netdev.h>
#include <command.h>

#ifdef CONFIG_MTD_PARTITIONS
#include "sysvar.h"
#endif

/* #define MV_DEBUG */
#ifdef MV_DEBUG
#define DB(x) x
#else
#define DB(x)
#endif

/* CPU address decode table. */
MV_CPU_DEC_WIN mvCpuAddrWinMap[] = MV_CPU_IF_ADDR_WIN_MAP_TBL;
MV_CPU_DEC_WIN mvCpuAddrWinMap6601[] = MV_CPU_IF_ADDR_WIN_MAP_TBL_6601;
#if 0
static void mvHddPowerCtrl(void);
#endif
#if defined(CONFIG_CMD_RCVR)
extern void recoveryDetection(void);
#endif
void mv_cpu_init(void);
#if defined(MV_INCLUDE_CLK_PWR_CNTRL)
int mv_set_power_scheme(void);
#endif

#ifdef	CONFIG_FLASH_CFI_DRIVER
MV_VOID mvUpdateNorFlashBaseAddrBank(MV_VOID);
int mv_board_num_flash_banks;
extern flash_info_t	flash_info[]; /* info for FLASH chips */
extern unsigned long flash_add_base_addr (uint flash_index, ulong flash_base_addr);
#endif	/* CONFIG_FLASH_CFI_DRIVER */

#if (defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) && defined(CONFIG_MV_ETH_LEGACY))
#include "mv_egiga_legacy.h"
//extern MV_VOID mvBoardEgigaPhySwitchInit(void);
#endif 

#if (defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) && defined(CONFIG_MV_ETH_NETA))
#include "mv_egiga_neta.h"
//extern MV_VOID mvBoardEgigaPhySwitchInit(void);
#endif 

//#if defined(CONFIG_CMD_NAND)
/* Define for SDK 2.0 */
int __aeabi_unwind_cpp_pr0(int a,int b,int c) {return 0;}
int __aeabi_unwind_cpp_pr1(int a,int b,int c) {return 0;}
//#endif

extern nand_info_t nand_info[];       /* info for NAND chips */

extern struct spi_flash *flash;
//MV_VOID mvMppModuleTypePrint(MV_VOID);

#ifdef MV_NAND_BOOT
extern MV_U32 nandEnvBase;
#endif
void mvDramWinConfig(void);

#ifdef LARGEKERNEL
#define  LOAD_ADDR_STR "0x3000000"
#else
#define  LOAD_ADDR_STR "0x2000000"
#endif
/* Define for SDK 2.0 */
//int raise(void) {return 0;}

void print_mvBanner(void)
{
#ifdef CONFIG_SILENT_CONSOLE
	DECLARE_GLOBAL_DATA_PTR;
	gd->flags |= GD_FLG_SILENT;
#endif
	printf("\n");
	printf(" __   __                      _ _\n");
	printf("|  \\/  | __ _ _ ____   _____| | |\n");
	printf("| |\\/| |/ _` | '__\\ \\ / / _ \\ | |\n");
	printf("| |  | | (_| | |   \\ V /  __/ | |\n");
	printf("|_|  |_|\\__,_|_|    \\_/ \\___|_|_|\n");
	printf("         _   _     ____              _\n");
	printf("        | | | |   | __ )  ___   ___ | |_ \n");
	printf("        | | | |___|  _ \\ / _ \\ / _ \\| __| \n");
	printf("        | |_| |___| |_) | (_) | (_) | |_ \n");
	printf("         \\___/    |____/ \\___/ \\___/ \\__| \n");
//#if !defined(MV_NAND_BOOT)
#if defined(MV_INCLUDE_MONT_EXT)
    //mvMPPConfigToSPI();
	if(!enaMonExt())
		printf(" ** LOADER **\n"); 
	else
		printf(" ** MONITOR **\n");
    //mvMPPConfigToDefault();
#else

	printf(" ** LOADER **\n"); 
#endif /* MV_INCLUDE_MONT_EXT */
//#endif
	return;
}

void maskAllInt(void)
{
        /* mask all external interrupt sources */
        MV_REG_WRITE(CPU_MAIN_IRQ_MASK_REG, 0);
        MV_REG_WRITE(CPU_MAIN_FIQ_MASK_REG, 0);
        MV_REG_WRITE(CPU_ENPOINT_MASK_REG, 0);
        MV_REG_WRITE(CPU_MAIN_IRQ_MASK_HIGH_REG, 0);
        MV_REG_WRITE(CPU_MAIN_FIQ_MASK_HIGH_REG, 0);
        MV_REG_WRITE(CPU_ENPOINT_MASK_HIGH_REG, 0);
}

/* init for the Master*/
void misc_init_r_dec_win(void)
{
#if defined(MV_INCLUDE_USB)
	{
		char *env;

		env = getenv("usb0Mode");
		if((!env) || (strcmp(env,"device") == 0) || (strcmp(env,"Device") == 0) )
		{
			printf("USB 0: Device Mode\n");	
			mvSysUsbInit(0, MV_FALSE);
		}
		else
		{
			printf("USB 0: Host Mode\n");	
			mvSysUsbInit(0, MV_TRUE);
		}
	}
#endif/* #if defined(MV_INCLUDE_USB) */

#if defined(MV_INCLUDE_XOR)
	mvSysXorInit();
#endif

#if defined(MV_INCLUDE_CLK_PWR_CNTRL)
	mv_set_power_scheme();
#endif
    return;
}


/*
 * Miscellaneous platform dependent initialisations
 */

extern	MV_STATUS mvEthPhyRegRead(MV_U32 phyAddr, MV_U32 regOffs, MV_U16 *data);
extern	MV_STATUS mvEthPhyRegWrite(MV_U32 phyAddr, MV_U32 regOffs, MV_U16 data);

/* golabal mac address for yukon EC */
unsigned char yuk_enetaddr[6];
//alior extern int interrupt_init (void);
extern int timer_init(void );
extern void i2c_init(int speed, int slaveaddr);


int board_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;

#ifdef	CONFIG_FLASH_CFI_DRIVER
	int portwidth;
	MV_U32 devParam;
#endif
#if defined(MV_INCLUDE_TWSI)
	MV_TWSI_ADDR slave;
#endif
	MV_GPP_HAL_DATA gppHalData;

	unsigned int i;

	maskAllInt();

	/* must initialize the int in order for udelay to work */
	//alior interrupt_init();
	timer_init();

	/* Init the Board environment module (device bank params init) */
	mvBoardEnvInit();

#if defined(MV_INCLUDE_TWSI)
	slave.type = ADDR7_BIT;
	slave.address = 0;
	mvTwsiInit(0, CONFIG_SYS_I2C_SPEED, CONFIG_SYS_TCLK, &slave, 0);
#endif

	/* Init the GPIO sub-system */
	gppHalData.ctrlRev = mvCtrlRevGet();
	mvGppInit(&gppHalData);

	/* Init the Controlloer environment module (MPP init) */
	mvCtrlEnvInit();
	
	mvBoardDebugLed(3);

	/* Init the Controller CPU interface */
	if (MV_6601_DEV_ID == mvCtrlModelGet())
	{
#if 0
		mvDramWinConfig();
#endif
		mvCpuIfInit(mvCpuAddrWinMap6601);
		if (mvBoardIdGet() == DB_88F6601_BP_ID)
			MV_REG_BIT_SET(IO_CONFIG_0_REG, BIT14);
	}
	else
		mvCpuIfInit(mvCpuAddrWinMap);

	/* arch number of Integrator Board */
	gd->bd->bi_arch_number = 529; //KW2 arch number

	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x00000100;

	/* relocate the exception vectors */
	/* U-Boot is running from DRAM at this stage */
	for(i = 0; i < 0x100; i+=4)
	{
		*(unsigned int *)(0x0 + i) = *(unsigned int*)(TEXT_BASE + i);
	}

	mvBoardDebugLed(4);
	
	return 0;
}



void setenv_if_undefined(char *env_name, char *value)
{
	char *env;
	env = getenv(env_name);
	if (!env)
		setenv(env_name, value);
}

void dual_image_vars_set(void)
{
	char *env;

	env = getenv("dual_image");
	if ((!env) || (strcmp(env, "yes") != 0)) {
		setenv("dual_image", "no");
		return;
	}

	/* Set default values. */
	setenv_if_undefined("isValidA", "false");
	setenv_if_undefined("isValidB", "false");
	setenv_if_undefined("committedBank", "A");
	setenv_if_undefined("act_test", "0");
	setenv_if_undefined("act_boot_complete", "0");
	setenv_if_undefined("image_address", "0");
	setenv_if_undefined("imgA_ubi_mtd", "2");
	setenv_if_undefined("imgB_ubi_mtd", "5");
	setenv_if_undefined("imgA_ubi_name", "rootfsU");
	setenv_if_undefined("imgB_ubi_name", "rootfsB");
	setenv_if_undefined("imgA_addr", LOAD_ADDR_STR);
	setenv_if_undefined("imgB_addr", "0x6600000");

	setenv_if_undefined("get_mtd_list",
			"if test ${isValidA} = true -a ${isValidB} = true; then if test ${committedBank} = A; then setenv mtd_list A B; "
			"else setenv mtd_list B A; fi; else if test ${isValidA} = true;then setenv mtd_list A; "
			"else if test ${isValidB} = true; then setenv mtd_list B; else setenv mtd_list; fi; fi; fi;");

	setenv_if_undefined("valid_bootcmd", "run get_mtd_list; for i in ${mtd_list}; do "
			"if test ${i} = A; then setenv ubi_mtd ${imgA_ubi_mtd}; setenv img_ubi_name ${imgA_ubi_name}; setenv img_addr ${imgA_addr}; "
			"else setenv ubi_mtd ${imgB_ubi_mtd}; setenv img_ubi_name ${imgB_ubi_name}; setenv img_addr ${imgB_addr}; fi;"
			"echo Booting Image $i....; run bootcmd_img; done;");

	setenv_if_undefined("bootcmd_img" , "setenv bootargs ${console} ubi.mtd=${ubi_mtd} root=ubi0:${img_ubi_name} rootfstype=ubifs "
			"${mvNetConfig} ${mvPhoneConfig}; nand read.e ${loadaddr} ${img_addr} 0x400000; bootm ${loadaddr};");
	
	setenv_if_undefined("act_bootcmd", "if itest ${act_test} == 1; then if itest ${act_boot_complete} == 0; "
			"then setenv act_boot_complete 1; saveenv; echo \"Booting Active Image....\"; run bootcmd_active;"
			"else setenv act_boot_complete 0; setenv act_test 0; saveenv; fi; fi;");

	setenv_if_undefined("bootcmd_active", "echo bootcmd_active was not initialized....");

	env = getenv("bootcmd");
	setenv_if_undefined("default_bootcmd", env);
	setenv("bootcmd", "run act_bootcmd; run valid_bootcmd; echo \"Using default bootcmd....\"; run default_bootcmd");

	return;
}

/* This function is used to specify whether to read system variables from hnvram
 * or the legacy sysvar. For all future FiberJacks (GFLT300 and above) we should
 * be using hnvram. */
int use_hnvram(void)
{
	if (mvBoardIdGet() == GFLT110_ID)
		return 0;
	return 1;
}

static void set_boot_variables(void) {
	char value[SYSVAR_VALUE];
	char *env;

	/* If we are reading from hnvram, then get the activated kernel name
	 * from hnvram, otherwise use sysvar. */
	if (use_hnvram() == 1) {
		env = getenv("HNV_ACTIVATED_KERNEL_NAME");
		if (!env)
			setenv("HNV_ACTIVATED_KERNEL_NAME", "kernel0");
		setenv("gfparams",
			"if test $HNV_ACTIVATED_KERNEL_NAME = kernel1; "
			"then gfkernel=0x1100000; gfroot=rootfs1; "
			"else gfkernel=0x0300000; gfroot=rootfs0; fi");
	} else {
		if (sf_getvar("ACTIVATED_KERNEL_NAME",
				value, SYSVAR_VALUE) == 0)
			setenv("ACTIVATED_KERNEL_NAME", value);
		else
			setenv("ACTIVATED_KERNEL_NAME", "kernel0");
		setenv("gfparams",
			"if test $ACTIVATED_KERNEL_NAME = kernel1; "
			"then gfkernel=0xF80000; gfroot=rootfs1; "
			"else gfkernel=0x180000; gfroot=rootfs0; fi");
	}

	setenv("bootcmd",
		"run gfparams; "
		"sf read $loadaddr $gfkernel 0xe00000; "
		"setenv bootargs $console $mtdparts root=$gfroot $mvNetConfig "
		"$bootargs_extra; bootm $loadaddr;");
}

char* set_mtdparts(void)
{
	if (mvBoardIdGet() == GFLT110_ID) {
		return ("mtdparts=spi_flash:768k(uboot),256k(env),128k(var1),"
			"128k(var2),128k(sysvar1),128k(sysvar2),14m(kernel0),"
			"14m(kernel1),-(user_data)");
	}
	return ("mtdparts=spi_flash:768k(uboot),"
		"256k(env),2m(hnvram),14m(kernel0),14m(kernel1),"
		"-(data+jffs2)");
}


void misc_init_r_env(void){
	char *env;
	char tmp_buf[10];
	unsigned int malloc_len;
	DECLARE_GLOBAL_DATA_PTR;
//	char buff[256];
#if 0
	unsigned int flashSize =0 , secSize =0, ubootSize =0;


#if defined(MV_BOOTSIZE_4M)
	flashSize = _4M;
#elif defined(MV_BOOTSIZE_8M)
	flashSize = _8M;
#elif defined(MV_BOOTSIZE_16M)
	flashSize = _16M;
#elif defined(MV_BOOTSIZE_32M)
	flashSize = _32M;
#elif defined(MV_BOOTSIZE_64M)
	flashSize = _64M;
#endif

#if defined(MV_SEC_64K)
	secSize = _64K;
	ubootSize = _512K;
#elif defined(MV_SEC_128K)
	secSize = _128K;
	ubootSize = _128K * 5;
#elif defined(MV_SEC_256K)
	secSize = _256K;
	ubootSize = _256K * 3;
#endif

	if ((0 == flashSize) || (0 == secSize) || (0 == ubootSize))
	{
		env = getenv("console");
		if(!env)
			setenv("console","console=ttyS0,115200");
	}
	else
#endif
	env = getenv("console");
	if(!env) {
		if (mvBoardIdGet() == RD_88F6510_SFU_ID)
			setenv("console","console=ttyS0,115200 mv_port1_config=disconnected");
		else
			setenv("console","console=ttyS0,115200");
	}

	/* debug boot arguments */
	env = getenv("bootargs_debug");
	if (!env)
		setenv("bootargs_debug", "debug=1 login=1 earlyprintk");

	/* if we are not a GFLT110/120, then load hnvram into the u-boot
	 * environment. */
	if (mvBoardIdGet() != GFLT110_ID) {
		do_hnvram();
	}

#ifdef CONFIG_MTD_PARTITIONS
	env = getenv("mtdids");
	if(!env) {
		setenv("mtdids", MTDIDS_DEFAULT);
	}
	env = getenv("mtdparts");
	if(!env) {
		setenv("mtdparts", set_mtdparts());
	}
	setenv("partition", NULL);
#endif

/*#if defined(MV_SPI_BOOT)

	sprintf(buff,"console=ttyS0,115200 mtdparts=spi_flash:0x%x@0(uboot)ro,0x%x@0x%x(root)",
		 0x100000, flash->size - 0x100000, 0x100000);
	env = getenv("console");
	if(!env)
		setenv("console",buff);
#endif*/
// #if defined(MV_NAND_BOOT)
// 	sprintf(buff,"console=ttyS0,115200 mtdparts=nand_mtd:0x%x@0(uboot)ro,0x%x@0x%x(root)",
// 			0x100000, nand_info[0].size - 0x100000, 0x100000);
// 	env = getenv("console");
// 	if(!env)
// 		setenv("console",buff);

	//env = getenv("nandEnvBase");
	//strcpy(env, "");
// 	sprintf(buff, "0x%x", nandEnvBase);
// 	setenv("nandEnvBase", buff);
// #endif
#if 0
	/* Linux open port support */
	env = getenv("mainlineLinux");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
		setenv("mainlineLinux","yes");
	else
		setenv("mainlineLinux","no");

	env = getenv("mainlineLinux");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
		/* arch number for open port Linux */
		env = getenv("arcNumber");
		if(!env )
		{
			/* arch number according to board ID */
			int board_id = mvBoardIdGet();	
			switch(board_id){
				case(DB_88F6281A_BP_ID):
				sprintf(tmp_buf,"%d", DB_88F6281_BP_MLL_ID);
				board_id = DB_88F6281_BP_MLL_ID; 
				break;
				case(RD_88F6192A_ID):
				sprintf(tmp_buf,"%d", RD_88F6192_MLL_ID);
				board_id = RD_88F6192_MLL_ID; 
				break;
				case(RD_88F6281A_ID):
				sprintf(tmp_buf,"%d", RD_88F6281_MLL_ID);
				board_id = RD_88F6281_MLL_ID; 
				break;
				case(DB_CUSTOMER_ID):
				break;
				default:
				sprintf(tmp_buf,"%d", board_id);
				board_id = board_id; 
				break;
			}
			gd->bd->bi_arch_number = board_id;
			setenv("arcNumber", tmp_buf);
		}
		else
		{
			gd->bd->bi_arch_number = simple_strtoul(env, NULL, 10);
		}
	}
#endif
	/* update the CASset env parameter */
	env = getenv("CASset");
	if(!env )
	{
#ifdef MV_MIN_CAL
		setenv("CASset","min");
#else
		setenv("CASset","max");
#endif
	}
        /* Monitor extension */
#ifdef MV_INCLUDE_MONT_EXT
	env = getenv("enaMonExt");
	if(/* !env || */ ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
		setenv("enaMonExt","yes");
	else
#endif
	setenv("enaMonExt","no");

#if defined (MV_INC_BOARD_NOR_FLASH)
	env = getenv("enaFlashBuf");
	if (((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0)))
		setenv("enaFlashBuf","no");
	else
		setenv("enaFlashBuf","yes");
#endif

	/* CPU streaming */
	env = getenv("enaCpuStream");
	if(!env || ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
		setenv("enaCpuStream","no");
	else
		setenv("enaCpuStream","yes");

	/* Write allocation */
	env = getenv("enaWrAllo");
	if( !env || ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
		setenv("enaWrAllo","no");
	else
		setenv("enaWrAllo","yes");

	/* Pex mode */
#ifdef MV_PEX_END_POINT_MODE
	setenv("pexMode","EP");
#else
	setenv("pexMode","RC");
#endif

	env = getenv("disL2Cache");
	if((mvCpuL2Exists() == MV_TRUE) && (!env || (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0)))
		setenv("disL2Cache","no"); 
	else
		setenv("disL2Cache","yes");

	env = getenv("setL2CacheWT");
	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
		setenv("setL2CacheWT","yes"); 
	else
		setenv("setL2CacheWT","no");

	env = getenv("disL2Prefetch");
	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) ) {
		setenv("disL2Prefetch","yes"); 

		/* ICache Prefetch */
		env = getenv("enaICPref");
#ifdef MV88F6601
	if (!env) {
		/* Disable instruction cache preftech by default for Avanta-MC */
		setenv("enaICPref","no");
		env = getenv("enaICPref");
	}
#endif

		if( env && ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
			setenv("enaICPref","no");
		else
			setenv("enaICPref","yes");
		
		/* DCache Prefetch */
		env = getenv("enaDCPref");
#ifdef MV88F6601
	if (!env) {
		/* Disable data cache preftech by default for Avanta-MC */
		setenv("enaDCPref","no");
		env = getenv("enaDCPref");
	}
#endif
		if( env && ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
			setenv("enaDCPref","no");
		else
			setenv("enaDCPref","yes");
	} else {
		setenv("disL2Prefetch","no");
		setenv("enaICPref","no");
		setenv("enaDCPref","no");
	}

	env = getenv("sata_dma_mode");
	if( env && ((strcmp(env,"No") == 0) || (strcmp(env,"no") == 0) ) )
		setenv("sata_dma_mode","no");
	else
		setenv("sata_dma_mode","yes");

	/* Malloc length */
	env = getenv("MALLOC_len");
	malloc_len =  simple_strtoul(env, NULL, 10) << 20;
	if(malloc_len == 0) {
		sprintf(tmp_buf,"%d",CONFIG_SYS_MALLOC_LEN>>20);
		setenv("MALLOC_len",tmp_buf);
	}

	/* primary network interface */
	env = getenv("ethprime");
	if(!env)
		setenv("ethprime",ENV_ETH_PRIME);
 
	/* netbsd boot arguments */
	env = getenv("netbsd_en");
	if( !env || ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
		setenv("netbsd_en","no");
	else {
		setenv("netbsd_en","yes");
		env = getenv("netbsd_gw");
		if(!env)
			setenv("netbsd_gw","192.168.0.254");
		env = getenv("netbsd_mask");
		if(!env)
			setenv("netbsd_mask","255.255.255.0");

		env = getenv("netbsd_fs");
		if(!env)
			setenv("netbsd_fs","nfs");

		env = getenv("netbsd_server");
		if(!env)
			setenv("netbsd_server","192.168.0.1");

		env = getenv("netbsd_ip");
		if(!env)
		{
			env = getenv("ipaddr");
			setenv("netbsd_ip",env);
		}

		env = getenv("netbsd_rootdev");
		if(!env)
			setenv("netbsd_rootdev","mgi0");

		env = getenv("netbsd_add");
		if(!env)
			setenv("netbsd_add","0x800000");

		env = getenv("netbsd_get");
		if(!env)
			setenv("netbsd_get","tftpboot ${netbsd_add} ${image_name}");

		env = getenv("netbsd_set_args");
		if(!env)
			setenv("netbsd_set_args","setenv bootargs nfsroot=${netbsd_server}:${rootpath} fs=${netbsd_fs} \
ip=${netbsd_ip} serverip=${netbsd_server} mask=${netbsd_mask} gw=${netbsd_gw} rootdev=${netbsd_rootdev} \
ethaddr=${ethaddr} eth1addr=${eth1addr} ethmtu=${ethmtu} eth1mtu=${eth1mtu} ${netbsd_netconfig}");

		env = getenv("netbsd_boot");
		if(!env)
			setenv("netbsd_boot","bootm ${netbsd_add} ${bootargs}");

		env = getenv("netbsd_bootcmd");
		if(!env)
			setenv("netbsd_bootcmd","run netbsd_get ; run netbsd_set_args ; run netbsd_boot");
	}

	/* vxWorks boot arguments */
	env = getenv("vxworks_en");
	if( !env || ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
		setenv("vxworks_en","no");
	else {
		char* buff = (char *)0x1100;
		setenv("vxworks_en","yes");
		
		sprintf(buff,"mgi(0,0) host:vxWorks.st");
		env = getenv("serverip");
		strcat(buff, " h=");
		strcat(buff,env);
		env = getenv("ipaddr");
		strcat(buff, " e=");
		strcat(buff,env);
		strcat(buff, ":ffff0000 u=anonymous pw=target ");

		setenv("vxWorks_bootargs",buff);
	}

	/* linux boot arguments */
	env = getenv("bootargs_root");
	if(!env)
		setenv("bootargs_root","root=/dev/nfs rw");

	/* For open Linux we set boot args differently */
	env = getenv("mainlineLinux");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
		env = getenv("bootargs_end");
		if(!env)
			setenv("bootargs_end",":::orion:eth0:none");
	}
	else
	{
		env = getenv("bootargs_end");
		if(!env)
			setenv("bootargs_end",MV_BOOTARGS_END);
	}
	
	env = getenv("image_name");
	if(!env)
		setenv("image_name","uImage");

#if (CONFIG_BOOTDELAY >= 0)
	env = getenv("bootcmd");
	if(!env)
#if defined(MV_INCLUDE_TDM) && defined(MV_INC_BOARD_QD_SWITCH)
		setenv("bootcmd","tftpboot "LOAD_ADDR_STR" ${image_name};\
setenv bootargs ${console} ${bootargs_root} nfsroot=${serverip}:${rootpath} \
ip=${ipaddr}:${serverip}${bootargs_end} ${mvNetConfig} ${mvPhoneConfig};bootm "LOAD_ADDR_STR";");
#elif defined(MV_INC_BOARD_QD_SWITCH)
		setenv("bootcmd","tftpboot "LOAD_ADDR_STR" ${image_name};\
setenv bootargs ${console} ${bootargs_root} nfsroot=${serverip}:${rootpath} \
ip=${ipaddr}:${serverip}${bootargs_end} ${mvNetConfig};bootm "LOAD_ADDR_STR"; ");
#elif defined(MV_INCLUDE_TDM)
		setenv("bootcmd","tftpboot "LOAD_ADDR_STR" ${image_name};\
setenv bootargs ${console} ${mtdparts} ${bootargs_root} nfsroot=${serverip}:${rootpath} \
ip=${ipaddr}:${serverip}${bootargs_end} ${mvNetConfig} ${mvPhoneConfig};bootm "LOAD_ADDR_STR";");
#else
		setenv("bootcmd","tftpboot "LOAD_ADDR_STR" ${image_name};\
setenv bootargs ${console} ${bootargs_root} nfsroot=${serverip}:${rootpath} \
ip=${ipaddr}:${serverip}${bootargs_end};bootm "LOAD_ADDR_STR";");
#endif

	set_boot_variables();
#endif /* (CONFIG_BOOTDELAY >= 0) */

	env = getenv("standalone");
	if(!env)
#if defined(MV_INCLUDE_TDM) && defined(MV_INC_BOARD_QD_SWITCH)
		setenv("standalone","fsload "LOAD_ADDR_STR" ${image_name};setenv bootargs ${console} root=/dev/mtdblock0 rw \
ip=${ipaddr}:${serverip}${bootargs_end} ${mvNetConfig} ${mvPhoneConfig}; bootm "LOAD_ADDR_STR";");
#elif defined(MV_INC_BOARD_QD_SWITCH)
		setenv("standalone","fsload "LOAD_ADDR_STR" ${image_name};setenv bootargs ${console} root=/dev/mtdblock0 rw \
ip=${ipaddr}:${serverip}${bootargs_end} ${mvNetConfig}; bootm "LOAD_ADDR_STR";");
#elif defined(MV_INCLUDE_TDM)
		setenv("standalone","fsload "LOAD_ADDR_STR" ${image_name};setenv bootargs ${console} root=/dev/mtdblock0 rw \
ip=${ipaddr}:${serverip}${bootargs_end} ${mvPhoneConfig}; bootm "LOAD_ADDR_STR";");
#else
		setenv("standalone","fsload "LOAD_ADDR_STR" ${image_name};setenv bootargs ${console} root=/dev/mtdblock0 rw \
ip=${ipaddr}:${serverip}${bootargs_end}; bootm "LOAD_ADDR_STR";");
#endif

	/* Set boodelay to 3 sec, if Monitor extension are disabled */
	if(!enaMonExt()){
		setenv("bootdelay","3");
		setenv("disaMvPnp","no");
	}

	/* Disable PNP config of Marvel memory controller devices. */
	env = getenv("disaMvPnp");
	if(!env)
		setenv("disaMvPnp","no");

#if (defined(MV_INCLUDE_GIG_ETH) || defined(MV_INCLUDE_UNM_ETH))
	/* Generate random ip and mac address */
	/* Read RTC to create pseudo-random data for enc */
	struct rtc_time tm;
	unsigned int xi, xj, xk, xl;
	char ethaddr_0[30];
	char ethaddr_1[30];
	char pon_addr[30];

	rtc_get(&tm);
	xi = ((tm.tm_yday + tm.tm_sec)% 254);
	/* No valid ip with one of the fileds has the value 0 */
	if (xi == 0)
		xi+=2;

	xj = ((tm.tm_yday + tm.tm_min)%254);
	/* No valid ip with one of the fileds has the value 0 */
	if (xj == 0)
		xj+=2;

	/* Check if the ip address is the same as the server ip */
	if ((xj == 1) && (xi == 11))
		xi+=2;

	xk = (tm.tm_min * tm.tm_sec)%254;
	xl = (tm.tm_hour * tm.tm_sec)%254;

	sprintf(ethaddr_0,"00:50:43:%02x:%02x:%02x",xk,xi,xj);
	sprintf(ethaddr_1,"00:50:43:%02x:%02x:%02x",xl,xi,xj);
	sprintf(pon_addr,"00:50:43:%02x:%02x:%02x",xj,xk,xl);

	if (use_hnvram() == 1) {
		/* Check imported hnvram variables for mac addresses. If they're
		 * not available in the u-boot environment then use the randomly
		 * generated ones from above. */
		env = getenv("HNV_ETH_MAC_ADDR");
		if (!env)
			setenv("ethaddr", ethaddr_0);
		else
			setenv("ethaddr", env);

		env = getenv("HNV_PON_MAC_ADDR");
		if (!env)
			setenv("mv_pon_addr", pon_addr);
		else
			setenv("mv_pon_addr", env);
	} else {
		/* Check sysvar for mac addresses. If they're not available,
		 * then use the ones we randomly generated above. */
		env = getenv("ethaddr");
		if (!env) {
			/* Override ethaddr from sysvar only if there is a
			 * sysvar variable available to replace it with. */
			if (sf_getvar("ETH_MAC_ADDR", ethaddr_0,
					sizeof(ethaddr_0)) != 0)
				setenv("ethaddr", ethaddr_0);
			else
				setenv("ethaddr", ethaddr_0);
		}

		env = getenv("mv_pon_addr");
		if (!env) {
			/* Override mv_pon_addr from sysvar only if there is a
			 * sysvar variable available to replace it with. */
			if (sf_getvar("PON_MAC_ADDR", pon_addr,
					sizeof(pon_addr)) != 0)
				setenv("mv_pon_addr", pon_addr);
			else
				setenv("mv_pon_addr", pon_addr);
		}
	}

	env = getenv("eth1addr");
	if(!env)
		setenv("eth1addr",ethaddr_1);

	env = getenv("ethmtu");
	if(!env)
		setenv("ethmtu","1500");

	env = getenv("eth1mtu");
	if(!env)
		setenv("eth1mtu","1500");

	/* Set mvNetConfig env parameter */
	env = getenv("mvNetConfig");
	if(!env ) {
		switch (mvBoardIdGet()) {
			case RD_88F6510_SFU_ID:
			case RD_88F6530_MDU_ID:
				setenv("mvNetConfig","mv_net_config=0");
			break;
			case RD_88F6560_GW_ID:
			case DB_88F6535_BP_ID:
			default:
				setenv("mvNetConfig","mv_net_config=4,(00:50:43:11:11:11,0:1:2:3),mtu=1500");
			break;
		}
	}
#endif /*  (MV_INCLUDE_GIG_ETH) || defined(MV_INCLUDE_UNM_ETH) */

#if defined(MV_INCLUDE_USB)
	/* USB Host */
	env = getenv("usb0Mode");
	if(!env)
		setenv("usb0Mode",ENV_USB0_MODE);
#endif  /* (MV_INCLUDE_USB) */

#if defined(YUK_ETHADDR)
	env = getenv("yuk_ethaddr");
	if(!env)
		setenv("yuk_ethaddr",YUK_ETHADDR);

	{
		int i;
		char *tmp = getenv ("yuk_ethaddr");
		char *end;

		for (i=0; i<6; i++) {
			yuk_enetaddr[i] = tmp ? simple_strtoul(tmp, &end, 16) : 0;
			if (tmp)
				tmp = (*end) ? end+1 : end;
		}
	}
#endif /* defined(YUK_ETHADDR) */

#if defined(MTD_NAND_LNC)
	env = getenv("nandEcc");
	if(!env)
		setenv("nandEcc", "1bit");
#endif

#if defined(CONFIG_CMD_RCVR)
	env = getenv("netretry");
	if (!env)
		setenv("netretry","no");

	env = getenv("rcvrip");
	if (!env)
		setenv("rcvrip",RCVR_IP_ADDR);

	env = getenv("loadaddr");
	if (!env)
		setenv("loadaddr",RCVR_LOAD_ADDR);

	env = getenv("autoload");
	if (!env)
		setenv("autoload","no");

	/* Check the recovery trigger */
	recoveryDetection();
#endif
	env = getenv("eeeEnable");
	if (!env)
		setenv("eeeEnable","no");

	dual_image_vars_set();
	return;
}

#ifdef BOARD_LATE_INIT
int board_late_init (void)
{
	/* Check if to use the LED's for debug or to use single led for init and Linux heartbeat */
	mvBoardDebugLed(0);
	return 0;
}
#endif

void pcie_tune(void)
{
	MV_REG_WRITE(0xF1041AB0, 0x100);
	MV_REG_WRITE(0xF1041A20, 0x78000801);
	MV_REG_WRITE(0xF1041A00, 0x4014022F);
	MV_REG_WRITE(0xF1040070, 0x18110008);

	return;
}

int board_eth_init(bd_t *bis)
{
#ifdef  CONFIG_MARVELL
#if defined(MV_INCLUDE_GIG_ETH) || defined(MV_INCLUDE_UNM_ETH)
	/* move to the begining so in case we have a PCI NIC it will
	read the env mac addresses correctlly. */
	mv_eth_initialize(bis);
#endif
#endif
#if defined(CONFIG_SK98)
	skge_initialize(bis);
#endif
#if defined(CONFIG_E1000)
	e1000_initialize(bis);
#endif
	return 0;
}

int print_cpuinfo (void)
{
	char name[50];

	mvBoardNameGet(name);
	printf("Board: %s\n",  name);
	mvCtrlModelRevNameGet(name);
	printf("SoC:   %s\n",  name);
	mvCpuNameGet(name);
	printf("CPU:   %s - ",  name);
#ifdef MV_CPU_BE
	printf("BE\n");
#else
	printf("LE\n");
#endif
	printf("       CPU @ %dMhz, ",  mvCpuPclkGet()/1000000);
	if (mvCpuL2Exists() == MV_TRUE)
		printf("L2 @ %dMhz\n", mvCpuL2ClkGet()/1000000);
#ifndef MV88F6601
	else
		printf("No L2\n");
#else
	printf("\n");
#endif
	printf("       DDR%s @ %dMhz, TClock @ %dMhz\n", ((mvDramIfIsTypeDdr3() == MV_TRUE) ? "3" : "2"),
			CONFIG_SYS_BUS_CLK/1000000, mvTclkGet()/1000000);
	return 0;
}

int misc_init_r (void)
{
	char *env;
	
	mvBoardDebugLed(5);

	/* init special env variables */
	misc_init_r_env();

	mv_cpu_init();

#if defined(MV_INCLUDE_MONT_EXT)
	if(enaMonExt()){
		printf("Marvell monitor extension:\n");
		mon_extension_after_relloc();
	}
#endif /* MV_INCLUDE_MONT_EXT */

	/* init the units decode windows */
	misc_init_r_dec_win();

#if 0
#ifdef CONFIG_PCI
#if !defined(MV_MEM_OVER_PCI_WA) && !defined(MV_MEM_OVER_PEX_WA)
	pci_init();
#endif
#endif
#endif

	mvBoardDebugLed(6);
	/* Prints the modules detected */
	mvBoardMppModuleTypePrint();
#undef A_MC_DEBUG
#ifdef A_MC_DEBUG
	{
		MV_U32 ethComplex = mvBoardEthComplexConfigGet();

		printf(" Avanta-MC- Jumper Scan: \n");
		if (ethComplex & ESC_OPT_GEPHY_MAC0)
			printf("         JP0=0 -> MAC0-->GbE PHY, MAC1 --> LP SERDES\n");
		else
			printf("         JP0=1 -> MAC0-->LP SERDES, MAC1-->NONE\n"); 

		if (ethComplex & ESC_OPT_LP_SERDES_FE_GE_PHY)
			printf("         JP2=0 -> LP SerDes connected to GBE PHY\n");
		else
			printf("         JP2=1 -> LP SerDes connected to SFP\n");

		if (ethComplex &  ESC_OPT_SGMII_2_5)
			printf("         JP3=1 -> LP SerDes at 2.5G mode\n");
		else
			printf("         JP3=0 -> LP SerDes at 1.25G mode\n");
	}

#endif

	mvBoardDebugLed(7);

	/* pcie fine tunning */
	env = getenv("pcieTune");
	if(env && ((strcmp(env,"yes") == 0) || (strcmp(env,"yes") == 0)))
		pcie_tune();
	else
		setenv("pcieTune","no");

#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)
	/* Init the PHY or Switch of the board */
	mvBoardEgigaPhyInit();
#endif /* #if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) */

#ifdef CONFIG_MTD_DEVICE
	mtdparts_init();
#endif

	return 0;
}

MV_U32 mvTclkGet(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	/* get it only on first time */
	if(gd->tclk == 0)
		gd->tclk = mvBoardTclkGet();

	return gd->tclk;
}

MV_U32 mvSysClkGet(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	/* get it only on first time */
	if(gd->bus_clk == 0)
		gd->bus_clk = mvBoardSysClkGet();

	return gd->bus_clk;
}

#if defined (MV_INCLUDE_RTC) || defined(CONFIG_RTC_DS1338_DS1339)
/* exported for EEMBC */
MV_U32 mvGetRtcSec(void)
{
	MV_RTC_TIME time;
#ifdef MV_INCLUDE_RTC
	mvRtcTimeGet(&time);
#elif CONFIG_RTC_DS1338_DS1339
	mvRtcDS133xTimeGet(&time);
#endif
	return (time.minutes * 60) + time.seconds;
}
#endif /* MV_INCLUDE_RTC || CONFIG_RTC_DS1338_DS1339 */

void reset_cpu(void)
{
	mvBoardReset();
}

void mv_cpu_init(void)
{
	char *env;
	volatile unsigned int temp;

	/*CPU streaming & write allocate */
	env = getenv("enaWrAllo");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))  
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp |= BIT28;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
		
	}
	else
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp &= ~BIT28;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
	}

	env = getenv("enaCpuStream");
	if(!env || (strcmp(env,"no") == 0) ||  (strcmp(env,"No") == 0) )
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp &= ~BIT29;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
	}
	else
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp |= BIT29;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
	}
	
	#if 0
	/* Verifay write allocate and streaming */
	printf("\n");
	__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
	if (temp & BIT29)
		printf("Streaming enabled \n");
	else
		printf("Streaming disabled \n");
	if (temp & BIT28)
		printf("Write allocate enabled\n");
	else
		printf("Write allocate disabled\n");
	#endif

	/* DCache Pref  */
	env = getenv("enaDCPref");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
		MV_REG_BIT_SET( CPU_CONFIG_REG , CCR_DCACH_PREF_BUF_ENABLE);
	}

	if(env && ((strcmp(env,"no") == 0) ||  (strcmp(env,"No") == 0)))
	{
		MV_REG_BIT_RESET( CPU_CONFIG_REG , CCR_DCACH_PREF_BUF_ENABLE);
	}

	/* ICache Pref  */
	env = getenv("enaICPref");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
		MV_REG_BIT_SET( CPU_CONFIG_REG , CCR_ICACH_PREF_BUF_ENABLE);
	}
	
	if(env && ((strcmp(env,"no") == 0) ||  (strcmp(env,"No") == 0)))
	{
		MV_REG_BIT_RESET( CPU_CONFIG_REG , CCR_ICACH_PREF_BUF_ENABLE);
	}

	/* Set L2C WT mode - Set bit 4 */
	temp = MV_REG_READ(CPU_L2_CONFIG_REG);
	env = getenv("setL2CacheWT");
	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
	{
		temp |= BIT4;
	}
	else
		temp &= ~BIT4;
	MV_REG_WRITE(CPU_L2_CONFIG_REG, temp);


	/* L2Cache settings */
	asm ("mrc p15, 1, %0, c15, c1, 0":"=r" (temp));

	/* Disable L2C pre fetch - Set bit 24 */
	env = getenv("disL2Prefetch");
	if(env && ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
		temp &= ~BIT24;
	else
		temp |= BIT24;

	/* enable L2C - Set bit 22 */
	env = getenv("disL2Cache");
	if(!env || ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
		temp |= BIT22;
	else
		temp &= ~BIT22;
	
	asm ("mcr p15, 1, %0, c15, c1, 0": :"r" (temp));

	/* Enable i cache */
	asm ("mrc p15, 0, %0, c1, c0, 0":"=r" (temp));
	temp |= BIT12;
	asm ("mcr p15, 0, %0, c1, c0, 0": :"r" (temp));
	
	/* Change reset vector to address 0x0 */
	asm ("mrc p15, 0, %0, c1, c0, 0":"=r" (temp));
	temp &= ~BIT13;
	asm ("mcr p15, 0, %0, c1, c0, 0": :"r" (temp));
}

/* Set unit in power off mode acording to the detection of MPP */
#if defined(MV_INCLUDE_CLK_PWR_CNTRL)
int mv_set_power_scheme(void)
{
	MV_U32 ethCompOpt;
	MV_U32 ponOpt;
	MV_U32 boardId = mvBoardIdGet();
#ifndef MV88F6601
	MV_U16 devId = mvCtrlModelGet();
	MV_BOOL	integSwitch, gePhy, fePhy;
#endif
	mvOsOutput("Shutting down unused interfaces:\n");

	/* GPON */
	ponOpt = mvBoardPonConfigGet();
	if(ponOpt == BOARD_PON_NONE) {
		mvOsOutput("       PON\n");
		mvCtrlPwrClckSet(XPON_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrMemSet(XPON_UNIT_ID, 0, MV_FALSE);
	}

	/* Sata & Ethernet Complex */
	ethCompOpt = mvBoardEthComplexConfigGet();
#ifndef MV88F6601
	/* Sata */
	if (!(ethCompOpt & ESC_OPT_SATA)) {
		mvOsOutput("       SATA\n");
		mvCtrlPwrClckSet(SATA_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrMemSet(SATA_UNIT_ID, 0, MV_FALSE);
	}
#endif
	/* Ethernet complex */
#if 0
	/* MAC0*/
	if (!(ethCompOpt & (ESC_OPT_RGMIIA_MAC0 | ESC_OPT_RGMIIB_MAC0 |
					ESC_OPT_MAC0_2_SW_P4 | ESC_OPT_SGMII))) {
		mvOsOutput("       MAC0\n");
		mvCtrlPwrClckSet(ETH_GIG_UNIT_ID, 0, MV_FALSE);
	}

	/* MAC1 */
	if(!(ethCompOpt & (ESC_OPT_RGMIIA_MAC1 | ESC_OPT_MAC1_2_SW_P5 | 
			ESC_OPT_GEPHY_MAC1))) {
		mvOsOutput("       MAC1\n");
		mvCtrlPwrClckSet(ETH_GIG_UNIT_ID, 1, MV_FALSE);
	}
#endif
#ifndef MV88F6601
	/* Switch */
	integSwitch = MV_FALSE;
	if (!(ethCompOpt & (ESC_OPT_MAC0_2_SW_P4 | ESC_OPT_MAC1_2_SW_P5))) {
		integSwitch = MV_TRUE;
		mvOsOutput("       Switch\n");
	}

	/* 3xFE */
	fePhy = MV_FALSE;
	if (!(ethCompOpt & ESC_OPT_FE3PHY)) {
		mvOsOutput("       3xFE-PHY\n");
		fePhy = MV_TRUE;
	}

	/* GE Phy */
	gePhy = MV_FALSE;
	if (!(ethCompOpt & (ESC_OPT_GEPHY_MAC1 | ESC_OPT_GEPHY_SW_P0 |
				ESC_OPT_GEPHY_SW_P5))) {
		mvOsOutput("       GE-PHY\n");
		gePhy = MV_TRUE;
	}

	mvEthernetComplexShutdownIf(integSwitch, gePhy, fePhy);

	/* SDIO */
	if ((boardId == RD_88F6510_SFU_ID) || (boardId == RD_88F6530_MDU_ID) ||
	    (mvBoardIsSdioEnabled() == MV_FALSE)) {
		mvOsOutput("       SDIO\n");
		mvCtrlPwrClckSet(SDIO_UNIT_ID, 0, MV_FALSE);
	}

	/* Shutdown other interfaces depending on device type. */
	if (devId == MV_6510_DEV_ID) {
		/* PCI-E */
		mvOsOutput("       PCI-E 0/1\n");
		mvCtrlPwrClckSet(PEX_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrClckSet(PEX_UNIT_ID, 1, MV_FALSE);
		mvCtrlPwrMemSet(PEX_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrMemSet(PEX_UNIT_ID, 1, MV_FALSE);

		/* Crypto engine */
		mvOsOutput("       Crypto\n");
		mvCtrlPwrClckSet(CESA_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrMemSet(CESA_UNIT_ID, 0, MV_FALSE);

		/* Xor engine */
		mvOsOutput("       XOR engine 0/1\n");
		mvCtrlPwrClckSet(XOR_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrClckSet(XOR_UNIT_ID, 1, MV_FALSE);
		mvCtrlPwrMemSet(XOR_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrMemSet(XOR_UNIT_ID, 1, MV_FALSE);
	}
#endif
	return MV_OK;
}

#endif /* defined(MV_INCLUDE_CLK_PWR_CNTRL) */

/*******************************************************************************
* mvUpdateNorFlashBaseAddrBank - 
*
* DESCRIPTION:
*       This function update the CFI driver base address bank with on board NOR
*       devices base address.
*
* INPUT:
*
* OUTPUT:
*
* RETURN:
*       None
*
*******************************************************************************/
#if 0
#ifdef	CONFIG_FLASH_CFI_DRIVER
MV_VOID mvUpdateNorFlashBaseAddrBank(MV_VOID)
{
	MV_U32 devBaseAddr;
	MV_U32 devNum = 0;
	int i;

	/* Update NOR flash base address bank for CFI flash init driver */
	for (i = 0 ; i < CONFIG_SYS_MAX_FLASH_BANKS_DETECT; i++)
	{
		devBaseAddr = mvBoardGetDeviceBaseAddr(i,BOARD_DEV_NOR_FLASH);
		if (devBaseAddr != 0xFFFFFFFF)
		{
			flash_add_base_addr (devNum, devBaseAddr);
			devNum++;
		}
	}
	mv_board_num_flash_banks = devNum;

	/* Update SPI flash count for CFI flash init driver */
	/* Assumption only 1 SPI flash on board */
	for (i = 0 ; i < CONFIG_SYS_MAX_FLASH_BANKS_DETECT; i++)
	{
		devBaseAddr = mvBoardGetDeviceBaseAddr(i,BOARD_DEV_SPI_FLASH);
		if (devBaseAddr != 0xFFFFFFFF)
		mv_board_num_flash_banks += 1;
	}
}
#endif	/* CONFIG_FLASH_CFI_DRIVER */
#endif

#if 0
#ifdef MV_INC_BOARD_SPI_FLASH
#include <environment.h>
#include "norflash/mvFlash.h"

void memcpyFlash(env_t *env_ptr, void* buffer, MV_U32 size)
{
    MV_FLASH_INFO *pFlash;
    pFlash = getMvFlashInfo(BOOT_FLASH_INDEX);

    mvFlashBlockRd(pFlash,(MV_U32 *)env_ptr - mvFlashBaseAddrGet(pFlash),
                    size, (MV_U8 *)buffer);
}
#endif
#endif

void mvDramWinConfig(void)
{
	MV_DRAM_DEC_WIN fastPassDecWin;
	MV_AHB_TO_MBUS_DEC_WIN CrossBarDecWin;
	MV_U32 satrValue;

	satrValue = mvBoardFreqGet();
	if (mvDramIfWinGet(SDRAM_CS0, &fastPassDecWin) != MV_OK) {
		mvOsPrintf("testWin2FastPass: Failed to get DRAM CS0 window\n");
		return ;
	}
	if ((3 == satrValue) || (2 == satrValue)) {
		/* work thru cross bar */
		if (fastPassDecWin.enable)  { /* wroking thru Fast Pass*/
			CrossBarDecWin.target =  SDRAM_CS0;
			CrossBarDecWin.addrWin.baseHigh = fastPassDecWin.addrWin.baseHigh;
			CrossBarDecWin.addrWin.baseLow  = fastPassDecWin.addrWin.baseLow;
			CrossBarDecWin.addrWin.size     = fastPassDecWin.addrWin.size;
			CrossBarDecWin.enable = fastPassDecWin.enable;
			mvAhbToMbusWinSet(0, &CrossBarDecWin); /* enable cross bard windows */
		}
		MV_REG_BIT_RESET(SDRAM_SIZE_REG(0, SDRAM_CS0), SCSR_WIN_EN); /* disable fast pass*/
	} else {
		/* DDR thru fast pass */
		if (MV_FALSE == fastPassDecWin.enable) { /* wroking thru cross barr*/
			mvAhbToMbusWinGet(0,&CrossBarDecWin);
			fastPassDecWin.addrWin.baseHigh = CrossBarDecWin.addrWin.baseHigh;
			fastPassDecWin.addrWin.baseLow  = CrossBarDecWin.addrWin.baseLow;
			fastPassDecWin.addrWin.size     = CrossBarDecWin.addrWin.size;
			fastPassDecWin.enable           = CrossBarDecWin.enable;
			mvDramIfWinSet(SDRAM_CS0, &fastPassDecWin);
		}
		mvAhbToMbusWinEnable(0,MV_FALSE); /* disable crossbar*/ 
	}
}


#ifdef CONFIG_POST
typedef	struct	post_data {
	unsigned long	boot_mode;  /* Boot mode */
} pd_t;

/*
 * Returns 1 if keys pressed to start the power-on long-running tests
 * Called from board_init_f().
 */
int post_hotkeys_pressed(void)
{
	return 0;               /* No hotkeys supported */
}
#endif /* CONFIG_POST */

#if defined(CONFIG_POST) || defined(CONFIG_LOGBUFFER)
void post_word_store (ulong a)
{
	DECLARE_GLOBAL_DATA_PTR;
	pd_t *pd = (pd_t *)(gd + 1);
	pd->boot_mode = a;
	return;
}

ulong post_word_load (void)
{
	DECLARE_GLOBAL_DATA_PTR;
	pd_t *pd = (pd_t *)(gd + 1);
	return pd->boot_mode;
}
#endif  /* CONFIG_POST || CONFIG_LOGBUFFER*/

void board_pre_boot_os(void)
{
	MV_BOARD_INFO *pBoardInfo = mvBoardInfoGet();

	if (pBoardInfo->pBoardPreBootOs)
		pBoardInfo->pBoardPreBootOs(pBoardInfo);
}
