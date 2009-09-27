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
#include <arpa/inet.h>
#include "debug.h"
#include "hexfile.h"
#include "mpp_funcs.h"
#include "pic_loader.h"
#include "astribank_usb.h"

#define	DBG_MASK	0x80
#define	MAX_HEX_LINES	10000

static char	*progname;

static void usage()
{
	fprintf(stderr, "Usage: %s [options...] -D {/proc/bus/usb|/dev/bus/usb}/<bus>/<dev> hexfile...\n", progname);
	fprintf(stderr, "\tOptions: {-F|-p}\n");
	fprintf(stderr, "\t\t[-F]               # Load FPGA firmware\n");
	fprintf(stderr, "\t\t[-p]               # Load PIC firmware\n");
	fprintf(stderr, "\t\t[-v]               # Increase verbosity\n");
	fprintf(stderr, "\t\t[-d mask]          # Debug mask (0xFF for everything)\n");
	exit(1);
}

int handle_hexline(struct astribank_device *astribank, struct hexline *hexline)
{
	uint16_t	len;
	uint16_t	offset_dummy;
	uint8_t		*data;
	int		ret;

	assert(hexline);
	assert(astribank);
	if(hexline->d.content.header.tt != TT_DATA) {
		DBG("Non data record type = %d\n", hexline->d.content.header.tt);
		return 0;
	}
	len = hexline->d.content.header.ll;
	offset_dummy = hexline->d.content.header.offset;
	data = hexline->d.content.tt_data.data;
	if((ret = mpp_send_seg(astribank, data, offset_dummy, len)) < 0) {
		ERR("Failed FPGA send line: %d\n", ret);
		return -EINVAL;
	}
	return 0;
}

static int load_fpga(struct astribank_device *astribank, const char *hexfile)
{
	struct hexdata		*hexdata = NULL;
	int			finished = 0;
	int			ret;
	int			i;

	if((hexdata  = parse_hexfile(hexfile, MAX_HEX_LINES)) == NULL) {
		perror(hexfile);
		return -errno;
	}
	INFO("Loading FPGA: %s (version %s)\n", hexdata->fname, hexdata->version_info);
#if 0
	FILE		*fp;
	if((fp = fopen("fpga_dump_new.txt", "w")) == NULL) {
		perror("dump");
		exit(1);
	}
#endif
	if((ret = mpp_send_start(astribank, DEST_FPGA)) < 0) {
		ERR("Failed FPGA send start: %d\n", ret);
		return ret;
	}
	for(i = 0; i < hexdata->maxlines; i++) {
		struct hexline	*hexline = hexdata->lines[i];

		if(!hexline)
			break;
		if(finished) {
			ERR("Extra data after End Of Data Record (line %d)\n", i);
			return 0;
		}
		if(hexline->d.content.header.tt == TT_EOF) {
			DBG("End of data\n");
			finished = 1;
			continue;
		}
		if((ret = handle_hexline(astribank, hexline)) < 0) {
			ERR("Failed FPGA sending in lineno %d (ret=%d)\n", i, ret);;
			return ret;
		}
	}
	if((ret = mpp_send_end(astribank)) < 0) {
		ERR("Failed FPGA send end: %d\n", ret);
		return ret;
	}
#if 0
	fclose(fp);
#endif
	free_hexdata(hexdata);
	DBG("FPGA firmware loaded successfully\n");
	return 0;
}

int main(int argc, char *argv[])
{
	char			*devpath = NULL;
	struct astribank_device *astribank;
	int			opt_fpga = 0;
	int			opt_pic = 0;
	const char		options[] = "vd:D:Fp";
	int			iface_num;
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
			case 'F':
				opt_fpga = 1;
				break;
			case 'p':
				opt_pic = 1;
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
				return 1;
		}
	}
	if((opt_fpga ^ opt_pic) == 0) {
		ERR("The -F and -p options are mutually exclusive.\n");
		usage();
	}
	iface_num = (opt_fpga) ? 1 : 0;
	if(opt_fpga) {
		if(optind != argc - 1) {
			ERR("The -F option requires exacly one hexfile argument\n");
			usage();
		}
	}
	if(!devpath) {
		ERR("Missing device path.\n");
		usage();
	}
	if((astribank = astribank_open(devpath, iface_num)) == NULL) {
		ERR("Opening astribank failed\n");
		return 1;
	}
	show_astribank_info(astribank);
	if(opt_fpga) {
		if(load_fpga(astribank, argv[optind]) < 0) {
			ERR("Loading FPGA firmware failed\n");
			return 1;
		}
	} else if(opt_pic) {
		if((ret = load_pic(astribank, argc - optind, argv + optind)) < 0) {
			ERR("Loading PIC's failed\n");
			return 1;
		}
	}
	astribank_close(astribank, 0);
	return 0;
}
