/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
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
 */

/**
 * \file
 *      CoAP module for reliable transport
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include "contiki.h"
#include "contiki-net.h"
#include "er-coap-transactions.h"
#include "er-coap-observe.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/*---------------------------------------------------------------------------*/
MEMB(transactions_memb, coap_transaction_t, COAP_MAX_OPEN_TRANSACTIONS);
LIST(transactions_list);

/* Eifel */

MEMB(rtt_estimations_memb, coap_rtt_estimations_t, COAP_MAX_RTT_ESTIMATIONS);
LIST(rtt_estimations_list);


static struct process *transaction_handler_process = NULL;

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_register_as_transaction_handler()
{
  transaction_handler_process = PROCESS_CURRENT();
}
coap_transaction_t *
coap_new_transaction(uint16_t mid, uip_ipaddr_t *addr, uint16_t port)
{
  coap_transaction_t *t = memb_alloc(&transactions_memb);

  if(t) {
    t->mid = mid;
    t->retrans_counter = 0;

    /* save client address */
    uip_ipaddr_copy(&t->addr, addr);
    t->port = port;

    list_add(transactions_list, t); /* list itself makes sure same element is not added twice */
  }

  return t;
}
/*---------------------------------------------------------------------------*/
/* Eifel */

clock_time_t check_rtt(coap_transaction_t *t){
	coap_rtt_estimations_t *prevseg = NULL;

	for (prevseg = (coap_rtt_estimations_t*)list_head(rtt_estimations_list); prevseg; prevseg = prevseg->next){
          			if (uip_ipaddr_cmp(&prevseg->addr, &t->addr)){
					clock_time_t rto = prevseg->rto;
					return rto;
				}
	}
	return COAP_INITIAL_RTO;
} 
/*---------------------------------------------------------------------------*/

/* Eifel */

