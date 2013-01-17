/* This file contains the PRISM specific commands.
 * The commands are organized as a tree format.
 *
 * prism ------- diag ---- pon ---- help
 *          |       |      |
 *          |       |      |------- loopback
 *          |       |      |
 *          |       |      |------- prbs_rx
 *          |       |      |
 *          |       |      -------- prbs_rx
 *          |       |
 *          |       ------ gbe ---- help
 *          |       |       |
 *          |       |       ------- loopback
 *          |       |
 *          |       ------ sff ---- help
 *          |               |
 *          |               ------- vbi
 *          |               |
 *          |               ------- vei
 *          |               |
 *          |               ------- csum
 *          |
 *          ---- help
 *
 * For help command, you don't need explicit definition, it is automatically
 * taken care of by the framework.
 *
 * Add a new leaf command.
 * =======================
 * 1. Add the command definition
 *
 * PRISM_CMD_LEAF(parent, new_command,
 * 	          "Your command description", "Your syntax, e.g. <param1> <param2>");
 *
 * 2. Append the new_command to the parent's sub commands.
 *
 * static const struct prism_cmd_entry *parent_sub_cmds[] =
 * {
 * 	&parent_old_command1,
 * 	&parent_old_command2,
 * 	&parent_new_command,
 * 	NULL
 * };
 *
 * Add a new command group.
 * ========================
 * 1. Add all leafs and command group belonging to this command group.
 *
 * 2. Add all new commands to sub_commands.
 * static const struct prism_cmd_entry *sub_cmds[] =
 * {
 * 	&leaf_command1,
 * 	&leaf_command2,
 * 	&leaf_command3,
 * 	NULL
 * };
 *
 * 3. Add the command group definition
 *
 * PRISM_CMD_PHONY(parent, new_command_group,
 * 	           "Your command description", "Your syntax, e.g. <param1> <param2>",
 * 	           sub_commands);
 *
 *
 * 4. Append the new_command_group to the parent's sub commands.
 *
 * static const struct prism_cmd_entry *parent_sub_cmds[] =
 * {
 * 	&parent_old_command1,
 * 	&parent_old_command2,
 * 	&parent_new_command_group,
 * 	NULL
 * };
 */
#include <config.h>
#include <common.h>
#include <command.h>
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

#if defined(CONFIG_CMD_PRISM)
#include "prism_sff.h"
#include "prism_gbe.h"

#define FOUR_SPACE "    "
typedef int (*exec_func_t) (int level, int argc, char *argv[]);

struct prism_cmd_entry {
	const char *name;
	const char *desc;
	const char *syntax;
	exec_func_t exec_func;
	const struct prism_cmd_entry **sub_cmds;
};

static void do_help(int level, int argc, char *argv[],
		    const struct prism_cmd_entry *cmd);

