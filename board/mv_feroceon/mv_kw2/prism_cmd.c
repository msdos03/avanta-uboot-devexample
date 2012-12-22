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
 *          |               |
 *          |               ------- loopback
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
        "prism board GbE loopback diagnostics", NULL);

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

static int do_prism_diag_gbe_loopback(int level, int argc, char *argv[])
{
	// TODO(kedong) Fill the register sequences.
	printf("do_prism_diag_gbe_loopback\n");
	return 0;
}

U_BOOT_CMD(
        prism,
	/* Don't forget to check maxargs when new commands are added. */
	7, 0, do_prism,
        "prism - prism specific commands\n",
        "prism - prism specific commands\n"
);
#endif /*CFG_CMD_PRISM*/

