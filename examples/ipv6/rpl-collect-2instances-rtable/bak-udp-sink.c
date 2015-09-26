/*
 ex * Redistribution and use in source and binary forms, with or without
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
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/uip.h"
#include "net/rpl/rpl.h"

#include "net/netstack.h"
#include "dev/button-sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG DEBUG_NONE
#include "net/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define STOP_CONDITIONS 253
#define MAIN_STOPCONITIONS 255 * UIP_CONF_MAX_ROUTES

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define INTERVAL 30
#define RANDWAIT  2
static int send_active = 1;

static struct uip_udp_conn *server_conn;

typedef struct sink_statistics {
	//uint8_t nodeID;  //can be calculated by position of the array
	uint8_t reqID;		// request ID
	uint32_t sinklastTX;// time of the last request that has been sent at the sink (ms)
	uint8_t lastResID;	// ID of the last response that has been received at the sink
	uint8_t numRes;			// number of responses received at the sink
	uint16_t senderMinTX;// mininmum RX time of the response from the sender to the sink
	uint16_t senderMaxTX;// maximum RX time of the response from the sender to the sink
	uint16_t senderAvgTX;// total RX time of the response from the sender to the sink
	uint16_t minRTT;		// minimum RTT value
	uint16_t maxRTT;		// maximum RTT value
	uint16_t avgRTT;		// total RTT value
} sink_statistics_t;

static sink_statistics_t nodeList[UIP_CONF_MAX_ROUTES];
static uint8_t stopCond;
/*--------------------------------------------------------*/
PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
clock_time_t get_time_ms(void) {
	/*Get time in milliseconds
	 * typedef unsigned long clock_time_t;
	 * CLOCK_CONF_SECOND is number of ticks per second; for z1: 128UL
	 * clock_time()*1000UL returns current number of ticks from starting time of the mote
	 * */
	return (clock_time_t) ((clock_time() * 1000UL) / CLOCK_CONF_SECOND);
}

/*
 *
 * server_send info function
 */
