/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * nf_router.c - route packets based on the provided config.
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_arp.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "uthash.h"

#define NF_TAG "router"
#define SPEED_TESTER_BIT 7

/* router information */
int nf_count;
static uint32_t nf_table_size = 10;
char * cfg_filename;
struct forward_nf *fwd_nf;

struct forward_nf {
        struct ci_hdr key;
        uint8_t dest;
	UT_hash_handle hh;
};

struct forward_nf *ci_norms = NULL;

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* number of package between each print */
static uint32_t print_delay = 1000000;


/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- <router_config> -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c = 0;

        while ((c = getopt(argc, argv, "f:p:n:")) != -1) {
                switch (c) {
                case 'f':
                        cfg_filename = strdup(optarg);
                        break;
                case 'p':
                        //print_delay = strtoul(optarg, NULL, 10);
                        nf_table_size = strtoul(optarg, NULL, 10);
			break;
                case 'n':
			nf_table_size = strtoul(optarg, NULL, 10);
			break;
		case '?':
                        usage(progname);
                        if (optopt == 'd')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (optopt == 'p')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }

        return optind;
}

/*
 * This function parses the forward config. It takes the filename 
 * and fills up the forward nf array. This includes the ip and dest 
 * address of the onvm_nf
 */
static int
parse_router_config(void) {
        int ret, temp, i;
        char ci[32];
        FILE * cfg;

        cfg  = fopen(cfg_filename, "r");
        if (cfg == NULL) {
                rte_exit(EXIT_FAILURE, "Error openning server \'%s\' config\n", cfg_filename);
        }
        ret = fscanf(cfg, "%*s %d", &temp);
        if (temp <= 0) {
                rte_exit(EXIT_FAILURE, "Error parsing config, need at least one forward NF configuration\n");
        }
	uint8_t j=0, k=0;
        nf_count = nf_table_size;
        for (i = 0; i < nf_count; i++) {
                /*ret = fscanf(cfg, "%s %d", ci, &temp);
                if (ret != 2) {
                        rte_exit(EXIT_FAILURE, "Invalid backend config structure\n");
                }*/
		sprintf(ci, "1.1.1.%" SCNu8".%" SCNu8, j, k);
		printf("%s\n",ci);
		j++;
		if (j==0) k++;
		temp = 2;
        	fwd_nf = (struct forward_nf *)rte_malloc("router fwd_nf info", sizeof(struct forward_nf), 0);
        	if (fwd_nf == NULL) {
                	rte_exit(EXIT_FAILURE, "Malloc failed, can't allocate forward_nf array\n");
        	}
                ret = onvm_pkt_parse_ci(ci, &(fwd_nf->key));
                if (ret < 0) {
                        rte_exit(EXIT_FAILURE, "Error parsing config IP address #%d\n", i);
                }

                if (temp < 0) {
                        rte_exit(EXIT_FAILURE, "Error parsing config dest #%d\n", i);
                }
                fwd_nf->dest = temp;
		HASH_ADD(hh, ci_norms, key, sizeof(struct ci_hdr), fwd_nf);
        }

        fclose(cfg);
        printf("\nDest config (%d):\n",nf_count);
	struct forward_nf *s;
        for (s=ci_norms; s!=NULL; s=s->hh.next) {
	  onvm_pkt_print_ci(&(s->key));
          printf(" %d\n", s->dest);
        }

        return ret;
}


/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf* pkt) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        static uint64_t pkt_process = 0;
        struct ipv4_hdr* ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("N°   : %"PRIu64"\n", pkt_process);
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found\n");
        }
}

/*
static int compare_ci(struct ci_hdr c1, struct ci_hdr c2) {
  if (c1.sender == c2.sender && c1.recipient == c2.recipient && c1.subject == c2.subject && c1.attributes == c2.attributes && c1.tp == c2.tp) {
    return 1;
  } else {
    return 0;
  }
}*/

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) {
        static uint32_t counter = 0;
        struct ci_hdr* ci;
       

        ci = onvm_pkt_ci_hdr(pkt);

        /* If the packet doesn't have an IP header check if its an ARP, if so fwd it to the matched NF*/
        if (ci == NULL) {
	  /*ci_hdr = onvm_pkt_ci_hdr(pkt);
                for (i = 0; i < nf_count; i++) {
                  if (ci_hdr == fwd_nf[i].) {
                     meta->destination = fwd_nf[i].dest;
                     meta->action = ONVM_NF_ACTION_TONF;
                     return 0;
                  }
		  }*/
		printf("No CI hdr found\n");
                meta->action = ONVM_NF_ACTION_DROP;
                meta->destination = 0;
                return 0;
		}

        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

	struct forward_nf *p;
	HASH_FIND(hh, ci_norms, ci, sizeof(struct ci_hdr), p);
	if (p) {
		meta->destination = p->dest;
                meta->action = ONVM_NF_ACTION_TONF;
		meta->flags = ONVM_SET_BIT(0, SPEED_TESTER_BIT);
                return 0;
        }

	printf("No match found\n");
        meta->action = ONVM_NF_ACTION_DROP;
        meta->destination = 0;

        return 0;
}


int main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }
        parse_router_config();

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending\n");
        return 0;
}
