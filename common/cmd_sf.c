/*
 * Command for accessing SPI flash.
 *
 * Copyright (C) 2008 Atmel Corporation
 */
#include <common.h>
#include <spi_flash.h>

#include <asm/io.h>

#ifndef CONFIG_SF_DEFAULT_SPEED
# define CONFIG_SF_DEFAULT_SPEED	1000000
#endif
#ifndef CONFIG_SF_DEFAULT_MODE
# define CONFIG_SF_DEFAULT_MODE		SPI_MODE_3
#endif

extern struct spi_flash *flash;

static int do_spi_flash_probe(int argc, char *argv[])
{
	unsigned int bus = 0;
	unsigned int cs;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	char *endp;
	struct spi_flash *new;

	if (argc < 2)
		goto usage;

	cs = simple_strtoul(argv[1], &endp, 0);
	if (*argv[1] == 0 || (*endp != 0 && *endp != ':'))
		goto usage;
	if (*endp == ':') {
		if (endp[1] == 0)
			goto usage;

		bus = cs;
		cs = simple_strtoul(endp + 1, &endp, 0);
		if (*endp != 0)
			goto usage;
	}

	if (argc >= 3) {
		speed = simple_strtoul(argv[2], &endp, 0);
		if (*argv[2] == 0 || *endp != 0)
			goto usage;
	}
	if (argc >= 4) {
		mode = simple_strtoul(argv[3], &endp, 16);
		if (*argv[3] == 0 || *endp != 0)
			goto usage;
	}

	new = spi_flash_probe(bus, cs, speed, mode);
	if (!new) {
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return 1;
	}

	if (flash)
		spi_flash_free(flash);
	flash = new;

	printf("%u KiB %s at %u:%u is now current device\n",
			flash->size >> 10, flash->name, bus, cs);

	return 0;

usage:
	puts("Usage: sf probe [bus:]cs [hz] [mode]\n");
	return 1;
}

static int do_spi_flash_read_write(int argc, char *argv[])
{
	unsigned long addr;
	unsigned long offset;
	unsigned long len;
	void *buf;
	char *endp;
	int ret;

	if (argc < 4)
		goto usage;

	addr = simple_strtoul(argv[1], &endp, 16);
	if (*argv[1] == 0 || *endp != 0)
		goto usage;
	offset = simple_strtoul(argv[2], &endp, 16);
	if (*argv[2] == 0 || *endp != 0)
		goto usage;
	len = simple_strtoul(argv[3], &endp, 16);
	if (*argv[3] == 0 || *endp != 0)
		goto usage;

	buf = map_physmem(addr, len, MAP_WRBACK);
	if (!buf) {
		puts("Failed to map physical memory\n");
		return 1;
	}

	if (strcmp(argv[0], "read") == 0)
		ret = spi_flash_read(flash, offset, len, buf);
	else
		ret = spi_flash_write(flash, offset, len, buf);

	unmap_physmem(buf, len);

	if (ret) {
		printf("SPI flash %s failed\n", argv[0]);
		return 1;
	}

	return 0;

usage:
	printf("Usage: sf %s addr offset len\n", argv[0]);
	return 1;
}

static int do_spi_flash_erase(int argc, char *argv[])
{
	unsigned long offset;
	unsigned long len;
	char *endp;
	int ret;

	if (argc < 3)
		goto usage;

	offset = simple_strtoul(argv[1], &endp, 16);
	if (*argv[1] == 0 || *endp != 0)
		goto usage;
	len = simple_strtoul(argv[2], &endp, 16);
	if (*argv[2] == 0 || *endp != 0)
		goto usage;


	ret = spi_flash_erase(flash, offset, len);
	if (ret) {
		printf("SPI flash %s failed\n", argv[0]);
		return 1;
	}

	return 0;

usage:
	puts("Usage: sf erase offset len\n");
	return 1;
}
#ifdef CONFIG_SPI_FLASH_PROTECTION
static int do_spi_flash_protect(int argc, char *argv[])
{
	int ret;
	const char *cmd;

	if (argc < 2)
		goto usage;

	cmd = argv[1];
	
	if (strcmp(cmd, "on") == 0)
		ret = spi_flash_protect(flash, 1);
	if (strcmp(cmd, "off") == 0)
		ret = spi_flash_protect(flash, 0);
	if (ret) {
		printf("SPI flash %s failed\n", argv[0]);
		return 1;
	}

	return 0;

usage:
	puts("Usage: sf protect on/off\n");
	return 1;
}