static void server_send(uip_ipaddr_t *client_ipaddr, uint8_t instance_id) {

	/*
	 *
	 * Server send to a particular client per instance
	 */

	struct {
		// information from the sink
		uint8_t reqID;
		uint8_t instance_id;
		uint32_t sinklastTX;
		uint8_t padding[24];

	} msg; //sizeof(30)

	int nodeID = (client_ipaddr->u8[sizeof(uip_ipaddr_t) - 2] << 8)
																															+ client_ipaddr->u8[sizeof(uip_ipaddr_t) - 1];
	int nodePos = nodeID - 2; // This is only correct for Cooja (sink is 1 and senders are 2,3,etc.)
	msg.instance_id = instance_id;
	msg.reqID = nodeList[nodePos].reqID;
	msg.sinklastTX = nodeList[nodePos].sinklastTX;
	//printf("sizeof %u\n", sizeof(msg));
	uip_udp_packet_sendto(server_conn, &msg, sizeof(msg), client_ipaddr,
			UIP_HTONS(UDP_CLIENT_PORT));
}
/*
---------------------------------------------------------------------------
static void tcpip_handler(void) {
	uint8_t *appdata;
	rimeaddr_t sender;
	uint8_t seqno;
	uint8_t hops;
	uint8_t resID, numReq;
	uint8_t instance_id;
	uint32_t senderlastRX, senderlastTX;

	int payload_len;
	static uip_ds6_route_t *r;


	if (uip_newdata()) {
		sender.u8[0] = UIP_IP_BUF->srcipaddr.u8[15];
		sender.u8[1] = UIP_IP_BUF->srcipaddr.u8[14];

		appdata = (uint8_t *) uip_appdata; // appdata
		payload_len = uip_datalen() - 12; //payload
		instance_id = *appdata; // instance_position
		appdata++; //increament 1byte
		resID =*appdata; //Request appdata
		appdata++; // pos of numReq
		numReq = *appdata;
		appdata++; // increment 1byte
		seqno = *appdata; //seqno
		appdata+=4; // pos of senderLastRx
		memcpy(&senderlastRX, appdata, 4);
		appdata += 4; // pos of senderlastTX
		memcpy(&senderlastTX, appdata, 4);
		hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;

		// Write out analysed information
		uint32_t time = get_time_ms(); //ms
		int i;
		unsigned int nodeID = sender.u8[0] + (sender.u8[1] << 8);

		unsigned int nodePos = nodeID - 2;
		// Store the old lastResID
		uint8_t lastResIDOld = nodeList[nodePos].lastResID;
		nodeList[nodePos].lastResID = resID;
		// Store the old numRes
		uint8_t numResOld = nodeList[nodePos].numRes;
		nodeList[nodePos].numRes++;
		// Calculating senderTX
		if (time < senderlastTX)
		{
			printf ("Big problem here <= 0\n");
		}
		uint16_t senderTX = (uint16_t)(time - senderlastTX) & 0xffff;
		nodeList[nodePos].senderAvgTX = (nodeList[nodePos].senderAvgTX*numResOld + senderTX) / nodeList[nodePos].numRes;
		if (nodeList[nodePos].senderMinTX > senderTX){
			nodeList[nodePos].senderMinTX = senderTX;
		}
		if (nodeList[nodePos].senderMaxTX < senderTX){
			nodeList[nodePos].senderMaxTX = senderTX;
		}
		// Calculating RTT
		// TX Time of this responded request - it may be different to lats TX request because of delay on the network
		uint8_t reqDiff = nodeList[nodePos].reqID - resID;
		uint32_t requestTXTime;
		if (reqDiff > 0){
			requestTXTime = nodeList[nodePos].sinklastTX - (uint32_t)reqDiff*RANDWAIT*UIP_DS6_ROUTE_NB*1000;
		}else{
			requestTXTime = nodeList[nodePos].sinklastTX;
		}
		uint16_t RTT = (uint16_t)(time - requestTXTime) & 0xffff;
		nodeList[nodePos].avgRTT = (nodeList[nodePos].avgRTT*numResOld + RTT) / nodeList[nodePos].numRes;
		if (nodeList[nodePos].minRTT > RTT){
			nodeList[nodePos].minRTT = RTT;
		}
		if (nodeList[nodePos].maxRTT < RTT){
			nodeList[nodePos].maxRTT = RTT;
		}

		// Calculating Offline time
		uint8_t resDiff = resID - lastResIDOld;
		uint32_t offTime; // in ms
		if (resDiff > 1){ // lost request/response
			offTime = (uint32_t)(resDiff-1)*UIP_DS6_ROUTE_NB*RANDWAIT*1000L;
		} else {
			offTime = 0;
		}

		RX:
 * NodeID ResID ResCount ReqCount (at sender)
 * TXTime SenderRXTime SinkRXTime
 * RTT AVG MIN MAX
 * SenderTX AVG MIN MAX

		//		printf("GEN: %u %u %u %u\n", nodeID, resID, nodeList[nodePos].numRes, numReq);
		//		printf("TIM: %lu %lu %lu\n", requestTXTime, senderlastTX, time);
		//		printf("RTT: %u %u %u %u\n", RTT, nodeList[nodePos].avgRTT,
		//				nodeList[nodePos].avgRTT - nodeList[nodePos].minRTT,
		//				nodeList[nodePos].maxRTT - nodeList[nodePos].avgRTT);
		//		printf("STX: %u %u %u %u\n", senderTX, nodeList[nodePos].senderAvgTX,
		//				nodeList[nodePos].senderAvgTX - nodeList[nodePos].senderMinTX,
		//				nodeList[nodePos].senderMaxTX - nodeList[nodePos].senderAvgTX);
		//		printf("LOS: %lu\n", offTime);
		printf("GEN %u %u %u %u %u %u "
				//"TIM: %lu %lu %lu "
				"RTT %u %u %u %u "
				"STX %u %u %u %u \n",
				//"LOS %lu\n",
				nodeID, resID, nodeList[nodePos].numRes, numReq,instance_id, hops,
				//requestTXTime, senderlastTX, time,
				RTT, nodeList[nodePos].avgRTT, nodeList[nodePos].avgRTT - nodeList[nodePos].minRTT, nodeList[nodePos].maxRTT - nodeList[nodePos].avgRTT,
				senderTX, nodeList[nodePos].senderAvgTX, nodeList[nodePos].senderAvgTX - nodeList[nodePos].senderMinTX, nodeList[nodePos].senderMaxTX - nodeList[nodePos].senderAvgTX
		offTime);
	}

}
 */

