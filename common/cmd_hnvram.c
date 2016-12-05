/*
 * cmd_hnvram.c -- makes hnvram contents available in u-boot.
 *      Loads contents of hnvram from spi flash and saves it
 *      to environment variables named HNV_<name>.
 *
 * Copyright (C) 2015 Google Inc.
 * Author: Chris Gibson <cgibson@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <command.h>
#include <common.h>
#include <malloc.h>
#include <spi_flash.h>

/* local debug macro */
#undef HNVRAM_DEBUG

#ifdef HNVRAM_DEBUG
#define DEBUG(fmt, args...)  printf(fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif  /* HNVRAM_DEBUG */

#define CONFIG_SF_DEFAULT_MODE SPI_MODE_3

#define HNVRAM_BLOCKSIZE 0x00020000

#define HNVRAM_MTD_OFFSET 0x00100000
#define HNVRAM_B1_OFFSET 0x00100000
#define HNVRAM_B2_OFFSET 0x00140000

#define MAX_HNVRAM_SIZE  0x00200000   // this is the mtd partition size

#define CMD_RET_SUCCESS 0
#define CMD_RET_FAILURE 1
#define CMD_RET_USAGE -1

/* these keys are stored in binary format for historical reasons */
const char *hnvram_binary_keys[] = {
	"LOADER_VERSION",
	"ACTIVATED_KERNEL_NUM",
	"HW_VER",
	"HDCP_KEY",
	"DTCP_KEY",
};

static void *xmalloc(size_t size)
{
	void *p = NULL;
	if (!(p = malloc(size))) {
		printf("error: memory not allocated\n");
		return 0;
	}
	memset(p, 0, size);
	return p;
}

int read_u8(const char **p)
{
	int v = *(const unsigned char *)(*p);
	*p += 1;
	return v;
}

int read_s32_be(const char **p)
{
	const unsigned char *vp = (const unsigned char *)*p;
	*p += 4;
	return (vp[0]<<24) + (vp[1]<<16) + (vp[2]<<8) + vp[3];
}

int read_u16_le(const char **p)
{
	const unsigned char *up = (const unsigned char *)(*p);
	*p += 2;
	return up[0] + (up[1] << 8);
}

int is_hnvram_binary(const char *name, int namelen)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hnvram_binary_keys); i++) {
		const char *k = hnvram_binary_keys[i];
		if ((int)strlen(k) == namelen && strncmp(k, name, namelen) == 0) {
			return 1;
		}
	}
	return 0;
}

char *encode_hex(const char *s, int len)
{
	char *optr, *out = xmalloc(len * 2 + 1);
	for (optr = out; len > 0; len--) {
		sprintf(optr, "%02x", read_u8(&s));
		optr += 2;
	}
	return out;
}

char *encode_macaddr(const char *mac)
{
	int i;
	char *out = xmalloc(6 * 2 + 5 + 2);
	for (i = 0; i < 6; i++) {
		sprintf(out + i * 3, "%02X:", read_u8(&mac));
	}
	out[6*2 + 5] = '\0';
	return out;
}

static void _copy_setenv(const char *name, int namelen,
		const char *val, int vallen)
{
	char *n, *v;
	if (namelen + vallen < 128) {
		n = xmalloc(4 + namelen + 1);
		v = xmalloc(vallen + 1);
		memcpy(n, "HNV_", 4);
		memcpy(n + 4, name, namelen);
		memcpy(v, val, vallen);
		n[namelen+4] = 0;
		v[vallen] = 0;
		setenv(n, v);
		free(n);
		free(v);
	} else {
		DEBUG("ignoring oversized val: %.15s, vallen: %d\n", val, vallen);
	}
}