static int do_spi_flash_lock(int argc, char *argv[])
{
	int ret;
	int lock;
	char *endp;
	unsigned long offset;

	if (argc < 2)
		goto usage;

	offset = simple_strtoul(argv[1], &endp, 0);
	if (*endp)
		goto usage;

	if (argc < 3)
		ret = spi_flash_read_lock(flash, offset, &lock);
	else {
		if (!strcmp(argv[2], "off"))
			lock = SPI_FLASH_LOCK_NONE;
		else if (!strcmp(argv[2], "on"))
			lock = SPI_FLASH_LOCK_WRITE;
		else if (!strcmp(argv[2], "never"))
			lock = SPI_FLASH_LOCK_DOWN;
		else if (!strcmp(argv[2], "forever"))
			lock = SPI_FLASH_LOCK_WRITE|SPI_FLASH_LOCK_DOWN;
		else
			goto usage;

		ret = spi_flash_write_lock(flash, offset, lock);
	}

	if (ret) {
		printf("SPI flash %s failed, error %d\n", argv[0], ret);
		return 1;
	}
	else if (argc < 3) {
		switch (lock) {
		case SPI_FLASH_LOCK_NONE:
			puts("off\n");
			break;
		case SPI_FLASH_LOCK_WRITE:
			puts("on\n");
			break;
		case SPI_FLASH_LOCK_DOWN:
			puts("never\n");
			break;
		case SPI_FLASH_LOCK_WRITE|SPI_FLASH_LOCK_DOWN:
			puts("forever\n");
			break;
		default:
			printf("? (%x)\n", lock);
			break;
		}
	}

	return 0;

usage:
	puts("Usage: sf lock offset [mode]\n"
		" modes:\n"
		"  'off' - unlocked\n"
		"  'on' - locked\n"
		"  'never' - unlock until power cycle\n"
		"  'forever' - lock until power cycle\n");

	return 1;
}

static int do_spi_flash_lock_range(int argc, char *argv[])
{
	int ret;
	int lock;
	char *endp;
	unsigned long offset;
	unsigned long len;

	if (argc < 4)
		goto usage;

	offset = simple_strtoul(argv[1], &endp, 0);
	if (*endp)
		goto usage;

	len = simple_strtoul(argv[2], &endp, 0);
	if (*endp)
		goto usage;

	if (!strcmp(argv[3], "off"))
		lock = SPI_FLASH_LOCK_NONE;
	else if (!strcmp(argv[3], "on"))
		lock = SPI_FLASH_LOCK_WRITE;
	else if (!strcmp(argv[3], "never"))
		lock = SPI_FLASH_LOCK_DOWN;
	else if (!strcmp(argv[3], "forever"))
		lock = SPI_FLASH_LOCK_WRITE|SPI_FLASH_LOCK_DOWN;
	else
		goto usage;

	ret = spi_flash_lock(flash, offset, len, lock);
	if (ret) {
		printf("SPI flash %s failed, error %d\n", argv[0], ret);
		return 1;
	}

	return 0;

usage:
	puts("Usage: sf lock-range offset len mode\n"
		" modes:\n"
		"  'off' - unlocked\n"
		"  'on' - locked\n"
		"  'never' - unlock until power cycle\n"
		"  'forever' - lock until power cycle\n");

	return 1;
}
#endif

static int do_spi_flash(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	const char *cmd;

	/* need at least two arguments */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	if (strcmp(cmd, "probe") == 0)
		return do_spi_flash_probe(argc - 1, argv + 1);

	/* The remaining commands require a selected device */
	if (!flash) {
		puts("No SPI flash selected. Please run `sf probe'\n");
		return 1;
	}

	if (strcmp(cmd, "read") == 0 || strcmp(cmd, "write") == 0)
		return do_spi_flash_read_write(argc - 1, argv + 1);
	if (strcmp(cmd, "erase") == 0)
		return do_spi_flash_erase(argc - 1, argv + 1);
#ifdef CONFIG_SPI_FLASH_PROTECTION
	if (strcmp(cmd, "protect") == 0)
		return do_spi_flash_protect(argc - 1, argv + 1);
	if (strcmp(cmd, "lock") == 0)
		return do_spi_flash_lock(argc - 1, argv + 1);
	if (strcmp(cmd, "lock-range") == 0)
		return do_spi_flash_lock_range(argc - 1, argv + 1);
#endif

usage:
	cmd_usage(cmdtp);
	return 1;
}

U_BOOT_CMD(
	sf,	5,	1,	do_spi_flash,
	"SPI flash sub-system",
	"probe [bus:]cs [hz] [mode]	- init flash device on given SPI bus\n"
	"				  and chip select\n"
	"sf read addr offset len 	- read `len' bytes starting at\n"
	"				  `offset' to memory at `addr'\n"
	"sf write addr offset len	- write `len' bytes from memory\n"
	"				  at `addr' to flash at `offset'\n"
	"sf erase offset len		- erase `len' bytes from `offset'\n"
#ifdef CONFIG_SPI_FLASH_PROTECTION
	"sf protect on			- protect spi flash\n"
	"sf protect off			- unprotect spi flash\n"
	"sf lock offset [mode]		- get sector lock or set to `mode'\n"
	"sf lock-range offset len mode	- set address range lock to `mode'\n"
#endif
);