/* Generic MACRO to add a command */
#define PRISM_CMD(parent, name, desc, syntax, sub_cmds) \
static int do_##parent##_##name(int level, int argc, char *argv[]); \
static const struct prism_cmd_entry parent##_##name = \
{#name, desc, syntax, do_##parent##_##name, sub_cmds}

/* MACRO for adding a leaf command. */
#define PRISM_CMD_LEAF(parent, name, desc, syntax) \
	PRISM_CMD(parent, name, desc, syntax, NULL)

/* MACRO for adding a command without syntax, usually a command group. */
#define PRISM_CMD_PHONY(parent, name, desc, syntax, sub_cmds) \
static const struct prism_cmd_entry parent##_##name = {#name, desc, syntax, NULL, sub_cmds}

/* SFF field info */
typedef struct sff_field_info_ {
	char *name;          /* field name */
	uint  start_daddr;   /* starting data address of the section */
	uint  off;           /* offset */
	uchar dlen;          /* data length */
	int   (*func)(char *, uchar *);    /* ptr of function */
} sff_field_info;

/* diag sff commands */
PRISM_CMD_LEAF(prism_diag_sff, vbi,
	       "prism board list sfp basic id (0-63) of 0xA0 addr", NULL);

PRISM_CMD_LEAF(prism_diag_sff, vei,
	       "prism board list sfp extended id (64-95) of 0xA0 addr", NULL);

PRISM_CMD_LEAF(prism_diag_sff, csum,
	       "prism board sff verifying checksum verification\n"
	       "ccb - verify CC_BASE at offset 63 of 0xA0 addr\n"
	       "cce - verify CC_EXT at offset 95 of 0xA0 addr\n"
	       "ccd - verify CC_DMI at offset 95 of 0xA2 addr",
	       "<checksum types - ccb cce ccd>\n");

static const struct prism_cmd_entry *prism_diag_sff_sub_cmds[] =
{
	&prism_diag_sff_vbi,
	&prism_diag_sff_vei,
	&prism_diag_sff_csum,
	NULL
};

PRISM_CMD_PHONY(prism_diag, sff,
	       "prism board SFF diagnostics",
	       NULL, prism_diag_sff_sub_cmds);

/* diag pon commands */
PRISM_CMD_LEAF(prism_diag_pon, loopback,
	       "prism board PON loopback diagnostics", NULL);

PRISM_CMD_LEAF(prism_diag_pon, prbs_rx,
	       "prism board prbx RX diagnostics",
	       "<prbs pattern - 7, 15, 23> "
	       "<lock count> <time out>\n");

PRISM_CMD_LEAF(prism_diag_pon, prbs_tx,
	       "prism board prbx TX diagnostics",
	       "<prbs patter - 7, 15, 23>");

static const struct prism_cmd_entry *prism_diag_pon_sub_cmds[] =
{
	&prism_diag_pon_loopback,
	&prism_diag_pon_prbs_rx,
	&prism_diag_pon_prbs_tx,
	NULL
};

PRISM_CMD_PHONY(prism_diag, pon,
	      "prism board PON diagnostics",
	      NULL, prism_diag_pon_sub_cmds);

/* diag gbe commands */
PRISM_CMD_LEAF(prism_diag_gbe, loopback,
	      "prism board GbE loopback diagnostics\n"
#ifdef GBE_CMD_DBG
	      "port - port number to test <range: 0 - 1>\n"
	      "opt  - lpbk or ro\n"
	      "example\n"
	      "    loopback 0 lpbk",
	      "<port> <opt>\n");
#else
	      "port - port number to test <range: 0 - 1>",
	      "<port>\n");
#endif

static const struct prism_cmd_entry *prism_diag_gbe_sub_cmds[] =
{
	&prism_diag_gbe_loopback,
	NULL
};

PRISM_CMD_PHONY(prism_diag, gbe,
	      "prism board GbE diagnostics",
	      NULL, prism_diag_gbe_sub_cmds);

/* diag commands */
static const struct prism_cmd_entry *prism_diag_sub_cmds[] =
{
	&prism_diag_gbe,
	&prism_diag_pon,
	&prism_diag_sff,
	NULL
};

PRISM_CMD_PHONY(prism, diag,
	      "prism board diagnostics",
	      NULL, prism_diag_sub_cmds);

/* root command */
static const struct prism_cmd_entry *prism_sub_cmds[] =
{
	&prism_diag,
	NULL
};

PRISM_CMD_PHONY(root, prism,
	      "prism specific commands",
	      NULL, prism_sub_cmds);

void do_help(int level, int argc, char *argv[],
	     const struct prism_cmd_entry *cmd)
{
	int i;
	if (argc < level)
	{
		return;
	}

	for (i = 0; i < level; ++i)
	{
		printf("%s ", argv[i]);
	}

	if (cmd->desc)
		printf("- %s", cmd->desc);

	printf("\n");

	if (cmd->syntax || cmd->sub_cmds) {
		printf("Syntax:\n");
		if (cmd->syntax)
		{
			printf(FOUR_SPACE"%s", cmd->syntax);
		}

		if (cmd->sub_cmds)
		{
			for (i = 0; cmd->sub_cmds[i]; ++i) {
				printf(FOUR_SPACE"[%s]\n", cmd->sub_cmds[i]->name);
			}
		}
	}
}

