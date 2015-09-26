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
 *
 */

#include "contiki.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "net/rpl/rpl.h"
#include "dev/serial-line.h"
#include "sys/energest.h"
#if CONTIKI_TARGET_Z1
#include "dev/uart0.h"
#else
#include "dev/uart1.h"
#endif

#include <stdio.h>
#include <string.h>

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#define STOPCONDITION 253
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

typedef struct sender_statistics {
	uint8_t nodeID;
	uint8_t resID;			// response ID ??
	uint32_t senderlastTX;// time of the last response that has been sent from the sender (ms)
	uint32_t senderlastRX;// time of the last request that has been received from the sink (ms)
	uint8_t numReq;			// number of request received at the sender
	uint16_t sinkMinTX;	// minimum TX time of the request from the sink to the sender
	uint16_t sinkMaxTX;	// maximum TX time of the request from the sink to the sender
	uint16_t sinkTotTX;	// total TX time of the request from the sink to the sender
} sender_statistics_t;

static sender_statistics_t nodeInf;
static uint8_t stopCond;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static clock_time_t get_time_ms(void) {
	/*Get time in milliseconds
	 * typedef unsigned long clock_time_t;
	 * CLOCK_CONF_SECOND is number of ticks per second; for z1: 128UL
	 * clock_time()*1000UL returns current number of ticks from starting time of the mote
	 * */
	return (clock_time_t) ((clock_time() * 1000UL) / CLOCK_CONF_SECOND);
}
void
powertrace_print(char *str)
{
  static uint32_t last_cpu, last_lpm, last_transmit, last_listen;
  static uint32_t last_idle_transmit, last_idle_listen;

  uint32_t cpu, lpm, transmit, listen;
  uint32_t all_cpu, all_lpm, all_transmit, all_listen;
  uint32_t idle_transmit, idle_listen;
  uint32_t all_idle_transmit, all_idle_listen;

  static uint32_t seqno;

  uint32_t time, all_time, radio, all_radio;

  struct powertrace_sniff_stats *s;

  energest_flush();

  all_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  all_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  all_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  all_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  all_idle_transmit = compower_idle_activity.transmit;
  all_idle_listen = compower_idle_activity.listen;

  cpu = all_cpu - last_cpu;
  lpm = all_lpm - last_lpm;
  transmit = all_transmit - last_transmit;
  listen = all_listen - last_listen;
  idle_transmit = compower_idle_activity.transmit - last_idle_transmit;
  idle_listen = compower_idle_activity.listen - last_idle_listen;

  last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  last_idle_listen = compower_idle_activity.listen;
  last_idle_transmit = compower_idle_activity.transmit;

  radio = transmit + listen;
  time = cpu + lpm;
  all_time = all_cpu + all_lpm;
  all_radio = energest_type_time(ENERGEST_TYPE_LISTEN) +
    energest_type_time(ENERGEST_TYPE_TRANSMIT);

  printf("%s %lu P %d.%d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu (radio %d.%02d%% / %d.%02d%% tx %d.%02d%% / %d.%02d%% listen %d.%02d%% / %d.%02d%%)\n",
         str,
         clock_time(), rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], seqno,
         all_cpu, all_lpm, all_transmit, all_listen, all_idle_transmit, all_idle_listen,
         cpu, lpm, transmit, listen, idle_transmit, idle_listen,
         (int)((100L * (all_transmit + all_listen)) / all_time),
         (int)((10000L * (all_transmit + all_listen) / all_time) - (100L * (all_transmit + all_listen) / all_time) * 100),
         (int)((100L * (transmit + listen)) / time),
         (int)((10000L * (transmit + listen) / time) - (100L * (transmit + listen) / time) * 100),
         (int)((100L * all_transmit) / all_time),
         (int)((10000L * all_transmit) / all_time - (100L * all_transmit / all_time) * 100),
         (int)((100L * transmit) / time),
         (int)((10000L * transmit) / time - (100L * transmit / time) * 100),
         (int)((100L * all_listen) / all_time),
         (int)((10000L * all_listen) / all_time - (100L * all_listen / all_time) * 100),
         (int)((100L * listen) / time),
         (int)((10000L * listen) / time - (100L * listen / time) * 100));


  seqno++;
}



