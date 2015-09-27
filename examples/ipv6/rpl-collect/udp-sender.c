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

#include "net/netstack.h"
#include "collect-common.h"
#include "collect-view.h"

#include <stdio.h>
#include <string.h>
#include <powertrace.h>

#define UDP_CLIENT_PORT 8775
#define UDP_SERVER_PORT 5688

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

// Changed
#define UIP_IP_BUF                ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#ifndef PERIOD
#define PERIOD 60
#endif

#define START_INTERVAL		(15 * CLOCK_SECOND)
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))
#define MAX_PAYLOAD_LEN		12

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process, &collect_common_process);
/*---------------------------------------------------------------------------*/
void
collect_common_set_sink(void)
{
  /* A udp client can never become sink */
}
/*---------------------------------------------------------------------------*/

void
collect_common_net_print(void)
{
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    /* Ignore incoming data */
  }
}
/*---------------------------------------------------------------------------*/
void
collect_common_send(void)
{
  /*For send regular packets*/
  static uint8_t seqno;
  struct {
    /*Changed*/
    uint8_t instance_id;   //RPL_SECOND_INSTANCE
    uint8_t for_alignment;
    uint8_t seqno;
    uint8_t for_alignment1;
    struct collect_view_data_msg msg;
  } msg;
  /* struct collect_neighbor *n; */
  uint16_t parent_etx;
  uint16_t rtmetric;
  uint16_t num_neighbors;
  uint16_t beacon_interval;
  rpl_parent_t *preferred_parent;
  rimeaddr_t parent;
  rpl_instance_t *instance; //changed
  rpl_dag_t *dag;

  if(client_conn == NULL) {
    /* Not setup yet */
    return;
  }
  memset(&msg, 0, sizeof(msg));
  seqno++;
  if(seqno == 0) {
    /* Wrap to 128 to identify restarts */
    seqno = 128;
  }
  msg.instance_id = RPL_SECOND_INSTANCE ; 
  msg.seqno = seqno;

  rimeaddr_copy(&parent, &rimeaddr_null);
  parent_etx = 0;

  /* Let's suppose we have only one instance */
  /*Changed*/
  instance = rpl_get_instance(RPL_DEFAULT_INSTANCE);
  dag = instance->current_dag;
  if(dag != NULL) {
    preferred_parent = dag->preferred_parent;
    if(preferred_parent != NULL) {
      uip_ds6_nbr_t *nbr;
      nbr = uip_ds6_nbr_lookup(rpl_get_parent_ipaddr(preferred_parent));
      if(nbr != NULL) {
        /* Use parts of the IPv6 address as the parent address, in reversed byte order. */
        parent.u8[RIMEADDR_SIZE - 1] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2];
        parent.u8[RIMEADDR_SIZE - 2] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
        parent_etx = rpl_get_parent_rank((rimeaddr_t *) uip_ds6_nbr_get_ll(nbr)) / 2;
      }
    }
    rtmetric = dag->rank;
    beacon_interval = (uint16_t) ((2L << dag->instance->dio_intcurrent) / 1000);
    num_neighbors = RPL_PARENT_COUNT(dag);
  } else {
    rtmetric = 0;
    beacon_interval = 0;
    num_neighbors = 0;
  }

  /* num_neighbors = collect_neighbor_list_num(&tc.neighbor_list); */
  collect_view_construct_message(&msg.msg, &parent,
                                 parent_etx, rtmetric,
                                 num_neighbors, beacon_interval);
  
  printf ("msg send, %d, %d\n", msg.instance_id, msg.seqno);
  // Cross layer
  //UIP_IP_BUF->instance = 0x1f;
  uip_udp_packet_sendto_by_instance(client_conn, &msg, sizeof(msg),
                        &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT), msg.instance_id);
}
/*---------------------------------------------------------------------------*/
static void
send_packet(void *ptr)
{
  /*For send critical packets*/
  static int seq_id;
  struct {
    uint8_t instance_id;   //RPL_DEFAULT_INSTANCE
    uint8_t for_alignment;
    uint8_t seqno;
    uint8_t for_alignment1;
    char msg[MAX_PAYLOAD_LEN];
  } msg;

  memset(&msg, 0, sizeof(msg));
  seq_id++;
  if(seq_id == 0) {
    /* Wrap to 128 to identify restarts */
    seq_id = 128;
  }
  msg.instance_id = RPL_DEFAULT_INSTANCE; 
  msg.seqno = seq_id ;
  sprintf(&msg.msg, "Hello %d", seq_id);
  
  printf ("msg send, %d, %d\n", msg.instance_id, msg.seqno);
  // Cross layer
  //UIP_IP_BUF->instance = 0x1e;
  uip_udp_packet_sendto_by_instance(client_conn, &msg, sizeof(msg),
                        &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT), msg.instance_id);
}
/*---------------------------------------------------------------------------*/
void
collect_common_net_init(void)
{
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* set server address */
  uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic;
  static struct ctimer backoff_timer;
  PROCESS_BEGIN();
  /* Start powertracing, once every two seconds. */
  //powertrace_start(CLOCK_SECOND * 30);

  PROCESS_PAUSE();

  set_global_address();

  PRINTF("UDP client process started\n");

  print_local_addresses();

  /* new connection with remote host */
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

  /* for sending data */
  etimer_set(&periodic, SEND_INTERVAL);

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
    }
    /*Changed*/
    /*Set interval for send critical packet*/ 
    if(etimer_expired(&periodic)) {
      etimer_reset(&periodic);
      ctimer_set(&backoff_timer, SEND_TIME, send_packet, NULL);

    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
