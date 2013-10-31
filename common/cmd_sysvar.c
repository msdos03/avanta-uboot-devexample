/* Copyright 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 */

#include <common.h>
#include <spi_flash.h>
#include <asm/io.h>

#include "sysvar.h"

#define SYSVAR_RO_MEM       0x00000100
#define SYSVAR_RW_MEM       SYSVAR_RO_MEM + SYSVAR_BLOCK_SIZE

struct spi_flash *sf_dev = NULL;
struct sysvar_buf ro_buf;
struct sysvar_buf rw_buf;
const long sysvar_offset[SYSVAR_SPI_BLOCK] = {
  SYSVAR_RW_OFFSET0, SYSVAR_RW_OFFSET1, SYSVAR_RO_OFFSET0, SYSVAR_RO_OFFSET1
};

/*
 * print_msg - print the message string
 */
static void print_msg(char *msg, int idx) {
  if (idx < 0) {
    printf("SV: %s\n", msg);
  } else {
    printf("SV: System variables(%s) has been %s\n",
          (idx < SYSVAR_RO_BUF) ? "RW" : "RO", msg);
  }
}

/*
 * print_err - print the error message
 */
static void print_err(char *err, int idx) {
  if (idx < 0) {
    printf("## Error: %s\n", err);
  } else {
    printf("## Error: failed to %s system variables(%s)\n",
           err, (idx < SYSVAR_RO_BUF) ? "RW" : "RO");
  }
}

/*
 * data_recovery - system variables recovering routine
 */
int data_recovery(struct sysvar_buf *buf, int idx) {
  int i, j, ret;

  /* load the system variables */
  printf("SV: Recovering data");
  for (i = idx, j = idx + 1; i < idx + 2; i++, j--) {
    /* read the data from SPI flash */
    if (spi_flash_read(sf_dev, sysvar_offset[i], buf->data_len, buf->data))
      continue;

    /* check crc32 and wc32 (write count) */
    if (check_var(buf, SYSVAR_LOAD_MODE) == SYSVAR_SUCCESS) {
#ifdef CONFIG_SPI_FLASH_PROTECTION
      printf("SV: Unprotecting flash\n");
      ret = spi_flash_protect(sf_dev, 0);
      if (ret) {
        printf("## Error: failed to unprotect flash\n");
        goto recovery_err;
      }
#endif

      /* erase SPI flash */
      ret = spi_flash_erase(sf_dev, sysvar_offset[j], buf->data_len);
      if (ret) {
        print_err("erase", j);
        goto recovery_err;
      }

      /* check crc32 and wc32 (write count) */
      if (check_var(buf, SYSVAR_SAVE_MODE))
        goto recovery_err;

      /* write system variables(RW) to SPI flash */
      ret = spi_flash_write(sf_dev, sysvar_offset[j],
                            buf->data_len, buf->data);
      printf("\n");
      if (ret) {
        print_err("write", j);
        goto recovery_err;
      }

#ifdef CONFIG_SPI_FLASH_PROTECTION
     printf("SV: Protecting flash\n");
     ret = spi_flash_protect(sf_dev, 1);
     if (ret)
       printf("## Error: failed to protect flash\n");
#endif

      buf->loaded = true;
      print_msg("Data recovery was completed", SYSVAR_MESSAGE);
      return 0;
    }
  }

recovery_err:
  clear_buf(buf);

  printf("\n");
  return 0;
}

/*
 * data_load - load the data from SPI flash to data buffer
 */
int data_load(struct sysvar_buf *buf, int idx) {
  int i, j;

  /* load the system variables */
  for (i = idx, j = 0; i < idx + 2; i++, j++) {
    buf->failed[j] = false;

    /* read the data from SPI flash */
    if (spi_flash_read(sf_dev, sysvar_offset[i], buf->data_len, buf->data))
      buf->failed[j] = true;

    /* check crc32 and wc32 (write count) */
    if (check_var(buf, SYSVAR_LOAD_MODE))
      buf->failed[j] = true;
  }

load_err:
  if (buf->failed[0] || buf->failed[1])
    return data_recovery(buf, idx);
  return 0;
}

/*
 * data_save - save the data from data buffer to SPI flash
 */