/*---------------------------------------------------------------------------*/
static void tcpip_handler(void) {
	uint8_t *appdata;
	uint8_t reqID;
	uint32_t sinklastTX;

	if (uip_newdata()) {
		// Collect data from received payload
		appdata = (uint8_t *) uip_appdata;
		//		printf ("sizeof(appdata): %u\n", uip_datalen());
		reqID = *appdata; // Check for next lines
		appdata += 2; // 01 for alignment
		memcpy(&sinklastTX, appdata, 4);

		// Stop when reaching condition
		if (reqID >= STOPCONDITION) {
			stopCond = 1;

			printf("I am stopping \n");
			return;
		}

		//Get the time for receiving response
		uint32_t time = get_time_ms();

		// Updating statistics information for the node
		//		PRINTF("Updating statistics information\n");

		if (nodeInf.nodeID == 0) { // get nodeId for the first time
			uip_ds6_addr_t *addr;
			addr = uip_ds6_get_link_local(-1);
			nodeInf.nodeID = (addr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2] << 8)
					+ addr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
		}

		uint8_t oldresID = nodeInf.resID; // Store the resID
		nodeInf.senderlastRX = time;
		/*if (nodeInf.resID==0){ // first time received a request

		 } else { // received at least one request

		 }*/
		nodeInf.resID = reqID; // Update resID to be identical with its request's ID
		//	nodeInf.senderlastTX = time;
		nodeInf.numReq++;
		uint16_t sinkTX = (uint16_t) (time - sinklastTX) & 0xffff;
		if (nodeInf.sinkMinTX > sinkTX) {
			nodeInf.sinkMinTX = sinkTX;
		}
		if (nodeInf.sinkMaxTX < sinkTX) {
			nodeInf.sinkMinTX = sinkTX;
		}

		/*PRINTF("RX_Time:%lu nodeID=%u resID=%u sinkTX=%u\n",
		 time, nodeInf.nodeID, nodeInf.resID, sinkTX);*/

		printf("RX: Time : %lu sinklastTX: %lu reqID: %u \n", time, sinklastTX,
				reqID);

		//client_send();

	}
}
/*---------------------------------------------------------------------------*/
void client_send(void) {
	static uint8_t seqno;
	struct {
		uint8_t seqno;
		uint8_t resID;
		uint8_t numReq;
		uint8_t for_alignment;
		uint32_t senderlastRX;
		uint32_t senderlastTX;
		uint8_t padding[18];

	} msg;

	if (client_conn == NULL) {
		/* Not setup yet */
		return;
	}
	memset(&msg, 0, sizeof(msg));
	seqno++;
	if (seqno == 0) {
		/* Wrap to 128 to identify restarts */
		seqno = 128;
	}
	msg.seqno = seqno;
	msg.resID = nodeInf.resID;
	msg.numReq = nodeInf.numReq;

	//Get the time before sending response
	uint32_t time = get_time_ms();
	msg.senderlastRX = nodeInf.senderlastRX;
	msg.senderlastTX = time;
	nodeInf.senderlastTX = time;
	printf("TX-RX: %u\n", time - msg.senderlastRX);
	printf("TX: Time : %lu resID: %u numReq: %u sizeof %u\n", time, msg.resID,
			msg.numReq, sizeof(msg));
	printf("seqno %u\n", seqno);

	uip_udp_packet_sendto(client_conn, &msg, sizeof(msg), &server_ipaddr,
			UIP_HTONS(UDP_SERVER_PORT));
}
static void set_global_address(void) {
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
PROCESS_THREAD(udp_client_process, ev, data) {
	PROCESS_BEGIN()
	;

	PROCESS_PAUSE()
	;

	set_global_address();

	//	senderStartTime = get_time_ms();
	PRINTF("UDP client process started\n");

	//	print_local_addresses();

	/* new connection with remote host */
	client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
	udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

	//	PRINTF("Created a connection with the server ");
	//	PRINT6ADDR(&client_conn->ripaddr);
	//	PRINTF(" local/remote port %u/%u\n",
	//			UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));
	// Initial value of nodeInf

	// Initial values
	nodeInf.sinkMinTX = 65535;
	stopCond = 0;

	static struct etimer et;

	//etimer_set(&et, CLOCK_SECOND * 1 + random_rand() % (CLOCK_SECOND * 1));

	static struct etimer periodic;
	//etimer_set(&et, 2 * CLOCK_SECOND);

	int i;
	while (1) {
//		PROCESS_YIELD();

		PROCESS_WAIT_EVENT()
		;

		/* Send a packet every 30 seconds. */
		if (etimer_expired(&periodic)) {
			i = 0;
			etimer_set(&periodic, CLOCK_SECOND * 2);
			etimer_set(&et, random_rand() % (CLOCK_SECOND * 2));
		}

		if (stopCond == 1) {
			break;
		}

		//

		if (ev == tcpip_event) {
			tcpip_handler();
		} else if (etimer_expired(&et)) {

			client_send();
		}
	}

PROCESS_END();
}
/*---------------------------------------------------------------------------*/