static void tcpip_handler(void) {


	static long seqno;
	uint8_t *appdata;
	rimeaddr_t sender;
	uint8_t hops;

	struct testmsg {
		uint32_t timestamp;
		uint16_t seqno;
		uint8_t padding[24];
	};

	struct testmsg msg;


	int payload_len;

	if (uip_newdata()) {

		appdata = (uint8_t *) uip_appdata; // appdata
		payload_len = uip_datalen() - 12;
		uint32_t time = get_time_ms();
		hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;

		//memcpy(&msg, packetbuf_dataptr(), sizeof(msg));

		memcpy(&msg.timestamp, appdata, 4);
		//appdata++;
		//memcpy(&msg.seqno, appdata, 2);
		msg.seqno = *appdata;

        seqno++;
		sender.u8[0] = UIP_IP_BUF->srcipaddr.u8[15];
		sender.u8[1] = UIP_IP_BUF->srcipaddr.u8[14];
		unsigned int nodeID = sender.u8[0] + (sender.u8[1] << 8);
		if(msg.timestamp > time)
		{
			printf("big problem here \n");
		}
		uint32_t reciever_latency = time - msg.timestamp ;
		uint32_t send_time = msg.timestamp;


		printf("sizeof %u ", sizeof(msg));
		printf( "%u, %lu, %lu, %lu  %u  %u %u\n", nodeID, reciever_latency, send_time,time, hops, msg.seqno , seqno);

	}




}


