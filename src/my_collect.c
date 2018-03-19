#include <stdlib.h>
#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "my_collect.h"
#include "my_routing_table.h"

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
    initialize_sink(conn);
  }

  printf("<open> Node is %u.\n", linkaddr_node_addr.u16);
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

void send_beacon_cb(void* ptr) {
  // Cast param
  struct my_collect_conn *conn = (struct my_collect_conn *)ptr;
  send_beacon(conn);
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
    // TODO use "beacon.metric + 1" or "beacon.metric"?
    // 1) avoid extra broadcast if metric distance not change (same node contact current node with the same metric
    //    and the current node avoid to re-broadcast the beacon)
    // 2) if same node with the same metric recontact the current node, it rebroadcast the beacon ->
    //    other nodes have the possibility to listen for the beacon if the first time has missed it
    if (((beacon.metric) < conn->metric)  && (rssi > RSSI_THRESHOLD)) {
      printf("<in_> <beacon> Received beacon has a better metric (%u < %u) New parent is node %02x:%02x\n",
        beacon.metric, conn->metric, sender->u8[0], sender->u8[1]);

      // Update current metric info and update parent
      conn->metric = beacon.metric + 1;
      linkaddr_copy(&conn->parent, sender);

      // Retransmit beacon to other nodes (with updated metric)
      // Wait some random time to avoid (hopefully) collision
      printf("<in_> <beacon> Schedule beacon forwarding in %lu seconds\n", BEACON_FORWARD_DELAY);
      // send_beacon(conn);
      // NB: here "&conn->beacon_timer" is used since in normal node it is unused and
      // in sink these lines of code are never executed (sink has always metric = 0)
      ctimer_set(&conn->beacon_timer, BEACON_FORWARD_DELAY, send_beacon_cb, conn);
    }

  } else {
      printf("<in_> <beacon> Received an old beacon (current node seqn %u, beacon seqn: %u). Discarded.\n", conn->beacon_seqn, beacon.seqn);
  }

}


/* Handling data packets --------------------------------------------------------------*/

// Our send function
int my_collect_send(struct my_collect_conn *conn) {
  // is_command=false -> this is NOT a packet routed from sink (it is a data collection packet)
  // path_length=1 -> add current node to the path array
  struct collect_header hdr = {.source=linkaddr_node_addr, .hops=0, .is_command=false, .path_length=1};

  if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {
    printf("<out> <packet> <ERROR> Trying to send a data collection packet but node's parent is missing!\n");
    return 0; // no parent
  }

  // TASK 5:
  //  - allocate space for the header
  //  - insert the header
  //  - send the packet to the parent using unicast

  // Try to allocate space
  int alloc_res = packetbuf_hdralloc(sizeof(struct collect_header) + sizeof(linkaddr_t)); // header + path array

  if (alloc_res == 0) { // Allocation failed -> report error
    printf("<out> <packet> <ERROR> Trying to send a data collection packet but node fails allocating header buffer!\n");
    return 0;
  }

  // Add header to packet
  memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct collect_header));
  // Add current node to path array after the header
  memcpy(packetbuf_hdrptr() + sizeof(struct collect_header), &linkaddr_node_addr, sizeof(linkaddr_t));
  // Send packet to parent
  printf("<out> <packet> Sending data collection packet to %02x:%02x\n", conn->parent.u8[0], conn->parent.u8[1]);
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


  if (hdr.is_command) { // Packet is of type "command" (sent from sink)
    handle_recv_command_packet(conn, &hdr, from);
  } else { // Packet is of type "data collection"
    handle_recv_data_collection_packet(conn, &hdr, from);
  }
}


/**
 * Handle the reception of a data collection packet.
 * If node is sink -> deliver packet to app
 * If node is a common node -> forward packet to parent
 *
 */
