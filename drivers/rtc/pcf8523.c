/*
 * (C) Copyright 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Date & Time support for Philips PCF8523 RTC
 */

#include <common.h>
#include <rtc.h>
#include <i2c.h>

#define CONTROL_3_REG	0x02
#define CONTROL_3_BLF	0x04	/* 0: ok, 1: low */
#define SECONDS_REG	0x03
#define MINUTES_REG	0x04
#define HOURS_REG	0x05
#define DAYS_REG	0x06
#define WEEKDAYS_REG	0x07
#define MONTHS_REG	0x08
#define YEARS_REG	0x09

#ifdef DEBUG
static uchar rtc_read  (uchar reg);
#endif
static void  rtc_write (uchar reg, uchar val);
static uchar bin2bcd   (unsigned int n);
static unsigned bcd2bin(uchar c);

/* ------------------------------------------------------------------------- */

int rtc_get (struct rtc_time *tmp)
{
	int rel = 0;
	uchar buf[YEARS_REG - CONTROL_3_REG + 1];
	uchar ctrl3, sec, min, hour, mday, wday, mon, year;

	/* todo: do we need to guard against rollover during read? */
	i2c_read(CONFIG_SYS_I2C_RTC_ADDR, 0x02, 1, buf, sizeof(buf));

	ctrl3	= buf[CONTROL_3_REG - CONTROL_3_REG];
	sec	= buf[SECONDS_REG - CONTROL_3_REG];
	min	= buf[MINUTES_REG - CONTROL_3_REG];
	hour	= buf[HOURS_REG - CONTROL_3_REG];
	mday	= buf[DAYS_REG - CONTROL_3_REG];
	wday	= buf[WEEKDAYS_REG - CONTROL_3_REG];
	mon	= buf[MONTHS_REG - CONTROL_3_REG];
	year	= buf[YEARS_REG - CONTROL_3_REG];

	debug ( "Get RTC year: %02x mon: %02x mday: %02x wday: %02x "
		"hr: %02x min: %02x sec: %02x\n",
		year, mon, mday, wday, hour, min, sec );
	debug ( "Alarms: wday: %02x day: %02x hour: %02x min: %02x\n",
		rtc_read (0x0d),
		rtc_read (0x0c),
		rtc_read (0x0b),
		rtc_read (0x0a) );

	if (ctrl3 & CONTROL_3_BLF) {
		puts ("### Warning: RTC Low Voltage - date/time not reliable\n");
		rel = -1;
	}

	tmp->tm_sec  = bcd2bin (sec  & 0x7F);
	tmp->tm_min  = bcd2bin (min  & 0x7F);
	/* todo: 12 hour mode */
	tmp->tm_hour = bcd2bin (hour & 0x3F);
	tmp->tm_mday = bcd2bin (mday & 0x3F);
	tmp->tm_mon  = bcd2bin (mon  & 0x1F);
	tmp->tm_year = bcd2bin (year) + 2000;
	tmp->tm_wday = bcd2bin (wday & 0x07);
	tmp->tm_yday = 0;
	tmp->tm_isdst= 0;

	debug ( "Get DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
		tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	return rel;
}

int rtc_set (struct rtc_time *tmp)
{
	debug ( "Set DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
		tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	/* todo: stop/restart time circuits */
	rtc_write (YEARS_REG, bin2bcd(tmp->tm_year % 100));
	rtc_write (MONTHS_REG, bin2bcd(tmp->tm_mon));
	rtc_write (WEEKDAYS_REG, bin2bcd(tmp->tm_wday));
	rtc_write (DAYS_REG, bin2bcd(tmp->tm_mday));
	/* todo: 12 hour mode */
	rtc_write (HOURS_REG, bin2bcd(tmp->tm_hour));
	rtc_write (MINUTES_REG, bin2bcd(tmp->tm_min));
	rtc_write (SECONDS_REG, bin2bcd(tmp->tm_sec));

	return 0;
}

void rtc_reset (void)
{
	rtc_write (0x00, 0x58);
}

/* ------------------------------------------------------------------------- */

#ifdef DEBUG
static uchar rtc_read (uchar reg)
{
	return (i2c_reg_read (CONFIG_SYS_I2C_RTC_ADDR, reg));
}
#endif

static void rtc_write (uchar reg, uchar val)
{
	i2c_reg_write (CONFIG_SYS_I2C_RTC_ADDR, reg, val);
}

static unsigned bcd2bin (uchar n)
{
	return ((((n >> 4) & 0x0F) * 10) + (n & 0x0F));
}

static unsigned char bin2bcd (unsigned int n)
{
	return (((n / 10) << 4) | (n % 10));
}