/*---------------------------------------------------------------------------*/
		PROCESS_THREAD(udp_server_process, ev, data) {
			uip_ipaddr_t ipaddr;
			struct uip_ds6_addr *root_if;

			static struct etimer periodic, wait;

			PROCESS_BEGIN();

			PROCESS_PAUSE();



			PRINTF("UDP server started\n");

#if UIP_CONF_ROUTER
			/* The choice of server address determines its 6LoPAN header compression.
			 * Obviously the choice made here must also be selected in udp-client.c.
			 *
			 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the 6LowPAN protocol preferences,
			 * e.g. set Context 0 to aaaa::.  At present Wireshark copies Context/128 and then overwrites it.
			 * (Setting Context 0 to aaaa::1111:2222:3333:4444 will report a 16 bit compressed address of aaaa::1111:22ff:fe33:xxxx)
			 * Note Wireshark's IPCMV6 checksum verification depends on the correct uncompressed addresses.
			 */

#if 0
			/* Mode 1 - 64 bits inline */
			uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
#elif 1
			/* Mode 2 - 16 bits inline */
			uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
			/* Mode 3 - derived from link local (MAC) address */
			uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
			uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
#endif

			uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
			root_if = uip_ds6_addr_lookup(&ipaddr);
			if (root_if != NULL) {
				rpl_dag_t *dag, *dag1;
				dag = rpl_set_root(RPL_DEFAULT_INSTANCE, (uip_ip6addr_t *) &ipaddr);
				dag1 = rpl_set_root(RPL_SECOND_INSTANCE,(uip_ip6addr_t *)&ipaddr);
				uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
				rpl_set_prefix(dag, &ipaddr, 64);
				rpl_set_prefix(dag1, &ipaddr, 64);

				//PRINTF("created a new RPL dag\n");
			} else {
				//PRINTF("failed to create a new RPL DAG\n");
			}
#endif /* UIP_CONF_ROUTER */

			/* The data sink runs with a 100% duty cycle in order to ensure high
	 packet reception rates. */
			NETSTACK_MAC.off(1);

			server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
			udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

			/* Wait until all nodes are in the routing table at the sink*/
			static struct etimer table_check_timer;
			//etimer_set(&table_check_timer, CLOCK_CONF_SECOND / 8);
			static clock_time_t r1, r2;
			static uint8_t i, j;
			uint8_t num_tests;

			/*Starting conditions*/
			i = 0;
			j = 0;
			r1 = get_time_ms();
			num_tests = 1;
			//	printf("Convergence: r1 %u\n", r1);

			static uip_ds6_route_t *r;

			/*while (i < num_tests) {
		PROCESS_WAIT_EVENT()
																								;
		if (ev == PROCESS_EVENT_TIMER && data == &table_check_timer) {
			Time for checking the routing table
			j = uip_ds6_route_num_routes();
			printf("Convergence: num_routes %u\n", j);
			if (j < UIP_CONF_MAX_ROUTES) {
				The routing table is not filled up, check again
				etimer_reset(&table_check_timer);
			} else {
				The routing table is filled up
				r2 = get_time_ms();
				//				printf("Convergence: r1 %u, r2 %u\n", r1, r2);
				printf(
						"Convergence: routing table is OK after %u milliseconds\n",
						(clock_time_t) (r2 - r1));
				i++;
			}
		}
	}*/

			/*
			 * Sends request to every leaf nodes in the network
			 */


			/*PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&table_check_timer));
	etimer_set(&table_check_timer, 120 * CLOCK_SECOND);*/

			etimer_set(&periodic, CLOCK_SECOND * INTERVAL);
			while (1) {


				PROCESS_WAIT_EVENT();
				//PROCESS_YIELD();

				if (ev == tcpip_event) {	//printf("i am here tcpip\n");
					tcpip_handler();
				}

				else if (ev == PROCESS_EVENT_TIMER && data == &periodic) {
					//printf("reached here _ev == PROCESS\n");
					r = uip_ds6_route_head();
					if (data == &periodic) {
						etimer_reset(&periodic);
						etimer_set(&wait,RANDWAIT + random_rand() % (CLOCK_SECOND * RANDWAIT));
					}
					while (r != NULL) {

						//wait until timer expired
						PROCESS_WAIT_UNTIL(etimer_expired(&wait));
						//printf("data == &wait\n");

						uint32_t time = get_time_ms();

						//Map route address to Cooja Node ID
						int nodeID = (r->ipaddr.u8[sizeof(uip_ipaddr_t) - 2] << 8)
																																			+ r->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
						int nodePos = nodeID - 2; // This is only correct for Cooja (sink is 1 and others are 2,3,etc.)

						/*
						 * stop condition true for all routes in the routing table
						 */

						if (stopCond == UIP_CONF_MAX_ROUTES) {
							PRINTF("Stop sending requests for all, stopCond=%u\n", stopCond);
							break;
						}
						//Check if it's the first and set a default value
						if (nodeList[nodePos].reqID == 0) {
							nodeList[nodePos].reqID = 1;
							nodeList[nodePos].sinklastTX = time;
							nodeList[nodePos].senderMinTX = 65535;
							nodeList[nodePos].minRTT = 65535;
						}
						else
						{
							nodeList[nodePos].reqID++;
							nodeList[nodePos].sinklastTX = get_time_ms();

						}
						if ((nodeList[nodePos].reqID) > STOP_CONDITIONS){
							PRINTF("Stop sending request for node %u\n", nodeID);
							stopCond++;
							printf("stopCond %u \ns", stopCond);
							//r = uip_ds6_route_next(r);
							//	server_send(&r->ipaddr, 30);
							//etimer_reset(&wait);

						}
						else {
							// Adding information to a node already in the statistic list
							//					printf("Updating an old node\n");

							printf("addr ");
							PRINT6ADDR(&r->ipaddr);
							printf("\n");
							if(r->state.rpl_id == 30)
							{
								server_send(&r->ipaddr, 30);
							}
							else
							{
								server_send(&r->ipaddr, 31);

							}

							r = uip_ds6_route_next(r);

						}
						etimer_reset(&wait);


					}
				}


			}
			PROCESS_END();
		}

		/*	while(1)
			{
				 PROCESS_YIELD();
				tcpip_handler();
			}

			PROCESS_END();

		}
		---------------------------------------------------------------------------*/