int data_save(struct sysvar_buf *buf, int *idx) {
  int i, j, ret;

#ifdef CONFIG_SPI_FLASH_PROTECTION
  printf("SV: Unprotecting flash\n");
  ret = spi_flash_protect(sf_dev, 0);
  if (ret) {
    printf("## Error: failed to unprotect flash\n");
    return 1;
  }
#endif

  /* save the system variables */
  for (j = 0; j < 2; j++) {
    i = idx[j];
    printf("SV: Erasing SPI flash 0x%08lx - 0x%08lx\n",
           sysvar_offset[i], sysvar_offset[i] + buf->data_len);
    ret = spi_flash_erase(sf_dev, sysvar_offset[i], buf->data_len);
    if (ret) {
      print_err("erase", i);
      return 1;
    }

    /* check crc32 and wc32 (write count) */
    if (check_var(buf, SYSVAR_SAVE_MODE)) {
      print_err("save", SYSVAR_RO_BUF);
      return 1;
    }

    /* write system variables to SPI flash */
    printf("SV: Writing to SPI flash");
    ret = spi_flash_write(sf_dev, sysvar_offset[i], buf->data_len, buf->data);
    printf("\n");
    if (ret) {
      print_err("write", i);
      return 1;
    }
  }

#ifdef CONFIG_SPI_FLASH_PROTECTION
  printf("SV: Protecting flash\n");
  ret = spi_flash_protect(sf_dev, 1);
  if (ret) {
    printf("## Error: failed to protect flash\n");
    return 1;
  }
#endif

  return 0;
}

/*
 * sf_open - open SPI flash and read the data to buffer
 */
int sf_open(bool load) {
  /* check SPI flash */
  if (sf_dev != NULL)
    return 0;

  memset(&rw_buf, 0, sizeof(rw_buf));
  memset(&ro_buf, 0, sizeof(ro_buf));

  /* probe SPI flash */
  sf_dev = spi_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
                           CONFIG_ENV_SPI_MAX_HZ, SPI_MODE_3);
  if (sf_dev == NULL) {
    print_err("failed to initialize SPI flash", SYSVAR_MESSAGE);
    return 1;
  }

  /* allocate data buffers */
  rw_buf.data = (uchar *)map_physmem(SYSVAR_RW_MEM,
                                     SYSVAR_BLOCK_SIZE, MAP_WRBACK);
  ro_buf.data = (uchar *)map_physmem(SYSVAR_RO_MEM,
                                     SYSVAR_BLOCK_SIZE, MAP_WRBACK);
  if (!rw_buf.data || !ro_buf.data) {
    print_err("failed to map physical memory", SYSVAR_MESSAGE);
    goto open_err;
  }

  /* allocate data lists */
  rw_buf.list = (struct sysvar_list *)malloc(sizeof(struct sysvar_list));
  ro_buf.list = (struct sysvar_list *)malloc(sizeof(struct sysvar_list));
  if (!rw_buf.list || !ro_buf.list) {
    print_err("failed to allocate data list", SYSVAR_MESSAGE);
    goto open_err;
  }

  rw_buf.data_len = SYSVAR_BLOCK_SIZE;
  rw_buf.total_len = SYSVAR_BLOCK_SIZE - SYSVAR_HEAD;
  rw_buf.free_len = rw_buf.total_len;
  rw_buf.readonly = false;

  strncpy(rw_buf.list->name, "rw", SYSVAR_NAME);
  rw_buf.list->value = NULL;
  rw_buf.list->len = SYSVAR_NAME + 2;
  rw_buf.list->next = NULL;

  ro_buf.data_len = SYSVAR_BLOCK_SIZE;
  ro_buf.total_len = SYSVAR_BLOCK_SIZE - SYSVAR_HEAD;
  ro_buf.free_len = ro_buf.total_len;
  ro_buf.readonly = true;

  strncpy(rw_buf.list->name, "ro", SYSVAR_NAME);
  ro_buf.list->value = NULL;
  ro_buf.list->len = SYSVAR_NAME + 2;
  ro_buf.list->next = NULL;

  /* load data from SPI flash to data buffer */
  if (load) {
    if (sf_loadvar())
      goto open_err;
  }
  return 0;

open_err:
  sf_close();
  return 1;
}

/*
 * sf_close - close SPI flash and release the data buffer
 */
int sf_close(void) {
  /* release data lists */
  if (rw_buf.list != NULL) {
    clear_var(&rw_buf);
    free(rw_buf.list);
  }
  if (ro_buf.list != NULL) {
    clear_var(&ro_buf);
    free(ro_buf.list);
  }

  /* release data buffers */
  if (rw_buf.data != NULL)
    unmap_physmem(rw_buf.data, rw_buf.data_len);
  if (ro_buf.data != NULL)
    unmap_physmem(ro_buf.data, ro_buf.data_len);

  sf_dev = NULL;
  return 0;
}

