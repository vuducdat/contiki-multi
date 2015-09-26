/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 * This is only to test the convergence time of RPL
 * Edited by Nguyen Thanh Long, ETRO, VUB
 *
 */

#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "sys/ctimer.h"
#include "rpl/rpl.h"
//#include "powertrace.h"

#ifdef WITH_COMPOWER
#include "powertrace.h"
#endif
#include <stdio.h>
#include <string.h>

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define UDP_EXAMPLE_ID  190
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define DEBUG DEBUG_NONE
#include "net/uip-debug.h"

#define STOPCONDITION 253
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

static uint8_t stopCond;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
static clock_time_t
get_time_ms(void)
{
	/*Get time in milliseconds
	 * typedef unsigned long clock_time_t;
	 * CLOCK_CONF_SECOND is number of ticks per second; for z1: 128UL
	 * clock_time()*1000UL returns current number of ticks from starting time of the mote
	 * */
	return (clock_time_t)((clock_time()*1000UL)/CLOCK_CONF_SECOND);
}
static void packet_send_priority()
{
	static long seqno;
	struct testmsg {

		uint32_t timestamp;
		uint16_t seqno;
		uint8_t  instance_id;
		uint8_t padding[22];

	} msg;


	rpl_dag_t *dag;
	dag = rpl_get_dag(30, &server_ipaddr);
	msg.instance_id = 30;

	seqno++;
	msg.seqno = seqno;

	if(msg.seqno > STOPCONDITION)
		{
			stopCond = 1;
		}

	msg.timestamp = get_time_ms();
	uip_udp_packet_sendto(client_conn, &msg, sizeof(msg),
			&server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));

	printf("sizeof %u ,%u\n", sizeof(msg), msg.seqno);
}

/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
	uip_ipaddr_t ipaddr;

	uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

	/* The choice of server address determines its 6LoPAN header compression.
	 * (Our address will be compressed Mode 3 since it is derived from our link-local address)
	 * Obviously the choice made here must also be selected in udp-server.c.
	 *
	 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the 6LowPAN protocol preferences,
	 * e.g. set Context 0 to aaaa::.  At present Wireshark copies Context/128 and then overwrites it.
	 * (Setting Context 0 to aaaa::1111:2222:3333:4444 will report a 16 bit compressed address of aaaa::1111:22ff:fe33:xxxx)
	 *
	 * Note the IPCMV6 checksum verification depends on the correct uncompressed addresses.
	 */

#if 0
	/* Mode 1 - 64 bits inline */
	uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
#elif 1
	/* Mode 2 - 16 bits inline */
	uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
	/* Mode 3 - derived from server link-local (MAC) address */
	uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0x0250, 0xc2ff, 0xfea8, 0xcd1a); //redbee-econotag
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
	//  static struct etimer periodic;
	//static struct ctimer backoff_timer;
#if WITH_COMPOWER
	static int print = 0;
#endif

	PROCESS_BEGIN();

	PROCESS_PAUSE();
	static struct etimer et;

	//powertrace_start(CLOCK_SECOND * 1);

	set_global_address();

	PRINTF("UDP client process started\n");

	/* new connection with remote host */
	client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
	if(client_conn == NULL) {
		PRINTF("No UDP connection available, exiting the process!\n");
		PROCESS_EXIT();
	}
	udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

	PRINTF("Created a connection with the server ");
	PRINT6ADDR(&client_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n",
			UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

	while(1)
	{
		etimer_set(&et, CLOCK_SECOND * 60 + random_rand() % (CLOCK_SECOND * 60));

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		if(stopCond == 1)
		{
		   break;
		}

		packet_send_priority();

	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
