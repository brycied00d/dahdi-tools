/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2008, Xorcom
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <arpa/inet.h>
#include "mpp_funcs.h"
#include "debug.h"

#define	DBG_MASK	0x80

static char	*progname;

static void usage()
{
	fprintf(stderr, "Usage: %s [options...] -D {/proc/bus/usb|/dev/bus/usb}/<bus>/<dev>\n", progname);
	fprintf(stderr, "\tOptions: {-n|-r kind}\n");
	fprintf(stderr, "\t\t[-n]               # Renumerate device\n");
	fprintf(stderr, "\t\t[-r kind]          # Reset: kind = {half|full}\n");
	fprintf(stderr, "\t\t[-p port]          # TwinStar: USB port number [0, 1]\n");
	fprintf(stderr, "\t\t[-w (0|1)]         # TwinStar: Watchdog off or on guard\n");
	fprintf(stderr, "\t\t[-v]               # Increase verbosity\n");
	fprintf(stderr, "\t\t[-d mask]          # Debug mask (0xFF for everything)\n");
	exit(1);
}

static int reset_kind(const char *arg)
{
	static const struct {
		const char	*name;
		int		type_code;
	} reset_kinds[] = {
		{ "half",	0 },
		{ "full",	1 },
	};
	int	i;

	for(i = 0; i < sizeof(reset_kinds)/sizeof(reset_kinds[0]); i++) {
		if(strcasecmp(reset_kinds[i].name, arg) == 0)
			return reset_kinds[i].type_code;
	}
	ERR("Uknown reset kind '%s'\n", arg);
	return -1;
}


static int show_hardware(struct astribank_device *astribank)
{
	uint8_t	unit;
	uint8_t	card_status;
	uint8_t	card_type;
	int	ret;
	struct eeprom_table	eeprom_table;
	struct capabilities	capabilities;
	struct extrainfo	extrainfo;

	show_astribank_info(astribank);
	ret = mpp_caps_get(astribank, &eeprom_table, &capabilities, NULL);
	if(ret < 0)
		return ret;
	show_eeprom(&eeprom_table, stdout);
	show_astribank_status(astribank, stdout);
	if(astribank->eeprom_type == EEPROM_TYPE_LARGE) {
		show_capabilities(&capabilities, stdout);
		if(STATUS_FPGA_LOADED(astribank->status)) {
			for(unit = 0; unit < 4; unit++) {
				ret = mpps_card_info(astribank, unit, &card_type, &card_status);
				if(ret < 0)
					return ret;
				printf("CARD %d: type=%x.%x %s\n", unit,
						((card_type >> 4) & 0xF), (card_type & 0xF),
						((card_status & 0x1) ? "PIC" : "NOPIC"));
			}
		}
		ret = mpp_extrainfo_get(astribank, &extrainfo);
		if(ret < 0)
			return ret;
		show_extrainfo(&extrainfo, stdout);
		if(CAP_EXTRA_TWINSTAR(&capabilities)) {
			twinstar_show(astribank, stdout);
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char			*devpath = NULL;
	struct astribank_device *astribank;
	const char		options[] = "vd:D:nr:p:w:";
	int			opt_renumerate = 0;
	char			*opt_port = NULL;
	char			*opt_watchdog = NULL;
	char			*opt_reset = NULL;
	int			tws_portnum;
	int			full_reset;
	int			ret;

	progname = argv[0];
	while (1) {
		int	c;

		c = getopt (argc, argv, options);
		if (c == -1)
			break;

		switch (c) {
			case 'D':
				devpath = optarg;
				break;
			case 'n':
				opt_renumerate++;
				break;
			case 'p':
				opt_port = optarg;
				break;
			case 'w':
				opt_watchdog = optarg;
				break;
			case 'r':
				opt_reset = optarg;
				if((full_reset = reset_kind(opt_reset)) < 0)
					usage();
				break;
			case 'v':
				verbose++;
				break;
			case 'd':
				debug_mask = strtoul(optarg, NULL, 0);
				break;
			case 'h':
			default:
				ERR("Unknown option '%c'\n", c);
				usage();
		}
	}
	if(!devpath) {
		ERR("Missing device path\n");
		usage();
	}
	DBG("Startup %s\n", devpath);
	if((astribank = mpp_init(devpath)) == NULL) {
		ERR("Failed initializing MPP\n");
		return 1;
	}
	show_hardware(astribank);
	if(opt_reset) {
		if((ret = mpp_reset(astribank, full_reset)) < 0) {
			ERR("%s Reseting astribank failed: %d\n",
				(full_reset) ? "Full" : "Half", ret);
		}
	} else if(opt_renumerate) {
		if((ret = mpp_renumerate(astribank)) < 0) {
			ERR("Renumerating astribank failed: %d\n", ret);
		}
	} else if(opt_watchdog) {
		int	watchdogstate = strtoul(opt_watchdog, NULL, 0);

		INFO("TWINSTAR: Setting watchdog %s-guard\n",
			(watchdogstate) ? "on" : "off");
		if((ret = mpp_tws_setwatchdog(astribank, watchdogstate)) < 0) {
			ERR("Failed to set watchdog to %d\n", watchdogstate);
			return 1;
		}
	} else if(opt_port) {
		int	new_portnum = strtoul(opt_port, NULL, 0);
		char	*msg = (new_portnum == tws_portnum)
					? " Same same, never mind..."
					: "";

		INFO("TWINSTAR: Setting portnum to %d.%s\n", new_portnum, msg);
		if((ret = mpp_tws_setportnum(astribank, new_portnum)) < 0) {
			ERR("Failed to set USB portnum to %d\n", new_portnum);
			return 1;
		}
	}
	mpp_exit(astribank);
	return 0;
}