/*
 * sf_loadvar - load the data from SPI flash to data buffer
 */
int sf_loadvar(void) {
  if (data_load(&rw_buf, SYSVAR_RW_BUF))
    return 1;

  /* move the data from data buffer to data list */
  if (load_var(&rw_buf))
    return 1;

  rw_buf.loaded = true;
  print_msg("loaded", SYSVAR_RW_BUF);

  if (data_load(&ro_buf, SYSVAR_RO_BUF))
    return 1;

  /* move the data from data buffer to data list */
  if (load_var(&ro_buf))
    return 1;

  ro_buf.loaded = true;
  print_msg("loaded", SYSVAR_RO_BUF);
  return 0;
}

/*
 * sf_savevar - save the data from data buffer to SPI flash
 */
int sf_savevar(struct sysvar_buf *buf, int idx) {
  int save_idx[2];

  if (sf_open(true))
    return 1;

  /* move the data from data list to data buffer */
  if (save_var(buf))
    return 1;

  /* erase failed partition first
   *  part0   part1       erase
   *  -----   -----       -----
   *    ok      ok        0, 1
   *  failed    ok        0, 1
   *    ok    failed      1, 0
   *  failed  failed      0, 1
   */
  if (buf->failed[1]) {
    save_idx[0] = idx + 1;
    save_idx[1] = idx;
  } else {
    save_idx[0] = idx;
    save_idx[1] = idx + 1;
  }

  /* save the data from data buffer to SPI flash */
  if (data_save(buf, save_idx))
    return 1;

  printf("\n");
  print_msg("saved", idx);
  return 0;
}

/*
 * sf_getvar - get or print the system variable from data list
 */
int sf_getvar(char *name, char *value, int len) {
  struct sysvar_list *var = NULL;

  if (sf_open(true))
    return 1;

  if (name == NULL) {
    /* print all system variables(RO) */
    print_var(&ro_buf);
    /* print all system variables(RW) */
    print_var(&rw_buf);
    return 0;
  }

  /* find the system variable(RO) */
  var = find_var(&ro_buf, name);
  if (var != NULL)
    goto get_data;

  /* find the system variable(RW) */
  var = find_var(&rw_buf, name);
  if (var != NULL)
    goto get_data;

  /* system variable not found */
  printf("## SYSVAR: '%s' not found\n", name);
  return 1;

get_data:
  return get_var(var, name, value, len);
}

/*
 * sf_setvar - add or delete the system variable in data list
 */
int sf_setvar(struct sysvar_buf *buf, int idx, char *name, char *value) {
  struct sysvar_list *var = NULL;
  int ret = 0;

  if (sf_open(true))
    return 1;

  if (name != NULL) {
    if (idx < SYSVAR_RO_BUF) {
      /* read only variable? */
      var = find_var(&ro_buf, name);
    } else {
      /* variable existed? */
      var = find_var(&rw_buf, name);
    }

    if (var != NULL) {
      if (idx < SYSVAR_RO_BUF) {
        printf("## Error: '%s' is read only variable\n", name);
        return 1;
      } else {
        printf("## Error: '%s' is not read only variable\n", name);
        return 1;
      }
    }

    var = find_var(buf, name);
    if (var != NULL) {
      /* delete system variable */
      ret = delete_var(buf, var);
      if (ret != SYSVAR_SUCCESS) {
        printf("## Error: failed to delete '%s'\n", name);
        return 1;
      }

      /* add system variable */
      if (value != NULL) {
        ret = set_var(buf, name, value);
        if (ret == 0)
          print_msg("added", idx);
      } else {
        print_msg("deleted", idx);
      }
    } else {
      /* add system variable */
      if (value != NULL) {
        ret = set_var(buf, name, value);
        if (ret == 0)
          print_msg("added", idx);
      } else {
        printf("## Error: '%s' not found\n", name);
        ret = 1;
      }
    }
  } else {
    /* delete all of system variables */
    ret = clear_var(buf);
    if (ret == 0)
      print_msg("deleted", idx);
  }
  return ret;
}

/*
 * do_loadvar - load system variables from SPI flash
 *
 * sf_loadvar command:
 *    sf_loadvar - load system variables from persistent storage
 */
int do_loadvar(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]) {
  if (sf_open(false))
    return 1;

  /* load system variables from SPI flash */
  return sf_loadvar();
}

U_BOOT_CMD(
  loadvar, 1, 0, do_loadvar,
  "load system variables",
  "\n    - load system variables from SPI flash\n"
);