void handle_recv_data_collection_packet(struct my_collect_conn *conn, struct collect_header *hdr, const linkaddr_t *from) {

  // Sink ///////////////////////////////////////
  if (is_the_sink) {

    // Collect all <parent, child> relationships contained into the path
    uint8_t path_length = hdr->path_length;
    linkaddr_t path[path_length];

    // Get route path from packet
    memcpy(&path, packetbuf_dataptr() + sizeof(struct collect_header), sizeof(linkaddr_t) * path_length);

    if (path_length == 0) { // Error -> "no one send me the packet" -> some node does not respect model
      printf("<in_> <packet> <ERROR> path_length value in header is wrong -> path_length is 0");
      return;
    }

    // Save <parent, child> relationship into routing table
    // Iterate over "path" array and consider i-element as parent and (i+1)-element as child
    int i;
    for (i = 0; i < (path_length - 1); i++) { // -1 last element has no child
      linkaddr_t parent = path[i];
      linkaddr_t child = path[i + 1];
      // Update routing table
      routing_table_update_entry(&parent, &child);
    }
    // Add special pair <sink, last_path_elem>
    routing_table_update_entry(&linkaddr_node_addr, &path[path_length - 1]);

    // Remove header
    int hdr_reduce_res = packetbuf_hdrreduce(sizeof(struct collect_header) + (sizeof(linkaddr_t) * path_length));

    if (hdr_reduce_res == 0) {
      printf("<in_> <packet> <ERROR> Fail to reduce header. Packet will not be delivered to app!\n");
      return;
    }

    // Deliver packet to application
    conn->callbacks->recv(&hdr->source, hdr->hops);

    printf("<in_> <packet> <SUCCESS> Packet arrived to the sink! (source: %02x:%02x, hops: %u)\n",
      hdr->source.u8[0], hdr->source.u8[1], hdr->hops);


  // Common node ////////////////////////////////
  } else { // Packet needs to be forwarded to parent

    // Check for parent existence
    if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {
      printf("<in_> <packet> <ERROR> Trying to forward a packet but node's parent is missing!\n");
      return; // no parent
    }

    // Extract routing path from packet
    uint8_t path_length = hdr->path_length;
    linkaddr_t path[path_length];
    memcpy(&path, packetbuf_dataptr() + sizeof(struct collect_header), sizeof(linkaddr_t) * path_length);

    // TODO remove printf or simplify
    int i;
    printf("<in_> <packet> New collection packet to forward: (from: %02x:%02x, source: %02x:%02x, hops: %u, length: %u) [",
      from->u8[0], from->u8[1], hdr->source.u8[0], hdr->source.u8[1], hdr->hops, path_length);
    for (i = 0; i < path_length; i++) {
      printf("%02x:%02x,", path[i].u8[0], path[i].u8[1]);
    }
    printf("]\n");


    // Check for loops -> if this node is present in path contained in packet, packet is
    // already been forwarded by this node -> drop
    int node_count = check_loop_presence(path, path_length, linkaddr_node_addr);

    if (node_count > 0) { // Loop -> stop forwarding
      printf("<in_> <packet> Packet cannot be forwarded beacuse a loop has been detected analyzing path");
      return;
    }

    // Add current node address to the route path contained in packet

    // Update path length in header before forward
    hdr->path_length += 1;
    // Update hops in header before forward
    hdr->hops += 1;

    // Allocate extra space in buffer header for new address
    int alloc_res = packetbuf_hdralloc(sizeof(struct collect_header) + (sizeof(linkaddr_t) * hdr->path_length)); // header + path array

    if (alloc_res == 0) { // Allocation failed -> report error
      printf("<in_> <packet> <ERROR> Trying to forward a data collection packet but node fails allocating header buffer!\n");
      return;
    }

    // Overwrite the header present in packet buffer
    memcpy(packetbuf_hdrptr(), hdr, sizeof(struct collect_header));
    // Add current node address in packet buffer [_, D, E, F] -> [A, D, E, F]
    memcpy(packetbuf_hdrptr() + sizeof(struct collect_header), &linkaddr_node_addr, sizeof(linkaddr_t));
    // Overwrite path in packet (should be unuseful)
    memcpy(packetbuf_hdrptr() + sizeof(struct collect_header) + sizeof(linkaddr_t), &path, (sizeof(linkaddr_t) * path_length));

    // Forward the packet to parent
    unicast_send(&conn->uc, &conn->parent);
    printf("<in_> <packet> Packet forwarded to %02x:%02x (current hops: %u)\n", conn->parent.u8[0], conn->parent.u8[1], hdr->hops);

  }
}

/**
 * Handle the reception of a "command" packet sent by sink.
 * If node is sink -> something goes wrong (sink send a packet to itself) (should not occur)
 * If node is a common node -> forward packet to the next node declared in the route path written in packet header
 * If node is the recipient of the packet -> deliver to app
 *
 */
