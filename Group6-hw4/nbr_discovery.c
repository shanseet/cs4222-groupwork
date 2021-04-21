#include <math.h>
#include <stdio.h>

#include "contiki.h"
#include "core/net/rime/rime.h"
#include "defs_and_types.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "net/netstack.h"
#include "node-id.h"
#include "random.h"
#ifdef TMOTE_SKY
#include "powertrace.h"
#endif
/*---------------------------------------------------------------------------*/
// #define TIME_SLOT RTIMER_SECOND / (40)    // 25ms
// #define TIME_SLOT RTIMER_SECOND/(20)      // 50ms
#define TIME_SLOT RTIMER_SECOND/(50/3)      // 60ms
// #define TIME_SLOT RTIMER_SECOND/(100/7)      // 70ms
// #define TIME_SLOT RTIMER_SECOND/(40/3)    // 75ms
// #define TIME_SLOT RTIMER_SECOND/(10)     // 100ms
/*---------------------------------------------------------------------------*/
// #define N_size 20     // 25ms
// #define N_size 14     // 50ms
#define N_size 13     // 60ms
// #define N_size 12     // 70 & 75ms
// #define N_size 10     // 100ms
static uint16_t i = 0;
static long curr_box = 0;
long rand_row = 0;
long rand_col = 0;
/*---------------------------------------------------------------------------*/
// sender timer
static struct rtimer rt;
static struct pt pt;
/*---------------------------------------------------------------------------*/
static data_packet_struct received_packet;
static data_packet_struct data_packet;
unsigned long curr_timestamp;
unsigned long start_timestamp;
/*---------------------------------------------------------------------------*/
PROCESS(cc2650_nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&cc2650_nbr_discovery_process);
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  leds_on(LEDS_GREEN);
  memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));

  printf("Send seq# %lu  @ %8lu  %3lu.%03lu\n", data_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

  printf("Received packet from node %lu with sequence number %lu and timestamp %3lu.%03lu\n",
    received_packet.src_id, received_packet.seq, (curr_timestamp-start_timestamp) / CLOCK_SECOND, 
    (((curr_timestamp-start_timestamp) % CLOCK_SECOND)*1000) / CLOCK_SECOND);
  leds_off(LEDS_GREEN);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
char sender_scheduler(struct rtimer *t, void *ptr) {
  PT_BEGIN(&pt);

  rand_row = random_rand() % N_size;
  rand_col = random_rand() % N_size;
  curr_timestamp = clock_time();
  start_timestamp = clock_time();

  printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

  while (1) {
    // if curr_box matches either row or col
    if (((curr_box >= rand_row * N_size) && (curr_box < (rand_row+1)*N_size)) || (curr_box % N_size) == rand_col) {
      // radio on
      NETSTACK_RADIO.on();

      // send a packet
      for(i = 0; i < NUM_SEND; i++){
        leds_on(LEDS_RED);
        
        data_packet.seq++;
        curr_timestamp = clock_time();
        data_packet.timestamp = curr_timestamp;
        
        //printf("  Sending a packet at slot %d\n", curr_box);
        printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq, curr_timestamp-start_timestamp, 
          (curr_timestamp-start_timestamp) / CLOCK_SECOND, 
          (((curr_timestamp-start_timestamp) % CLOCK_SECOND)*1000) / CLOCK_SECOND);

        packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));
        broadcast_send(&broadcast);
        leds_off(LEDS_RED);

        if(i != (NUM_SEND - 1)){
          rtimer_set(t, RTIMER_TIME(t) + TIME_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
          PT_YIELD(&pt);
        }
      }
    } else { // if curr_box does not match row and col, sleep for that 1 slot
      leds_on(LEDS_BLUE);
      NETSTACK_RADIO.off();

      rtimer_set(t, RTIMER_TIME(t) + TIME_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
      PT_YIELD(&pt);

      leds_off(LEDS_BLUE);
    }
    
    curr_box++;
    
    if (curr_box >= N_size*N_size) {
      curr_box = 0;
      data_packet.seq = 0;
      rand_row = random_rand() % N_size;
      rand_col = random_rand() % N_size;
      start_timestamp = clock_time();
      printf("\n==========================\n");
    }
  }
  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_nbr_discovery_process, ev, data) {
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  random_init(node_id);

  #ifdef TMOTE_SKY
  powertrace_start(CLOCK_SECOND * 5);
  #endif

  broadcast_open(&broadcast, 129, &broadcast_call);

  // for serial port
  #if !WITH_UIP && !WITH_UIP6
  uart1_set_input(serial_line_input_byte);
  serial_line_init();
  #endif

  printf("CC2650 neighbour discovery - deterministic approach\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));
  printf("Size of timeslot, N used is %d, %d\n", TIME_SLOT, N_size);
  
  // radio off
  NETSTACK_RADIO.off();

  // initialize data packet
  data_packet.src_id = node_id;
  data_packet.seq = 0;

  // Start sender in one millisecond.
  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}
/*-------------------------
Commands:
  cd contiki/examples/nbr_discovery
  make TARGET=srf06-cc26xx BOARD=sensortag/cc2650 nbr_discovery.bin
  CPU_FAMILY=cc26xx
  make TARGET=sky nbr_discovery.upload
-------------------------------*/
