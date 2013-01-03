/*
 * This file contains SFF related definitions
 */
#ifndef PRISM_SFF_H
#define PRISM_SFF_H

#include <common.h>
#include <command.h>
#include <environment.h>
#include <i2c.h>
#include <malloc.h>
#include <asm/byteorder.h>

/**************************************************
 * Definitions
 **************************************************/
#ifdef SFF_CMD_DBG
	#define SFF_DBG(format, args...)  \
		printf("%s: "format"\n", __func__, ## args)
#else
	#define SFF_DBG(format, args...)
#endif

typedef enum _bool{false, true} bool;

#define SFF_ADDR_A0    0x50      /* A0 = 10100000X   b7:1 address, b0 r/w bit */
#define SFF_ADDR_A2    0x51      /* A0 = 10100001X   b7:1 address, b0 r/w bit */

#define SFF_BUF_LEN    128
#define SFF_ADDR_LEN   1         /* SFF address length */
#define CC_BASE        "ccb"
#define CC_EXT         "cce"
#define CC_DMI         "ccd"

#if (defined(CONFIG_POST) && defined(CONFIG_SYS_POST_I2C))
	#define I2C_ADDR_LIST   {SFF_ADDR_A0, SFF_ADDR_A2, 0x64}

	#define SFF_LOG(format, args...)
	#define SFF_ERR_LOG(format, args...) \
		post_log("I2C: "format"\n", ## args)
#else
	#define SFF_LOG(format, args...)  printf(format"\n", ## args)
	#define SFF_ERR_LOG               SFF_LOG
#endif /* end of CONFIG_POST & CONFIG_SYS_POST_I2C */

/* SFF field starting data address and data length */
/* A0 starting address per group */
#define SFF_A0_BASE_ID   0
#define SFF_A0_EXT_ID    64
#define SFF_A0_VEND_SPEC 96
#define SFF_A0_END       127
/* A0 group lengths */
#define A0_BASE_ID_L			(SFF_A0_EXT_ID - SFF_A0_BASE_ID)
#define A0_EXT_ID_L				(SFF_A0_VEND_SPEC - SFF_A0_EXT_ID)
#define A0_VEND_SPEC_L		(SFF_A0_END + 1 - SFF_A0_VEND_SPEC)

/* A0 checksum addresses */
#define SFF_A0_CC_BASE   63
#define SFF_A0_CC_EXT    95

/* A2 starting address per group */
#define SFF_A2_ALRM_WARN   0
#define SFF_A2_EXT_CONSTS  56
#define SFF_A2_RT_DIAG     96
#define SFF_A2_END         179
/* A2 group lengths */
#define A2_ALRM_WARN_L     (SFF_A2_EXT_CONSTS - SFF_A2_ALRM_WARN)
#define A2_CC_DMI_CHKSUM_L (SFF_A2_RT_DIAG - SFF_A2_ALRM_WARN)


/**************************************************
 * local data and function
 **************************************************/

/* SFF verify-checksum command entry */
typedef struct csum_vfy_func_entry_ {
	char *name;      /* cmd string */
	int chip;        /* SFF addr */
	int daddr;       /* data addr to access of the SFF addr */
	int nbytes;      /* number of bytes to access */
} csum_vfy_func_entry, sff_addr;

/* Command lookup table
 * - For verifying CCB_BASE and CC_EXC checksums, read in all of data in
 *   the range including checksum bytes.
 */
static const csum_vfy_func_entry sff_csum_vfy_sub_cmds[] = {
/* name,	chip,				daddr,					nbyte */
	{CC_BASE, SFF_ADDR_A0, SFF_A0_BASE_ID,   A0_BASE_ID_L},
	{CC_EXT,  SFF_ADDR_A0, SFF_A0_EXT_ID,    A0_EXT_ID_L},
	{CC_DMI,  SFF_ADDR_A2, SFF_A2_ALRM_WARN, A2_CC_DMI_CHKSUM_L},
};
#define SIZEOF_CSUM_VFY_SUB_CMD_TBL	(sizeof(sff_csum_vfy_sub_cmds)/sizeof(csum_vfy_func_entry))

/*
 * 8-bit checksum
 * (Copied the routine from mv_feroceon/mv_kw2_mv_cmd.c)
 */
static uchar checksum8(uchar *start, uchar len, uchar csum)
{
	uchar sum = csum;
	volatile uchar *startp = (volatile uchar *)start;

	do {
		sum += *startp;
		startp++;
	} while(--len);

	return (sum);
} /* end of checksum */


static int sff_read_verify_checksum(char *csum_name)
{
	int ret;
	int i;
	uchar sff_buf[SFF_BUF_LEN];
	uchar	csum = 0;
	csum_vfy_func_entry *ptable = (csum_vfy_func_entry *)sff_csum_vfy_sub_cmds;
	bool	found = false;

	do {
		/* Lookup the command table and find the sub-cmd entry */
		for (i = 0; i < SIZEOF_CSUM_VFY_SUB_CMD_TBL; i++, ptable++) {
			SFF_DBG("name=%s, len=%d\n", ptable->name, (int)strlen(ptable->name));
			if (!strncmp(csum_name, ptable->name, (int)strlen(ptable->name))) {
				/* Found the diag func, return entry address */
				found = true;
				break;
			}
		}
		if (found != true) {
			SFF_DBG("Invalid command....\n");
			break;			/* not found. exit */
		}

		/* read and verify the checksum
		 * buffer all read data, so we can make sure data is read only once.
		 */
		ret = i2c_read(
		        ptable->chip, ptable->daddr, SFF_ADDR_LEN,
		        sff_buf, ptable->nbytes);
		if (ret != 0) {
			SFF_ERR_LOG("failed to access the sff chip addr (0x%02x).\n",
			            (ptable->chip<<1));
			break;
		}

		/* Calculate 8-bit checksum */
		csum = checksum8(sff_buf, (ptable->nbytes-1), 0);
		SFF_DBG("calculated csum=0x%02X, csum=0x%02X",
		        csum, sff_buf[ptable->nbytes-1]);
		if (csum != sff_buf[ptable->nbytes-1]) {
			SFF_ERR_LOG("csum mismatched - chip=0x%02x, off= 0x%02x, csum=0x%02x, "
			        "calculated csum=0x%02x",
			        (ptable->chip<<1), (ptable->daddr +ptable->nbytes-1),
			        sff_buf[ptable->nbytes-1], csum);
			ret = 1;	/* fail */
		} else {
			SFF_LOG("csum matched - chip=0x%02x, off= 0x%02x, csum=0x%02x",
			        (ptable->chip<<1), (ptable->daddr+ptable->nbytes-1), csum);
			ret = 0;	/* success */
		}

	} while (false);

	return(ret);
} /* end of sff_read_print_group */

#endif /* end of PRISM_SFF_H */