/*
 * do_savevar - save system variables(RW) to SPI flash
 *
 * sf_savevar command:
 *    sf_savevar - save system variables(RW) to SPI flash
 */
int do_savevar(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  /* save system variables(RW) */
  return sf_savevar(&rw_buf, SYSVAR_RW_BUF);
}

U_BOOT_CMD(
  savevar, 1, 0, do_savevar,
  "save system variables(RW)",
  "\n    - save system variables(RW) to SPI flash\n"
);

/*
 * do_printvar - print system variables
 *
 * printvar command:
 *    printvar name - print system variable with name
 *    printvar      - print all system variables
 */
int do_printvar(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]) {
  char value[SYSVAR_VALUE];

  if (sf_open(true))
    return 1;

  if (argv[1] == NULL) {
    /* print all system variables */
    sf_getvar(NULL, NULL, 0);

    printf("\nSV: System Variables(RO): %d/%d bytes\n",
      ro_buf.used_len, ro_buf.total_len);
    printf("SV: System Variables(RW): %d/%d bytes\n",
      rw_buf.used_len, rw_buf.total_len);
  } else {
    /* get a system variable */
    if (sf_getvar(argv[1], value, SYSVAR_VALUE) == 0) {
      printf("%s=", argv[1]);
      /* puts value in case CONFIG_SYS_PBSIZE < SYSVAR_VALUE */
      puts(value);
      putc('\n');
      printf("\nSV: System Variable: %d bytes\n",
        (int)(SYSVAR_NAME + 2 + strlen(value)));
    }
  }
  return 0;
}

U_BOOT_CMD(
  printvar, 2, 0, do_printvar,
  "print system variables",
  "\n    - print values of all system variables\n"
  "printvar name ...\n"
  "    - print value of system variable 'name'\n"
);

/*
 * do_setvar - add or delete system variables(RW)
 *
 * sf_setvar command:
 *    sf_setvar name value - add system variable with name:value
 *    sf_setvar name       - delete system variable with name
 *    sf_setvar            - delete all system variables
 */
int do_setvar(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]) {
  int ret = 0;

  if (argc == 3) {
    /* add a system variable(RW) */
    ret = sf_setvar(&rw_buf, SYSVAR_RW_BUF, argv[1], argv[2]);
  } else if (argc == 2) {
    /* delete a system variable(RW) */
    ret = sf_setvar(&rw_buf, SYSVAR_RW_BUF, argv[1], NULL);
  } else {
    /* delete all system variables(RW) */
    ret = sf_setvar(&rw_buf, SYSVAR_RW_BUF, NULL, NULL);
  }
  return ret;
}

U_BOOT_CMD(
  setvar, 3, 0, do_setvar,
  "set system variables(RW)",
  "setvar name value ...\n"
  "    - set system variable(RW) 'name' to 'value ...'\n"
  "setvar name\n"
  "    - delete system variable(RW) 'name'\n"
  "setvar\n"
  "    - delete all system variables(RW)\n"
);

/*
 * sysvar_dump - dump the data buffer in binary/ascii format
 */
void sysvar_dump(struct sysvar_buf *buf, int idx, bool load) {
  extern char console_buffer[];
  int start = 0;

  if (sf_open(load))
    return 1;

  printf("System Variables(%s):\n", (idx < SYSVAR_RO_BUF) ? "RW" : "RO");
  printf("offset : 0x%08lx\n", sysvar_offset[idx]);
  printf("size   : %d bytes\n", buf->data_len);
  printf("total  : %d bytes\n", buf->total_len);
  printf("used   : %d bytes\n", buf->used_len);
  printf("wc32   : 0x%08lx\n", get_wc32(buf));
  printf("crc32  : 0x%08lx\n", get_crc32(buf));

  while (1) {
    /* dump one page data in data buffer */
    dump_buf(buf, start, PAGE_SIZE);
    /* continue to dump...? */
    readline("(n)ext, (p)rev, (f)irst, (l)ast ? >> ");
    if (strcmp(console_buffer, "n") == 0) {
      start += PAGE_SIZE;    /* go to next page */
      if (start >= buf->data_len)
        return;
    } else if (strcmp(console_buffer, "p") == 0) {
      start -= PAGE_SIZE;    /* go to previous page */
      if (start < 0)
        return;
    } else if (strcmp(console_buffer, "f") == 0) {
      if (start == 0)
        return;
      start = 0;  /* go to first page */
    } else if (strcmp(console_buffer, "l") == 0) {
      if (start == buf->data_len - PAGE_SIZE)
        return;
      start = buf->data_len - PAGE_SIZE;  /* go to last page */
    } else {
      return;
    }
  }
}

