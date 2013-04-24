#ifndef __CONFIG_H
/* configs/mv_kw2.h defines __CONFIG_H */
#include <configs/mv_kw2.h>

#undef MV_INCLUDE_RTC

#define CONFIG_CMD_DIAG

#define CONFIG_RTC_PCF8523
#define CONFIG_SYS_I2C_RTC_ADDR	0x68

#endif /* __CONFIG_H */
