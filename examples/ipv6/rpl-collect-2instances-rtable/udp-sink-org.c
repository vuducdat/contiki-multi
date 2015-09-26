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
//#include "core/net/uip-ds6-route.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

//#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define SERVER_REPLY          1

static struct uip_udp_conn *server_conn;
const struct sensors_sensor button_sensor;
/*--------------------------------------------------------*/
PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);

//extern uip_ds6_route_t uip_ds6_routing_table[UIP_CONF_MAX_ROUTES];
/*---------------------------------------------------------------------------*/
clock_time_t
get_time_ms(void)
{
	/*Get time in milliseconds
	 * typedef unsigned long clock_time_t;
	 * CLOCK_CONF_SECOND is number of ticks per second; for z1: 128UL
	 * clock_time()*1000UL returns current number of ticks from starting time of the mote
	 * */
	return (clock_time_t)((clock_time()*1000UL)/CLOCK_CONF_SECOND);
}


/*
 * print routing table size
 */
void printRoutingTableSize(uint8_t instance_id)
{
	uip_ds6_route_t *r;
	uint8_t i;
	uint32_t time;
	//printf("Instance_id %u RT size %u Time: %lu\n", instance_id, i, get_time_ms());
	i = 0;

	for(r = uip_ds6_route_head();
			r != NULL;
			r = uip_ds6_route_next(r)) {
		if (r->state.rpl_id == instance_id)
		{ i++;

		   if(i == 10)
		   {
			  printf("Instance_id %u RT size %u Time: %lu\n", instance_id, i, get_time_ms());
		   }

		}

	}
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
	uip_ipaddr_t ipaddr;
	struct uip_ds6_addr *root_if;

	PROCESS_BEGIN();

	PROCESS_PAUSE();

	SENSORS_ACTIVATE(button_sensor);

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
	if(root_if != NULL) {
		rpl_dag_t *dag, *dag1;;
		dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
		dag1 = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
		uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
		rpl_set_prefix(dag, &ipaddr, 64);
		rpl_set_prefix(dag1, &ipaddr, 64);


		printf("Objective fucntion dag_id %u, of %u, \n", dag->dag_id.u8, dag->instance->of->ocp);
		printf("Objective fucntion dag_id %u, of %u, \n", dag1->dag_id.u8, dag1->instance->of->ocp);

		PRINTF("created a new RPL dag\n");
	} else {
		PRINTF("failed to create a new RPL DAG\n");
	}
#endif /* UIP_CONF_ROUTER */


	/* The data sink runs with a 100% duty cycle in order to ensure high
		 packet reception rates. */
	NETSTACK_MAC.off(1);

	server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
	if(server_conn == NULL) {
		PRINTF("No UDP connection available, exiting the process!\n");
		PROCESS_EXIT();
	}
	udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

	PRINTF("Created a server connection with remote address ");
	PRINT6ADDR(&server_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
			UIP_HTONS(server_conn->rport));

	/* Wait until all nodes are in the routing table at the sink*/
	static struct etimer table_check_timer;
	etimer_set(&table_check_timer, CLOCK_CONF_SECOND/8);
	static clock_time_t r1, r2;
	static uint8_t i, j;
	uint8_t num_tests;
	uip_ds6_route_t *r;

	/*Starting conditions*/
	i = 0;
	j = 0;
	r1 = get_time_ms();
	num_tests = 30;
	//	printf("Convergence: r1 %u\n", r1);

	while(i<num_tests) {


		//printf("sizeof %u\n" sizeof(&rtable_ext));
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_TIMER && data == &table_check_timer){
			/*Time for checking the routing table*/
			j = uip_ds6_route_num_routes();
			printf("Convergence: num_routes %u\n", j);

			if (j < UIP_CONF_MAX_ROUTES){
				/*The routing table is not filled up, check again*/
				etimer_reset(&table_check_timer);
			} else {
				/*The routing table is filled up*/
				r2 = get_time_ms();

				//  printf("Convergence: r1 %u, r2 %u\n", r1, r2);
				//printRoutingTableSize(30);
				//printRoutingTableSize(31);
				printf("Convergence: routing table is OK after %u milliseconds\n", (clock_time_t)(r2-r1));
				/*Reset the root, increase the number of tests, and reset the test timer
				 *to do the test again*/
				sensors_changed(&button_sensor);
				i++;
				etimer_reset(&table_check_timer);
				r1 = get_time_ms();
			}
		}


		else if (ev == sensors_event && data == &button_sensor) {
			//PRINTF("Initiaing global repair\n");
			rpl_repair_root(RPL_DEFAULT_INSTANCE);
			/*Remove all routes in the routing table*/
			uip_ds6_route_t *r;
			while (uip_ds6_route_num_routes()!=0 && uip_ds6_route_head()!= NULL){
				uip_ds6_route_rm(uip_ds6_route_head());
			}
			//	printf("Convergence: updated num_routes %u\n", uip_ds6_route_num_routes());
		}
	}

	while(1)
	{

		PROCESS_YIELD();
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