void update_rto(coap_transaction_t *t, clock_time_t oldrto){
	coap_rtt_estimations_t *prevseg = NULL;

	for (prevseg = (coap_rtt_estimations_t*)list_head(rtt_estimations_list); prevseg; prevseg = prevseg->next){
          			if (uip_ipaddr_cmp(&prevseg->addr, &t->addr)){
					prevseg->rto = oldrto;
					return;
				}
	}
	return;
}
/*---------------------------------------------------------------------------*/
void
coap_send_transaction(coap_transaction_t *t)
{
  PRINTF("Sending transaction %u\n", t->mid);

  coap_send_message(&t->addr, t->port, t->packet, t->packet_len);

  if(COAP_TYPE_CON ==
     ((COAP_HEADER_TYPE_MASK & t->packet[0]) >> COAP_HEADER_TYPE_POSITION)) {
    if(t->retrans_counter < COAP_MAX_RETRANSMIT) {
      /* not timed out yet */
     // PRINTF("Keeping transaction %u\n", t->mid);
     // if(t->retrans_counter == 0) {
     /* Eifel */

    if(t->retrans_counter == 0) {
	 t->timestamp = clock_time();
      	clock_time_t rto = check_rtt(t);
        t->retrans_timer.timer.interval = rto + random_rand() % (rto >> 1) ; 
	// t->retrans_timer.timer.interval = rto;
      //  PRINTF("Initial interval %f\n", (float)t->retrans_timer.timer.interval / CLOCK_SECOND);
	
    /*  t->retrans_timer.timer.interval =
          COAP_RESPONSE_TIMEOUT_TICKS + (random_rand()
                                         %
                                         (clock_time_t)
                                         COAP_RESPONSE_TIMEOUT_BACKOFF_MASK); */

	//t->retrans_timer.timer.interval = COAP_RESPONSE_TIMEOUT_TICKS;

        //PRINTF("Initial interval %f\n",
          //     (float)t->retrans_timer.timer.interval / CLOCK_SECOND);
	    printf("Initial interval %f (%u) \n",(float)t->retrans_timer.timer.interval / CLOCK_SECOND, t->mid);
      } else {
        t->retrans_timer.timer.interval <<= 1;  /* double */
        //PRINTF("Doubled (%u) interval %f\n", t->retrans_counter,
          //     (float)t->retrans_timer.timer.interval / CLOCK_SECOND);
	   printf("Doubled (%u) interval %f (%u) \n",t->retrans_counter,(float)t->retrans_timer.timer.interval / CLOCK_SECOND, t->mid);
	   
	   /* Eifel */
	   
	   update_rto(t,t->retrans_timer.timer.interval);
      }

      PROCESS_CONTEXT_BEGIN(transaction_handler_process);
      etimer_restart(&t->retrans_timer);        /* interval updated above */
      PROCESS_CONTEXT_END(transaction_handler_process);

      t = NULL;
    } else {
      /* timed out */
      printf("Timeout\n");
      restful_response_handler callback = t->callback;
      void *callback_data = t->callback_data;

      /* handle observers */
      coap_remove_observer_by_client(&t->addr, t->port);

      coap_clear_transaction(t);

      if(callback) {
        callback(callback_data, NULL);
      }
    }
  } else {
    coap_clear_transaction(t);
  }
}
/*---------------------------------------------------------------------------*/
void
coap_clear_transaction(coap_transaction_t *t)
{
  /* if(t) {
    PRINTF("Freeing transaction %u: %p\n", t->mid, t);
    if(COAP_TYPE_CON == ((COAP_HEADER_TYPE_MASK & t->packet[0]) >> COAP_HEADER_TYPE_POSITION)) {
    	if(t->retrans_counter < COAP_MAX_RETRANSMIT) {
    		//printf("Counter (%u), RTO %f\n", t->retrans_counter,(float)t->retrans_timer.timer.interval / CLOCK_SECOND); 
         }
    }   
    etimer_stop(&t->retrans_timer);
    list_remove(transactions_list, t);
    memb_free(&transactions_memb, t); 
   } */
 /* Eifel */
 
  coap_rtt_estimations_t *prevseg = NULL;
  int pktnotfound = 1;
  if(t) {
    if(COAP_TYPE_CON == ((COAP_HEADER_TYPE_MASK & t->packet[0]) >> COAP_HEADER_TYPE_POSITION)) {
	//if(t->retrans_counter < COAP_MAX_RETRANSMIT) {
	if(t->retrans_counter == 0) {
   		clock_time_t rtt = clock_time() - t->timestamp; 
		for (prevseg = (coap_rtt_estimations_t*)list_head(rtt_estimations_list); prevseg; prevseg = prevseg->next){
          		if (uip_ipaddr_cmp(&prevseg->addr, &t->addr)){
              			prevseg->rtt = rtt;
              			pktnotfound = 0;
              			break;
          		}
        	}
        	if(pktnotfound){
          		coap_rtt_estimations_t *e = memb_alloc(&rtt_estimations_memb);
          		if(e){
	        		e->rtt = rtt;
          			e->rttvar = 0;
          			e->srtt = 0;
          			e->srttCoef = 1;
          			e->rttvarCoef = 1;

         			uip_ipaddr_copy(&e->addr, &t->addr);
          			list_add(rtt_estimations_list, e);
					prevseg = e;
			}
	  	}
		calculateRTO(prevseg);
    		//printf("RTT %lu\n",rtt);
	}

     	//if(t->retrans_counter > 0 && t->retrans_counter<COAP_MAX_RETRANSMIT){
			//printf("Counter (%u), RTO %f\n", t->retrans_counter,(float)t->retrans_timer.timer.interval / CLOCK_SECOND);
     	//}
    }
    etimer_stop(&t->retrans_timer);
    list_remove(transactions_list, t);
    memb_free(&transactions_memb, t);
  } 
}

/*---------------------------------------------------------------------------*/ 

coap_transaction_t *
coap_get_transaction_by_mid(uint16_t mid)
{
  coap_transaction_t *t = NULL;

  for(t = (coap_transaction_t *)list_head(transactions_list); t; t = t->next) {
    if(t->mid == mid) {
      PRINTF("Found transaction for MID %u: %p\n", t->mid, t);
      return t;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
void
coap_check_transactions()
{
  coap_transaction_t *t = NULL;

  for(t = (coap_transaction_t *)list_head(transactions_list); t; t = t->next) {
    if(etimer_expired(&t->retrans_timer)) {
      ++(t->retrans_counter);
      printf("Retransmitting %u (%u)\n", t->mid, t->retrans_counter);
      coap_send_transaction(t);
    }
  }
}
/*---------------------------------------------------------------------------*/
