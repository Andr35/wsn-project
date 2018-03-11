#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "my_collect.h"

#define BEACON_INTERVAL (CLOCK_SECOND*10) // TODO set (CLOCK_SECOND*60)
#define BEACON_FORWARD_DELAY (random_rand() % CLOCK_SECOND)

#define RSSI_THRESHOLD -95

/* Forward declarations */
void bc_recv(struct broadcast_conn *conn, const linkaddr_t *sender);
void uc_recv(struct unicast_conn *c, const linkaddr_t *from);
void beacon_timer_cb(void* ptr);
/* Callback structures */
struct broadcast_callbacks bc_cb = {.recv=bc_recv};
struct unicast_callbacks uc_cb = {.recv=uc_recv};

bool is_the_sink = false;

/*--------------------------------------------------------------------------------------*/
void my_collect_open(struct my_collect_conn* conn, uint16_t channels, bool is_sink, const struct my_collect_callbacks *callbacks) {
  // initialise the connector structure
  linkaddr_copy(&conn->parent, &linkaddr_null);
  conn->metric = 65535; // the max metric (means that the node is not connected yet)
  conn->beacon_seqn = 0;
  conn->callbacks = callbacks;

  // open the underlying primitives
  broadcast_open(&conn->bc, channels,     &bc_cb);
  unicast_open  (&conn->uc, channels + 1, &uc_cb);

  // TASK 1: make the sink send beacons periodically

  // Save is_sink value (used in on_recv callback)
  is_the_sink = is_sink;

  // Sink ///////////////////////////////////////
  if (is_the_sink) { // Only if the node is the sink, otherwise everybody starts sending stuff
    printf("<open> Node is the sink (node: %02x:%02x).\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

    // Sink has 0 as metric
    conn->metric = 0;

    // Params
    // c	A pointer to the callback timer.
    // t	The interval before the timer expires.
    // f	A function to be called when the timer expires.
    // ptr	An opaque pointer that will be supplied as an argument to the callback function.
    ctimer_set(&conn->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, conn);
  }
}

/* Handling beacons --------------------------------------------------------------------*/

struct beacon_msg { // Beacon message structure
  uint16_t seqn;
  uint16_t metric;
} __attribute__((packed));

// Send beacon using the current seqn and metric
void send_beacon(struct my_collect_conn* conn) {
  struct beacon_msg beacon = {.seqn = conn->beacon_seqn, .metric = conn->metric};

  packetbuf_clear();
  packetbuf_copyfrom(&beacon, sizeof(beacon));
  broadcast_send(&conn->bc);
  printf("<out> <beacon> Beacon sent in broadcast (seqn: %d, metric: %d)\n", conn->beacon_seqn, conn->metric);
}

// Beacon timer callback
void beacon_timer_cb(void* ptr) { // ptr is the connection (my_collect_conn* conn)
  // TASK 2: implement the beacon callback

  // Cast param
  struct my_collect_conn *conn = (struct my_collect_conn *)ptr;
  // sink send new beacon -> increment the beacon seq num
  conn->beacon_seqn = conn->beacon_seqn + 1;
  // Send beacon
  send_beacon(conn);
  // Restart timer
  ctimer_reset(&conn->beacon_timer);
}

// Beacon receive callback
void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
  struct beacon_msg beacon;
  int16_t rssi;
  // Get the pointer to the overall structure my_collect_conn from its field bc
  struct my_collect_conn* conn = (struct my_collect_conn*)(((uint8_t*)bc_conn) - offsetof(struct my_collect_conn, bc));

  if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
    printf("<in_> <beacon> Beacon received but with the wrong size\n");
    return;
  }

  memcpy(&beacon, packetbuf_dataptr(), sizeof(struct beacon_msg));
  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf("<in_> <beacon> Beacon received from: %02x:%02x (seqn: %u, metric: %u, rssi %d)\n",
    sender->u8[0], sender->u8[1], beacon.seqn, beacon.metric, rssi);

  // TASK 3: analyse the received beacon, update the routing info (parent, metric), if needed
  // TASK 4: retransmit the beacon if the metric or the seqn has been updated

  // Check if beacon is new or is the current one (already seen but maybe with a better metric) ->
  // if true, analyze beacon metric (otherwise discard it)
  if (beacon.seqn >= conn->beacon_seqn) { // Beacon has higher seqn than every beacon already seen

    // Update current beacon seqn with the newest
    conn->beacon_seqn = beacon.seqn;

    // Update parent if metric is better and RSSI is tolerable (> -95 dBm)
    if ((beacon.metric < conn->metric)  && (rssi > RSSI_THRESHOLD)) {
      printf("<in_> <beacon> Received beacon has a better metric (%u < %u) New parent is node %02x:%02x\n",
        beacon.metric, conn->metric, sender->u8[0], sender->u8[1]);

      // Update current metric info and update parent
      conn->metric = beacon.metric + 1;
      linkaddr_copy(&conn->parent, sender);

      // Retransmit beacon to other nodes (with updated metric)
      // Wait some random time to avoid (hopefully) collision
      printf("<in_> <beacon> Schedule beacon forwarding in %d seconds\n", BEACON_FORWARD_DELAY);
      // send_beacon(conn);
      // NB: here "&conn->beacon_timer" is used since in normal node it is unused and
      // in sink these lines of code are never executed (sink has always metric = 0)
      ctimer_set(&conn->beacon_timer, BEACON_FORWARD_DELAY, send_beacon, conn);
    }

  } else {
      printf("<in_> <beacon> Received an old beacon (current node seqn %u, beacon seqn: %u). Discarded.\n", conn->beacon_seqn, beacon.seqn);
  }

}