static int do_execute_cmd(int level, int argc, char *argv[],
			  const struct prism_cmd_entry *cmd)
{
	int i;
	if (argc < level)
	{
		return 1;
	}

	if (argc == level)
	{
		if (cmd->exec_func)
		{
			return cmd->exec_func(level, argc, argv);
		}
		else
		{
			do_help(level, argc, argv, cmd);
		}
		return 0;
	}

	if (strcmp(argv[level], "help") == 0)
	{
		do_help(level, argc, argv, cmd);
		return 0;
	}

	if (cmd->sub_cmds)
	{
		for (i = 0; cmd->sub_cmds[i]; ++i) {
			if (strcmp(cmd->sub_cmds[i]->name, argv[level]) == 0) {
				return do_execute_cmd(
						level+1, argc, argv,
						cmd->sub_cmds[i]);
			}
		}
	}

	if (cmd->exec_func)
	{
		return cmd->exec_func(level, argc, argv);
	}
	else
	{
		do_help(level, argc, argv, cmd);
	}
	return 0;
}

static int do_prism(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	do_execute_cmd(1, argc, argv, &root_prism);
	return 1;
}

static int do_prism_diag_pon_loopback(int level, int argc, char *argv[])
{
	// TODO(kedong) Fill the register sequences.
	printf("do_prism_diag_pon_loopback\n");
	return 0;
}

static int do_prism_diag_pon_prbs_rx(int level, int argc, char *argv[])
{
	// TODO(kedong) Fill the register sequences.
	printf("do_prism_diag_pon_prbs_rx\n");
	return 0;
}

static int do_prism_diag_pon_prbs_tx(int level, int argc, char *argv[])
{
	// TODO(kedong) Fill the register sequences.
	printf("do_prism_diag_pon_prbs_tx\n");
	return 0;
}

#ifdef GBE_CMD_DBG
 #define MAX_GBE_LPBK_PARAMS	6
#else
 #define MAX_GBE_LPBK_PARAMS	5
#endif
static int do_prism_diag_gbe_loopback(int level, int argc, char *argv[])
{
	int ret = 1;
	int port;
	char *pport_str = argv[4];
	char *pcmd_opt = argv[5];

	do {
		if (argc > MAX_GBE_LPBK_PARAMS) {
			printf("Error - too many input parameters\n");
			break;
		}

		PRISM_DBG("do_prism_diag_gbe_loopback - port=%s, opt=%s", argv[4], argv[5]);
		/* validate the port number */
		if ((strlen(pport_str) != 1) || (*pport_str < '0') || (*pport_str > '1')) {
			printf("Error - invalid port number (port=%s)\n", pport_str);
			break;
		}

		/* get port number */
		port = *pport_str - '0';

#ifdef GBE_CMD_DBG
		if (strcmp(pcmd_opt, "lpbk") == 0) {
			/* gbe loopback test */
			ret = gbe_loopback_test(port, MV_TRUE);
		} else if (strcmp(pcmd_opt, "ro") == 0) {
			/* gbe receive-only test */
			ret = gbe_loopback_test(port, MV_FALSE);
		} else {
			printf("Error - invalid opt (opt=%s)\n", argv[5]);
		}
#else
		ret = gbe_loopback_test(port, MV_TRUE);
#endif
	} while (0);

	printf("%s: gbe loopback test %s\n", __func__, (ret == 0)? "success" : "fail");

	return ret;
}

static int sff_print_digits(char *pentry, uchar *pbuf)
{
	int i;
	sff_field_info *pfield = (sff_field_info *)pentry;
	int	addr = pfield->start_daddr + pfield->off;

	printf("%03d/0x%02X  %s:", addr, addr, pfield->name);
	for (i = 0;	i < pfield->dlen; i++) {
		printf(" 0x%02X", pbuf[i]);
	}
	printf("\n");
	return(0);
}

