/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "board-peripherals.h"
#include <stdio.h>
#include "sys/etimer.h"
#include "net/rime/rime.h"
#include <stdio.h>
/*---------------------------------------------------------------------------*/


static char message[50] = {0};
static void send(char message[], int size);
static struct etimer timer_etimer;

PROCESS(transmit_process, "unicasting...");
AUTOSTART_PROCESSES(&transmit_process);

static const struct unicast_callbacks unicast_callbacks = {};
static struct unicast_conn uc;

static void send(char message[], int size){
	linkaddr_t addr;
	printf("%s\n",message);
   packetbuf_copyfrom(message, strlen(message));

   // COMPUTE THE ADDRESS OF THE RECEIVER FROM ITS NODE ID, FOR EXAMPLE NODEID 0xBA04 MAPS TO 0xBA AND 0x04 RESPECTIVELY
   // In decimal, if node ID is 47620, this maps to 186 (higher byte) AND 4 (lower byte)
   addr.u8[0] = 0x5; // HIGH BYTE or 186 in decimal
   addr.u8[1] = 0x02; // LOW BYTE or 4 in decimal
   if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
         unicast_send(&uc, &addr);
   }

}

PROCESS_THREAD(transmit_process, ev, data) {

	PROCESS_EXITHANDLER(unicast_close(&uc);)
	PROCESS_BEGIN();
	unicast_open(&uc, 146, &unicast_callbacks);
   while(1) {
      etimer_set(&timer_etimer, CLOCK_SECOND/4);
      PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
      message[0] += 1;
      message[0] = message[0] % 40;
      // counter++;
      // counter = counter%40;

      // memcpy(message, &counter, sizeof(counter));
      // message[49]+= 1;
      // if (message[49] == ':') {
      //    message[49] = '0';
      //    message[48] += 1;
      // }
      // if (message[48] == '4') {
      //    message[48] = '0';
      // }
      send(message, 0);
   }


   PROCESS_END();
}

/*---------------------------------------------------------------------------*/