/* Handling data packets --------------------------------------------------------------*/

struct collect_header { // Header structure for data packets
  linkaddr_t source;
  uint8_t hops;
} __attribute__((packed));

// Our send function
int my_collect_send(struct my_collect_conn *conn) {
  struct collect_header hdr = {.source=linkaddr_node_addr, .hops=0};

  if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {
    printf("<out> <packet> <ERROR> Trying to send a data collection packet but node's parent is missing!\n");
    return 0; // no parent
  }

  // TASK 5:
  //  - allocate space for the header
  //  - insert the header
  //  - send the packet to the parent using unicast

  // Try to allocate space
  int alloc_res = packetbuf_hdralloc(sizeof(struct collect_header));

  if (alloc_res == 0) { // Allocation failed -> report error
    printf("<out> <packet> <ERROR> Trying to send a data collection packet but node fails allocating header buffer!\n");
    return 0;
  }

  // Add header to packet
  memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct collect_header));
  // Send packet to parent
  return unicast_send(&conn->uc, &conn->parent);
}

// Data receive callback
void uc_recv(struct unicast_conn *uc_conn, const linkaddr_t *from) {
  // Get the pointer to the overall structure my_collect_conn from its field uc
  struct my_collect_conn* conn = (struct my_collect_conn*)(((uint8_t*)uc_conn) -
    offsetof(struct my_collect_conn, uc));

  struct collect_header hdr;

  if (packetbuf_datalen() < sizeof(struct collect_header)) {
    printf("<in_> <packet> <ERROR> Received a too short unicast packet! (length: %d)\n", packetbuf_datalen());
    return;
  }

  // TASK 6:
  //  - extract the header
  //  - on the sink, remove the header and call the application callback
  //  - on a router, update the header and forward the packet to the parent using unicast

  // Save header in hdr (read from "dataptr" to "dataptr" + sizeof header)
  memcpy(&hdr, packetbuf_dataptr(), sizeof(struct collect_header));

  // Sink ///////////////////////////////////////
  if (is_the_sink) {

    // Remove header
    int hdr_reduce_res = packetbuf_hdrreduce(sizeof(struct collect_header));

    if (hdr_reduce_res == 0) {
      printf("<in_> <packet> <ERROR> Fail to reduce header. Packet will not be delivered to app!\n");
      return;
    }

    // Send packet to application
    conn->callbacks->recv(&hdr.source, hdr.hops);

    printf("<in_> <packet> <SUCCESS> Packet arrived to the sink! (source: %02x:%02x, hops: %u)\n",
      hdr.source.u8[0], hdr.source.u8[1], hdr.hops);


  // Common node ////////////////////////////////
  } else { // Packet needs to be forwarded to parent

    // Check for parent existence
    if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {
      printf("<in_> <packet> <ERROR> Trying to forward a packet but node's parent is missing!\n");
      return; // no parent
    }

    // Update hops in header before forward
    hdr.hops += 1;
    // Overwrite the header present in packet buffer
    memcpy(packetbuf_dataptr(), &hdr, sizeof(struct collect_header));
    // Forward the packet to parent
    unicast_send(&conn->uc, &conn->parent);
    printf("<in_> <packet> Packet forwarded to %02x:%02x (current hops: %u)\n", conn->parent.u8[0], conn->parent.u8[1], hdr.hops);

  }
}



/* Send commands from sink ------------------------------------------------------------*/


// Send command function
int sr_send(struct my_collect_conn *conn, const linkaddr_t *dest) {
  // TODO implement
  return -1;
}