/*
 * sysvar_io - SPI flash IO operations
 */
int sysvar_io(int argc, char *argv[]) {
  struct sysvar_buf *buf;
  int i, idx, ret = 0;

  if (sf_open(false))
    return 1;

  if (strcmp(argv[1], "0") == 0) {
    idx = 0;
    buf = &rw_buf;
  } else if (strcmp(argv[1], "1") == 0) {
    idx = 1;
    buf = &rw_buf;
  } else if (strcmp(argv[1], "2") == 0) {
    idx = 2;
    buf = &ro_buf;
  } else if (strcmp(argv[1], "3") == 0) {
    idx = 3;
    buf = &ro_buf;
  } else {
    print_err("invalid SPI flash device", SYSVAR_MESSAGE);
    return 1;
  }

  if (strcmp(argv[0], "write") == 0) {
    /* fill data to data buffer */
    for (i = 0; i < buf->data_len; i++)
      buf->data[i] = i;

    /* write the data buffer to spi_flash */
    ret = spi_flash_write(sf_dev, sysvar_offset[idx],
                          buf->data_len, buf->data);
    printf("\n");
  } else if (strcmp(argv[0], "erase") == 0) {
    /* erase spi_flash */
    ret = spi_flash_erase(sf_dev, sysvar_offset[idx], buf->data_len);
  }

  if (ret == 0) {
    ret = spi_flash_read(sf_dev, sysvar_offset[idx],
                         buf->data_len, buf->data);
    if (ret == 0)
      sysvar_dump(buf, idx, false);
  }

  if (ret != 0)
    printf("## Error: SPI flash %s failed at 0x%08lx\n",
           argv[0], sysvar_offset[idx]);

  rw_buf.loaded = false;
  ro_buf.loaded = false;
  return ret;
}

/*
 * do_sysvar - system variable debug functions
 */
int do_sysvar(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]) {
  if (argc < 2)
    goto usage;

  if (strcmp(argv[1], "set") == 0) {
    if (argc == 4) {
      /* add a system variable(RO) */
      return sf_setvar(&ro_buf, SYSVAR_RO_BUF, argv[2], argv[3]);
    } else if (argc == 3) {
      /* delete a system variable(RO) */
      return sf_setvar(&ro_buf, SYSVAR_RO_BUF, argv[2], NULL);
    } else if (argc == 2) {
      /* delete all system variables(RO) */
      return sf_setvar(&ro_buf, SYSVAR_RO_BUF, NULL, NULL);
    } else {
      goto usage;
    }
  }

  if (strcmp(argv[1], "save") == 0 && argc == 2) {
    /* save system variables(RO) */
    return sf_savevar(&ro_buf, SYSVAR_RO_BUF);
  }

  if (strcmp(argv[1], "dump") == 0 && argc == 3) {
    if (strcmp(argv[2], "rw") == 0) {
      /* dump data in data buffer(RW) */
      sysvar_dump(&rw_buf, SYSVAR_RW_BUF, true);
    } else if (strcmp(argv[2], "ro") == 0) {
      /* dump data in data buffer(RO) */
      sysvar_dump(&ro_buf, SYSVAR_RO_BUF, true);
    } else {
      goto usage;
    }
    return 0;
  }

  if ((strcmp(argv[1], "read") == 0 && argc == 3) ||
      (strcmp(argv[1], "write") == 0 && argc == 3) ||
      (strcmp(argv[1], "erase") == 0 && argc == 3))
    return sysvar_io(argc - 1, argv + 1);

usage:
  cmd_usage(cmdtp);
  return 1;
}

U_BOOT_CMD(
  sysvar, 4, 0, do_sysvar,
  "system variable debug functions",
  "set name value\n"
  "    - set system variable(RO) 'name' to 'value ...'\n"
  "sysvar set name\n"
  "    - delete system variable(RO) 'name'\n"
  "sysvar set\n"
  "    - delete all system variables(RO)\n"
  "sysvar save\n"
  "    - save system variables(RO) to SPI flash\n"
  "sysvar dump rw|ro\n"
  "    - dump data in data buffer\n"
  "sysvar read 0|1|2|3\n"
  "    - read data from SPI flash 0|1|2|3 to data buffer\n"
  "sysvar write 0|1|2|3\n"
  "    - write data from data buffer to SPI flash 0|1|2|3\n"
  "sysvar erase 0|1|2|3\n"
  "    - erase data on SPI flash 0|1|2|3\n"
);

