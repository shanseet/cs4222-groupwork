#include "contiki.h"
#include "dev/leds.h"
#include <stdio.h>
#include "core/net/rime/rime.h"
#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "node-id.h"
#include "defs_and_types.h"
#include "net/netstack.h"
#include "random.h"
#ifdef TMOTE_SKY
#include "powertrace.h"
#endif
/*---------------------------------------------------------------------------*/
#define WAKE_TIME RTIMER_SECOND/10    // 10 HZ, 0.1s
/*---------------------------------------------------------------------------*/
#define SLEEP_CYCLE  9        	      // 0 for never sleep
#define SLEEP_SLOT RTIMER_SECOND/10   // sleep slot should not be too large to prevent overflow
/*---------------------------------------------------------------------------*/
// duty cycle = WAKE_TIME / (WAKE_TIME + SLEEP_SLOT * SLEEP_CYCLE)
/*---------------------------------------------------------------------------*/
// sender timer
static struct rtimer rt;
static struct pt pt;
/*---------------------------------------------------------------------------*/
static data_packet_struct received_packet;
static data_packet_struct data_packet;
int nodes[50][4];
    // nodes[i][1] = first detected time
    // nodes[i][2] = most recent time
    // nodes[i][3] = proximity flag
int num_nodes = 0;
int nodes_in_proximity = 0;
unsigned long curr_timestamp;
/*---------------------------------------------------------------------------*/
PROCESS(cc2650_nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&cc2650_nbr_discovery_process);
/*---------------------------------------------------------------------------*/
int indexOf( const int a[][4], int size, int value )
{
    int index = 0;
    while ( index < size && a[index][0] != value ) ++index;                                               
    return ( index == size ? -1 : index );
}

static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  leds_on(LEDS_GREEN);
  memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));

  int threshold = 0;
  int curr_node = (int)received_packet.src_id;
  int node_index = indexOf(nodes, num_nodes+1, curr_node);
  int curr_rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  curr_timestamp = clock_time() / CLOCK_SECOND;

  if (nodes_in_proximity > 10) {
    threshold = -100;
  } else if (nodes_in_proximity > 5) {
    threshold = -75;
  } else {
    threshold = -50;
  }

  if (curr_rssi > threshold) {
    // Detection of a new node
    if (node_index == -1) {
      printf("%lu DETECT %d\n", curr_timestamp, curr_node);
      nodes_in_proximity++;
      printf("New node: %d || Nodes in proximity: %d\n", curr_node, nodes_in_proximity);
      num_nodes++;
      // Add new nodeID to array
      nodes[num_nodes][0] = curr_node;
      // Add to both current and most recent timestamps
      nodes[num_nodes][1] = nodes[num_nodes][2] = (int)curr_timestamp;
      nodes[num_nodes][3] = 1;
    }
    else {
      // Detection of nodes that left but came back
      if (nodes[node_index][3] == 0) {
        printf("%lu DETECT %d\n", curr_timestamp, curr_node);
        nodes_in_proximity++;
        printf("Node: %d has returned || Nodes in proximity: %d\n", curr_node, nodes_in_proximity);
        nodes[node_index][1] = nodes[node_index][2] = (int)curr_timestamp;
      }
      // For nodes already in the array
      else {
        printf("Received Node %d || RSSI: %d\n", curr_node, curr_rssi);
        // Update only the most recent timestamp
        nodes[node_index][2] = (int)curr_timestamp;
      }
    }
  }

  leds_off(LEDS_GREEN);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
char sender_scheduler(struct rtimer *t, void *ptr) {
  static uint16_t i = 0;
  static int NumSleep=0;
  int time_in_proximity=0;
  PT_BEGIN(&pt);

  // curr_timestamp = clock_time(); 
  // printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

  while(1){

    // radio on
    NETSTACK_RADIO.on();

    for(i = 0; i < NUM_SEND; i++){
      leds_on(LEDS_RED);
      
      packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));
      broadcast_send(&broadcast);
      leds_off(LEDS_RED);

      if(i != (NUM_SEND - 1)){
        rtimer_set(t, RTIMER_TIME(t) + WAKE_TIME, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }

    if(SLEEP_CYCLE != 0){
      leds_on(LEDS_BLUE);
      // radio off
      NETSTACK_RADIO.off();

      // SLEEP_SLOT cannot be too large as value will overflow,
      // to have a large sleep interval, sleep many times instead

      // get a value that is uniformly distributed between 0 and 2*SLEEP_CYCLE
      // the average is SLEEP_CYCLE 
      NumSleep = random_rand() % (2 * SLEEP_CYCLE + 1);  
      // printf(" Sleep for %d slots \n",NumSleep);

      // NumSleep should be a constant or static int
      for(i = 0; i < NumSleep; i++){
        rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
      leds_off(LEDS_BLUE);
    }
    // 3D array: nodes
    curr_timestamp = clock_time() / CLOCK_SECOND;
    //printf("TIMESTAMP (MS): %lu\n", curr_timestamp);
    //printf("TIMESTAMP (SEC): %lu\n", curr_timestamp/CLOCK_SECOND);

    for(i=0; i<50; i++) {
      // Secondary check if the most recent time non 0, meaning they are not uninitialized
      if ((((int)curr_timestamp - nodes[i][2]) > 30) && (nodes[i][2] > 0)) {
          printf("%lu LEAVE %d\n", curr_timestamp, nodes[i][0]);
          time_in_proximity = nodes[i][2] - nodes[i][1];
          
          // Reset nodes that left
          nodes[i][1] = nodes[i][2] = nodes[i][3] = 0;
          printf("Time in proximity: %d\n", time_in_proximity);
          nodes_in_proximity--;
          printf("Nodes in proximity: %d\n", nodes_in_proximity);
      }
      else {
        continue;
      }
    }
  }
  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_nbr_discovery_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  random_init(54222);

  #ifdef TMOTE_SKY
  powertrace_start(CLOCK_SECOND * 5);
  #endif

  broadcast_open(&broadcast, 129, &broadcast_call);

  // for serial port
  #if !WITH_UIP && !WITH_UIP6
  uart1_set_input(serial_line_input_byte);
  serial_line_init();
  #endif

  // radio off
  NETSTACK_RADIO.off();

  // initialize data packet
  data_packet.src_id = node_id;

  // Start sender in one millisecond.
  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
