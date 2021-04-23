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
#define TIME_SLOT RTIMER_SECOND/10    // 100 ms
#define SLEEP_CYCLE  10
/*---------------------------------------------------------------------------*/
// duty cycle = TIME_SLOT / ((1+SLEEP_CYLE)*TIME_SLOT)
/*---------------------------------------------------------------------------*/
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
PROCESS(cs4222_final_proj, "cs4222 final project");
AUTOSTART_PROCESSES(&cs4222_final_proj);
/*---------------------------------------------------------------------------*/
int indexOf( const int a[][4], int size, int value ) {
    int index = 0;
    while ( index < size && a[index][0] != value ) ++index;
    return ( index == size ? -1 : index );
}
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
  leds_on(LEDS_GREEN);
  memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));

  int threshold = 0;
  int curr_node = (int)received_packet.src_id;
  int node_index = indexOf(nodes, num_nodes+1, curr_node);
  int curr_rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  curr_timestamp = clock_time() / CLOCK_SECOND;

  if (nodes_in_proximity > 10) {
    threshold = -75;
  } else if (nodes_in_proximity > 5) {
    threshold = -70;
  } else {
    threshold = -65;
  }

  if (curr_rssi > threshold) {
    // Detection of a new node
    if (node_index == -1) {
      printf("%lu DETECT %d\n", curr_timestamp, curr_node);
      nodes_in_proximity++;
      printf("New node: %d || Nodes in proximity: %d\n", curr_node, nodes_in_proximity);
      // Add new nodeID to array
      nodes[num_nodes][0] = curr_node;
      // Add to both current and most recent timestamps
      nodes[num_nodes][1] = nodes[num_nodes][2] = (int)curr_timestamp;
      nodes[num_nodes][3] = 1;
        num_nodes++;
    }
    else {
      // Detection of nodes that left but came back
      if (nodes[node_index][3] == 0) {
        printf("%lu DETECT %d\n", curr_timestamp, curr_node);
        nodes_in_proximity++;
        printf("Node: %d has returned || Nodes in proximity: %d\n", curr_node, nodes_in_proximity);
        nodes[node_index][1] = nodes[node_index][2] = (int)curr_timestamp;
        nodes[node_index][3] = 1;
      }
      // For nodes already in the array
      else {
        printf("Node %d || RSSI: %d || %lu\n", curr_node, curr_rssi, curr_timestamp);
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

  while(1){
    // radio on
    NETSTACK_RADIO.on();

    // Device sends
    for(i = 0; i < NUM_SEND; i++){
      leds_on(LEDS_RED);
      
      packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));
      broadcast_send(&broadcast);
      leds_off(LEDS_RED);

      if(i != (NUM_SEND - 1)){
        rtimer_set(t, RTIMER_TIME(t) + TIME_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }

    // Device sleeps
    if(SLEEP_CYCLE != 0){
      leds_on(LEDS_BLUE);
      // radio off
      NETSTACK_RADIO.off();

      NumSleep = random_rand() % (2 * SLEEP_CYCLE + 1);

      for(i = 0; i < NumSleep; i++){
        rtimer_set(t, RTIMER_TIME(t) + TIME_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
      leds_off(LEDS_BLUE);
    }

    // Device checks for nodes not detected > 30s
    curr_timestamp = clock_time() / CLOCK_SECOND;
    //printf("TIMESTAMP (MS): %lu\n", curr_timestamp);
    //printf("TIMESTAMP (SEC): %lu\n", curr_timestamp/CLOCK_SECOND);

    // loop through all nodes detected since the beginning
    for(i=0; i<num_nodes; i++) {
      // Check if nodes in proximity have not been detected > 30s
      if (((nodes[i][3] == 1) && ((int)curr_timestamp - nodes[i][2]) > 30)) {
          printf("%lu LEAVE %d\n", curr_timestamp, nodes[i][0]);
          time_in_proximity = nodes[i][2] - nodes[i][1];
          
          // Reset nodes that left to 0
          nodes[i][1] = nodes[i][2] = nodes[i][3] = 0;

          printf("Time in proximity: %d\n", time_in_proximity);
          nodes_in_proximity--;
          printf("Nodes in proximity: %d\n", nodes_in_proximity);
      }
    }
  }
  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cs4222_final_proj, ev, data)
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

  NETSTACK_RADIO.off();

  data_packet.src_id = node_id;

  // Start sender in one millisecond.
  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