#define SFF_STR_MAX_LEN		40
static int sff_print_str(char *pentry, uchar *pbuf)
{
	sff_field_info *pfield = (sff_field_info *)pentry;
	int	addr = pfield->start_daddr + pfield->off;
	uchar	sff_str[SFF_STR_MAX_LEN];

	if (pfield->dlen < SFF_STR_MAX_LEN) {
		memcpy(sff_str, pbuf, pfield->dlen);
		sff_str[pfield->dlen] = '\0';
		printf("%03d/0x%02X  %s: %s\n", addr, addr, pfield->name, sff_str);
	}
	else
		printf("dlen is > %d (dlen=%d)\n", SFF_STR_MAX_LEN, pfield->dlen);
	return(0);
}

static int sff_read_print_group(
	sff_addr *psff_addr, sff_field_info *pfields, int entries)
{
	uchar sff_buf[SFF_BUF_LEN];
	sff_field_info *pfield;
	int	i, ret;

	/* read and print the vendor general information.
	 * buffer all read data, so we can make sure data is read only once.
	 */
	ret = i2c_read(
	        psff_addr->chip, psff_addr->daddr, SFF_ADDR_LEN,
	        sff_buf, psff_addr->nbytes);
	if (ret != 0) {
		printf("Error - failed to access the sff addr.\n");
		return(ret);
	}
	/* print out the field data per table setting */
	for (i = 0, pfield = &pfields[0]; i < entries; i++, pfield++) {
		/* pass the staring addr of the field to calling routine */
		(pfield->func)((char *)pfield, (sff_buf + pfield->off));
	}

	return(ret);
} /* end of sff_read_print_group */

static int do_prism_diag_sff_vbi(int level, int argc, char *argv[])
{
	int	ret;
	sff_addr	sff_info = {"vbi", SFF_ADDR_A0, SFF_A0_BASE_ID,  A0_BASE_ID_L};
	static const sff_field_info sfp_serial_id[] = {
		{"id",       SFF_A0_BASE_ID, 0,  1,  sff_print_digits},
		{"ext id",   SFF_A0_BASE_ID, 1,  1,  sff_print_digits},
		{"conn",     SFF_A0_BASE_ID, 2,  1,  sff_print_digits},
		{"ven name", SFF_A0_BASE_ID, 20, 16, sff_print_str},
		{"ven pn",   SFF_A0_BASE_ID, 40, 16, sff_print_str},
		{"ven rev",  SFF_A0_BASE_ID, 56, 4,  sff_print_str}
	};

	PRISM_DBG("%s\n", __func__);
	ret = sff_read_print_group(&sff_info,
	                           (sff_field_info *)sfp_serial_id,
	                           (sizeof(sfp_serial_id)/sizeof(sff_field_info)));

	return(ret);
}

static int do_prism_diag_sff_vei(int level, int argc, char *argv[])
{
	int	ret;
	sff_addr	sff_info = {"vei", SFF_ADDR_A0, SFF_A0_EXT_ID, A0_EXT_ID_L};
	static const sff_field_info sfp_ext_id[] = {
		{"ven sn",     SFF_A0_EXT_ID, 4, 16,  sff_print_str},
		{"date code",  SFF_A0_EXT_ID, 20, 8,  sff_print_str}
	};

	PRISM_DBG("%s\n", __func__);
	ret = sff_read_print_group(&sff_info,
	                           (sff_field_info *)sfp_ext_id,
	                           (sizeof(sfp_ext_id)/sizeof(sff_field_info)));

	return(ret);
}

static int do_prism_diag_sff_csum(int level, int argc, char *argv[])
{
	int	ret;

	PRISM_DBG("%s: argc=%d, argv[0]=%s\n", __func__, argc, argv[4]);
	ret = sff_read_verify_checksum(argv[4]);
	return(ret);
}

U_BOOT_CMD(
        prism,
	/* Don't forget to check maxargs when new commands are added. */
	14, 0, do_prism,
        "prism - prism specific commands\n",
        "prism - prism specific commands\n"
);
#endif /*CFG_CMD_PRISM*/