void handle_recv_command_packet(struct my_collect_conn *conn, struct collect_header *hdr, const linkaddr_t *from) {

  // Sink ///////////////////////////////////////
  if (is_the_sink) {

    printf("<in_> <command> <ERROR> Sink received a command packet! It will be discarded\n");
    return;

  // Common node ////////////////////////////////
  } else { // Packet needs to be forwarded to parent

    // Check if this node is the recipient of the packet
    if (hdr->path_length == 0) { // Route path is empty -> current node is the recipient

    // Remove header
    int hdr_reduce_res = packetbuf_hdrreduce(sizeof(struct collect_header));

    if (hdr_reduce_res == 0) {
      printf("<in_> <command> <ERROR> Fail to reduce header. Command packet will not be delivered to app!\n");
      return;
    }

    // Deliver packet to application
    conn->callbacks->sr_recv(conn, hdr->hops);

    printf("<in_> <command> <SUCCESS> Command arrived to the node! (source: %02x:%02x, hops: %u)\n",
      hdr->source.u8[0], hdr->source.u8[1], hdr->hops);

    } else { // Node is NOT the recipient -> it must forward the packet to the next node

      linkaddr_t next_node_addr;
      // Extract next node from route path attached to packet header
      memcpy(packetbuf_dataptr() + sizeof(struct collect_header), &next_node_addr, sizeof(linkaddr_t));

      // Resize the packet buffer removing size of the extracted node address
      int hdr_reduce_res = packetbuf_hdrreduce(sizeof(linkaddr_t));

      if (hdr_reduce_res == 0) {
        printf("<out> <command> <ERROR> Fail to reduce header. Command packet will not be forwarded to the next node!\n");
        return;
      }

      // Update hops and path length in header before forward
      hdr->hops += 1;
      hdr->path_length -= 1;

      // Overwrite the header present in packet buffer with new one
      memcpy(packetbuf_dataptr(), hdr, sizeof(struct collect_header));

      // Forward the packet to next node
      unicast_send(&conn->uc, &next_node_addr);
      printf("<out> <command> Packet forwarded to %02x:%02x (current hops: %u)\n", next_node_addr.u8[0], next_node_addr.u8[1], hdr->hops);
    }

  }

}



/* Send commands from sink ------------------------------------------------------------*/


// Send command function
int sr_send(struct my_collect_conn *conn, const linkaddr_t *dest) {

  // Prepare header
  // is_command=true -> this is a sink to node packet (one-to-many)
  struct collect_header hdr = {.source=linkaddr_node_addr, .hops=0, .is_command=true, .path_length=0};

  // Create the route path to attach to the packet to help nodes to forward the packet
  struct source_route route = routing_table_find_route_path(dest);

  // Check for errors or detected loops
  if (route.route == NULL) {
    printf("<out> <command> <ERROR> Cannot send command since source routing path cannot be created (loop detected or missing info)!\n");
    return 0;
  }

  // Ok, build routing path array

  // Create path array (-1 to exclude first node from path -> sink will directly send packet to first node)
  uint8_t path_length = route.length - 1;
  linkaddr_t path[path_length];
  // First node to which the sink will send the packet
  linkaddr_t next_node = route.route[0];

  // Fill routing path excluding first node
  int i = 1;
  for (i = 1; i < path_length; i++) {
    path[i] = route.route[i];
  }

  // Route is no more needed
  free(route.route);

  // Update path length in header
  hdr.path_length = path_length;

  // Try to allocate space for header
  int alloc_res = packetbuf_hdralloc(sizeof(struct collect_header) + (sizeof(linkaddr_t) * path_length)); // header + path array

  if (alloc_res == 0) { // Allocation failed -> report error
    printf("<out> <command> <ERROR> Trying to send a command packet but node fails allocating header buffer!\n");
    return 0;
  }

  // Add header to packet
  memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct collect_header));
  // Add current node to path array after the header
  memcpy(packetbuf_hdrptr() + sizeof(struct collect_header), &path, sizeof(linkaddr_t) * path_length);
  // Send packet to next node and report success

  // TODO add \n
  printf("<out> <command> Send command packet (dest: %02x:%02x, length: %d route: [", dest->u8[0], dest->u8[1], hdr.path_length);

  // TODO remove log
  int j = 0;
  for (j = 0; j < hdr.path_length; j++) {
    printf("%02x:%02x, ", path[j].u8[0], path[j].u8[1]);
  }

  printf("])\n");

  return unicast_send(&conn->uc, &next_node);
}



/* Sink -------------------------------------------------------------------------------*/

void initialize_sink(struct my_collect_conn* conn) {
    printf("<open> Node is the sink (node: %02x:%02x).\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

    // Sink has 0 as metric
    conn->metric = 0;

    // Initialize routing table
    routing_table_init();

    // Params
    // c	A pointer to the callback timer.
    // t	The interval before the timer expires.
    // f	A function to be called when the timer expires.
    // ptr	An opaque pointer that will be supplied as an argument to the callback function.
    ctimer_set(&conn->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, conn);
}
