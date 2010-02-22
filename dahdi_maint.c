/*
 * Performance and Maintenance utility
 *
 * Written by Russ Meyerriecks <rmeyerriecks@digium.com>
 *
 * Copyright (C) 2001-2010 Digium, Inc.
 *
 * All rights reserved.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>

#include <dahdi/user.h>
#include "dahdi_tools_version.h"

#define DAHDI_CTL "/dev/dahdi/ctl"

extern char *optarg;
extern int optind;

void display_help(char *argv0, int exitcode)
{
	char *c;
	c = strrchr(argv0, '/');
	if (!c)
		c = argv0;
	else
		c++;
	fprintf(stderr, "%s\n\n", dahdi_tools_version);
	fprintf(stderr, "Usage: %s -s <span num> [-h|--help] <-j|--local "\
							"[on|off]>\n", c);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "        -h, --help		display help\n");
	fprintf(stderr, "        -s, --span <span num>	specify the span\n");
	fprintf(stderr, "        -j, --host <on|off>	"\
			"turn on/off local host looopback\n");
	fprintf(stderr, "        -l, --line <on|off>	"\
			"turn on/off network line looopback\n");
	fprintf(stderr, "        -p, --payload <on|off>	"\
			"turn on/off network payload looopback\n");
	fprintf(stderr, "        -i, --insert <fas|multi|crc|cas|prbs|bipolar>"\
			"  insert an error of a specific type\n");
	fprintf(stderr, "        -r, --reset		"\
			"reset the error counters\n\n");
	fprintf(stderr, "Examples: \n");
	fprintf(stderr, "Enable local host loopback(virtual loopback plug)\n");
	fprintf(stderr, "	dahdi_maint -s 1 --local on\n");
	fprintf(stderr, "Disable local host loopback(virtual loopback plug)\n");
	fprintf(stderr, "	dahdi_maint -s 1 --local off\n\n");

	exit(exitcode);
}

int main(int argc, char *argv[])
{
	static int ctl = -1;
	int res;

	int localhostloopback = 0;
	char *jarg = NULL;
	int networklineloopback = 0;
	char *larg = NULL;
	int networkpayloadloopback = 0;
	char *parg = NULL;
	int sflag = 0;
	int span = 1;
	int iflag = 0;
	char *iarg = NULL;
	int gflag = 0;
	int c;
	int rflag = 0;

	struct dahdi_maintinfo m;
	struct dahdi_spaninfo s;

	static struct option long_options[] = {
		{"help",	no_argument,	   0, 'h'},
		{"host",	required_argument, 0, 'j'},
		{"line",	required_argument, 0, 'l'},
		{"payload", 	required_argument, 0, 'p'},
		{"span",	required_argument, 0, 's'},
		{"insert",	required_argument, 0, 'i'},
		{"reset",	no_argument, 	   0, 'r'},
		{0, 0, 0, 0}
	};
	int option_index = 0;

	if (argc < 2) { /* no options */
		display_help(argv[0], 1);
	}

	while ((c = getopt_long(argc, argv, "hj:l:p:s:i:g:r",
				long_options, &option_index)) != -1) {
			switch (c) {
			case 'h': /* local host loopback */
				display_help(argv[0], 0);
				break;
			case 'j': /* local host loopback */
				jarg = optarg;
				localhostloopback = 1;
				break;
			case 'l': /* network line loopback */
				larg = optarg;
				networklineloopback = 1;
				break;
			case 'p': /* network payload loopback */
				parg = optarg;
				networkpayloadloopback = 1;
				break;
			case 's': /* specify a span */
				span = atoi(optarg);
				sflag = 1;
				break;
			case 'i': /* insert an error */
				iarg = optarg;
				iflag = 1;
				break;
			case 'g': /* generate psuedo random sequence */
				gflag = 1;
				break;
			case 'r': /* reset the error counters */
				rflag = 1;
				break;
			}
	}

	ctl = open(DAHDI_CTL, O_RDWR);
	if (ctl < 0) {
		fprintf(stderr, "Unable to open %s\n", DAHDI_CTL);
		return -1;
	}

	if (!(localhostloopback || networklineloopback || networkpayloadloopback 
				|| iflag || gflag || rflag)) {
		s.spanno = span;
		res = ioctl(ctl, DAHDI_SPANSTAT, &s);
		printf("Span %d:\n", span);
		printf(">FEC : %d:\n", s.count.fe);
		printf(">CEC : %d:\n", s.count.crc4);
		printf(">CVC : %d:\n", s.count.cv);
		printf(">EBC : %d:\n", s.count.ebit);
		printf(">BEC : %d:\n", s.count.be);
		printf(">PRBS: %d:\n", s.count.prbs);
		printf(">GES : %d:\n", s.count.errsec);

		return 0;
	}

	m.spanno = span;

	if (localhostloopback) {
		if (!strcasecmp(jarg, "on")) {
			printf("Span %d: local host loopback ON\n", span);
			m.command = DAHDI_MAINT_LOCALLOOP;
		} else if (!strcasecmp(jarg, "off")) {
			printf("Span %d: local host loopback OFF\n", span);
			m.command = DAHDI_MAINT_NONE;
		} else {
			display_help(argv[0], 1);
		}

		res = ioctl(ctl, DAHDI_MAINT, &m);
	}

	if (networklineloopback) {
		if (!strcasecmp(larg, "on")) {
			printf("Span %d: network line loopback ON\n", span);
			m.command = DAHDI_MAINT_NETWORKLINELOOP;
		} else if (!strcasecmp(larg, "off")) {
			printf("Span %d: network line loopback OFF\n", span);
			m.command = DAHDI_MAINT_NONE;
		} else {
			display_help(argv[0], 1);
		}
		res = ioctl(ctl, DAHDI_MAINT, &m);
	}

	if (networkpayloadloopback) {
		if (!strcasecmp(parg, "on")) {
			printf("Span %d: network payload loopback ON\n", span);
			m.command = DAHDI_MAINT_NETWORKPAYLOADLOOP;
		} else if (!strcasecmp(parg, "off")) {
			printf("Span %d: network payload loopback OFF\n", span);
			m.command = DAHDI_MAINT_NONE;
		} else {
			display_help(argv[0], 1);
		}
		res = ioctl(ctl, DAHDI_MAINT, &m);
	}

	if (iflag) {
		if (!strcasecmp(iarg, "fas")) {
			m.command = DAHDI_MAINT_FAS_DEFECT;
			printf("Inserting a single FAS defect\n");
		} else if (!strcasecmp(iarg, "multi")) {
			m.command = DAHDI_MAINT_MULTI_DEFECT;
			printf("Inserting a single multiframe defect\n");
		} else if (!strcasecmp(iarg, "crc")) {
			m.command = DAHDI_MAINT_CRC_DEFECT;
			printf("Inserting a single CRC defect\n");
		} else if (!strcasecmp(iarg, "cas")) {
			m.command = DAHDI_MAINT_CAS_DEFECT;
			printf("Inserting a single CAS defect\n");
		} else if (!strcasecmp(iarg, "prbs")) {
			m.command = DAHDI_MAINT_PRBS_DEFECT;
			printf("Inserting a single PRBS defect\n");
		} else if (!strcasecmp(iarg, "bipolar")) {
			m.command = DAHDI_MAINT_BIPOLAR_DEFECT;
			printf("Inserting a single bipolar defect\n");
		} else {
			display_help(argv[0], 1);
		}
		res = ioctl(ctl, DAHDI_MAINT, &m);
	}

	if (gflag) {
		printf("Enabled the Pseudo-Random Binary Sequence Generation"\
			" and Monitor\n");
		m.command = DAHDI_MAINT_PRBS;
		res = ioctl(ctl, DAHDI_MAINT, &m);
	}

	if (rflag) {
		printf("Resetting error counters for span %d\n", span);
		m.command = DAHDI_RESET_COUNTERS;
		res = ioctl(ctl, DAHDI_MAINT, &m);
	}

	return 0;
}