static int _parse_hnvram(const char *buf, int len)
{
	// An hnvram structure. Format is a tag-length-value sequence of:
	//    [1 byte]   type (1 for notdone, 0 for done)
	//    [4 bytes]  record length
	//    [1 byte]   key length
	//    [x bytes]  key
	//    [4 bytes]  value length
	//    [y bytes]  value
	int rectype, reclen, namelen, vallen;
	int done = 0;
	const char *name, *val, *p = buf;
	while (p - buf <= len + 11) {
		rectype = read_u8(&p);
		if (rectype == 0x00) {
			DEBUG("done processing hnvram block!");
			done = 1;
			break;
		}
		if (rectype != 0x01) {
			DEBUG("error: hnvram invalid rectype %x\n", rectype);
			return -1;
		}

		reclen = read_s32_be(&p);
		if (reclen <= 6 || (p - buf) + reclen >= len) {
			DEBUG("error: hnvram invalid reclen %d\n", reclen);
			return -1;
		}
		namelen = read_u8(&p);
		if (namelen < 1 || (p - buf) + namelen >= len) {
			DEBUG("error: hnvram invalid namelen %d\n", namelen);
			return -1;
		}
		name = p;
		p += namelen;
		vallen = read_s32_be(&p);
		if (vallen < 0 || (p - buf) + vallen >= len) {
			DEBUG("error: hnvram invalid vallen %d\n", vallen);
			return -1;
		}
		val = p;
		p += vallen;
		if (vallen == 6 && namelen >= 8 &&
				strncmp("MAC_ADDR", name, 8) == 0) {
			char *macstr = encode_macaddr(val);
			_copy_setenv(name, namelen, macstr, strlen(macstr));
			free(macstr);
		} else if (is_hnvram_binary(name, namelen)) {
			char *hexstr = encode_hex(val, vallen);
			_copy_setenv(name, namelen, hexstr, strlen(hexstr));
			free(hexstr);
		} else {
			_copy_setenv(name, namelen, val, vallen);
		}
	}
	if (!done) {
		DEBUG("error: failed to find final hnvram record?\n");
		return -1;
	}
	return 0;
}

#if defined(CONFIG_CMD_HNVRAM)
int do_hnvram(void) {
	unsigned int bus = 0;
	unsigned int cs = 0;
	unsigned int speed = 0x1312d00;  // hz
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	char *buf;
	struct spi_flash *flash;

	flash = spi_flash_probe(bus, cs, speed, mode);
	if (!flash) {
		printf("failed to initialize SPI flash at %u:%u\n", bus, cs);
		return CMD_RET_FAILURE;
	}
	buf = (char *)xmalloc(MAX_HNVRAM_SIZE);
	if (!buf) {
		printf("failed to allocate memory for hnvram contents\n");
		return CMD_RET_FAILURE;
	}
	DEBUG("malloc'd new hnvram buffer at 0x%p\n", buf);

	// Maximum size of hnvram partition is 2MB, so read up to that.
	if (spi_flash_read(flash, HNVRAM_MTD_OFFSET, MAX_HNVRAM_SIZE, buf) != 0) {
		printf("reading hnvram from SPI flash failed: off:%x, len:%x\n",
			HNVRAM_MTD_OFFSET, MAX_HNVRAM_SIZE);
		return -1;
	}

	// Next step: Seek to different parts of the buffer that contain the
	// first, second, and third sections of hnvram.
	if (_parse_hnvram(buf, HNVRAM_BLOCKSIZE) != 0) {
		printf("failed parsing hnvram at offset: 0x%p\n", buf);
		return -1;
	}
	if (_parse_hnvram(buf+HNVRAM_B1_OFFSET, HNVRAM_BLOCKSIZE) != 0) {
		printf("failed parsing hnvram at offset: 0x%p\n", buf+HNVRAM_B1_OFFSET);
		return -1;
	}
	if (_parse_hnvram(buf+HNVRAM_B2_OFFSET, HNVRAM_BLOCKSIZE) != 0) {
		printf("failed parsing hnvram at offset: 0x%p\n", buf+HNVRAM_B2_OFFSET);
		return -1;
	}

	free(buf);
	return CMD_RET_SUCCESS;
}

static int do_hnvram_cmd(cmd_tbl_t *cmdtp, int flag, int argc,
		char * const argv[]) {
	return do_hnvram();
}

U_BOOT_CMD(
	hnvram, 1, 0, do_hnvram_cmd,
	"load hnvram from flash",
	"\n"
	"load hnvram from flash into environment vars named HNV_<name>\n"
	);
#endif  /* CONFIG_CMD_HNVRAM */
