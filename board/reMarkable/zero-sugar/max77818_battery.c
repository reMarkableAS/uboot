/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Steinar Bakkemo <steinar.bakkemo@remarkable.com>
 */

#include "max77818.h"
#include "max77818_battery.h"

#include <dm/device.h>
#include <linux/errno.h>
#include <i2c.h>

#define MAX77818_REG_FG_CONFIG						0x1D
#define MAX77818_FG_CONFIG__FGCC_MASK					0x0800
#define MAX77818_FG_CONFIG__FGCC_ENABLED				0x0800
#define MAX77818_FG_CONFIG__FGCC_DISABLED				0x0000

#define MAX77818_REG_FG_REPSOC 0x06

static struct udevice *fgDev = NULL;

int max77818_init_fg_device(void)
{
	int ret;

	struct udevice *bus = max77818_get_bus();
	if (!bus) {
		ret = max77818_init_i2c_bus();
		if (ret) {
			printf("%s: Unable to complete fg device initialization",
			       __func__);
			return ret;
		}
	}

	ret = dm_i2c_probe(bus, MAX77818_FG_I2C_ADDR, 0, &fgDev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
		       __func__, MAX77818_FG_I2C_ADDR, MAX77818_I2C_BUS);
		return ret;
	}

	return 0;
}

int max77818_read_fgcc_state(bool *state)
{
	int ret;
	u16 value;

	if (!fgDev) {
		ret = max77818_init_fg_device();
		if (ret) {
			printf("%s: Unable to read FGCC state\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_i2c_reg_read16(fgDev,
				      MAX77818_REG_FG_CONFIG,
				      &value);
	if (ret) {
		printf("%s: Failed to read current FGCC state\n", __func__);
		return ret;
	}
	value &= MAX77818_FG_CONFIG__FGCC_MASK;
	*state = (value == MAX77818_FG_CONFIG__FGCC_ENABLED);

	return 0;
}

int max77818_set_fgcc_state(bool enabled, bool *restore_state)
{
	int ret;
	u16 value;

	if (!fgDev) {
		ret = max77818_init_fg_device();
		if(ret) {
			printf("%s: Unable to set FGCC state\n",
			       __func__);
			return ret;
		}
	}

	if (restore_state) {
		ret = max77818_read_fgcc_state(restore_state);
		if (ret) {
			printf("%s: Unable to read current FGCC state,"
			       "assuming FGCC should be configured\n",
			       __func__);

			*restore_state = true;
		}
	}

	value = (enabled ? MAX77818_FG_CONFIG__FGCC_ENABLED :
			   MAX77818_FG_CONFIG__FGCC_DISABLED);

	ret = max77818_i2c_reg_write16(fgDev, MAX77818_REG_FG_CONFIG,
				       MAX77818_FG_CONFIG__FGCC_MASK,
				       value);
	if (ret) {
		printf("%s: Failed to write FGCC mode\n", __func__);
		return ret;
	}

	return 0;
}

int max77818_restore_fgcc(bool restore_state)
{
	int ret;

	if (!fgDev) {
		ret = max77818_init_fg_device();
		if(ret) {
			printf("%s: Unable to restore FGCC state\n",
			       __func__);
			return ret;
		}
	}

	if (restore_state) {
		printf("Enabling FGCC mode\n");
		return max77818_set_fgcc_state(true, NULL);
	}
	else {
		printf("Disabling FGCC mode\n");
		return max77818_set_fgcc_state(false, NULL);
	}
}

int max77818_get_battery_capacity(u8 *capacity)
{
	int ret;
	u16 value;

	if (!fgDev) {
		ret = max77818_init_fg_device();
		if (ret)
			return ret;
	}

	ret = max77818_i2c_reg_read16(fgDev,
				      MAX77818_REG_FG_REPSOC,
				      &value);
	if (ret)
		return ret;

	*capacity = (u8)(value >> 8);

	return 0;
}

static int do_max77818_get_battery_capacity(cmd_tbl_t *cmdtp,
					   int flag,
					   int argc,
					   char * const argv[])
{
	int ret;
	u8 capacity;

	ret = max77818_get_battery_capacity(&capacity);
	if (ret)
		return ret;

	printf("Capacity: %u%%\n", capacity);

	return 0;
}

U_BOOT_CMD(
	max77818_get_battery_capacity, 1, 1, do_max77818_get_battery_capacity,
	"Get battery state of charge",
	""
);