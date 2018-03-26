#include <stdbool.h>
#include "core/net/linkaddr.h"
#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include "my_routing_table.h"


/* Routing table vars -----------------------------------------------------------------*/

int routing_table_size = 255;
struct routing_table_entry* *routing_table = NULL;


/* Routing table functions ------------------------------------------------------------*/

void routing_table_init() {

 // TODO how much allocate?
  routing_table =  (struct routing_table_entry**) malloc(routing_table_size);

  // Check if success
  if (routing_table == NULL) {
    printf("<routing_table> Fail to allocate routing table\n");
    exit(-1);
  }

  // Init all entries to NULL
  int i = 0;
  for (i = 0; i < routing_table_size; i++) {
    routing_table[i] = NULL;
  }

}

struct routing_table_entry** routing_table_get() {
  return routing_table;
}


linkaddr_t routing_table_get_parent(const linkaddr_t node) {

  // Calc index in routing table
  uint16_t index = node.u16;
  struct routing_table_entry* entry = routing_table[index];
  if (entry == NULL) {
    return linkaddr_null;
  } else {
    return entry->parent;
  }
}


void routing_table_update_entry(const linkaddr_t *parent, const linkaddr_t *child) {

  printf("<routing_table> Updating table with <parent: %02x:%02x, child: %02x:%02x>\n",
    parent->u8[0], parent->u8[1], child->u8[0], child->u8[1]);

  // Calc index in routing table
  uint16_t index = child->u16;

  // Get current entry
  struct routing_table_entry* current_entry = routing_table[index];

  if (current_entry == NULL) {
    // Initialize entry

    // Allocate new space for the entry
    struct routing_table_entry* new_entry = (struct routing_table_entry*) malloc(sizeof(struct routing_table_entry));

    // Check if success
    if (new_entry == NULL) {
      printf("<routing_table> Fail to allocate space for a new entry\n");
      exit(-1);
    }

    // Set values
    new_entry->parent = *parent;
    new_entry->child = *child;

    // Set pointer
    routing_table[index] = new_entry;
    return; // Child inserted -> nothing more to do

  } else { // Update the existing entry replacing the parent
    current_entry->parent = *parent;
  }
}


struct source_route routing_table_find_route_path(const linkaddr_t *dest) {
  printf("<routing_table> <find_route> Search route for %02x:%02x\n", dest->u8[0], dest->u8[1]);

  // Init route with dest node
  linkaddr_t* route = (linkaddr_t*) malloc(sizeof(linkaddr_t));
  int route_length = 1;
  route[0] = *dest;

    if (route == NULL) {
      printf("<routing_table> <find_route> Fail to allocate space for route\n");
      exit(-1);
    }

  // Current interaction destination node
  linkaddr_t current_node = *dest;

  // Search a path while route is not arrived to sink (current dest node is sink)
  while(!linkaddr_cmp(&linkaddr_node_addr, &current_node)) {
    // Get the parent node of the current dest
    linkaddr_t parent_node = routing_table_get_parent(current_node);

    // Check of parent exists
    if (linkaddr_cmp(&parent_node, &linkaddr_null)) {
      printf("<routing_table> <find_route> Fail to create route. Parent of %02x:%02x is missing\n", current_node.u8[0], current_node.u8[1]);
      return (struct source_route) {.route = NULL, .length = 0}; // Parent does not exists -> route is incomplete and cannot be created
    }

    // Parent found -> add current node to route array and proceed to next iteration
    printf("<routing_table> <find_route> Found parent of %02x:%02x. It is %02x:%02x\n",
       current_node.u8[0], current_node.u8[1], parent_node.u8[0], parent_node.u8[1]);
    route = route_add_node(route, route_length, parent_node);

    if (route == NULL) {
      printf("<routing_table> <find_route> Fail to reallocate space for route\n");
      exit(-1);
    }

    // Update route length
    route_length++;

    if (check_loop_presence(route, route_length, parent_node) > 1) { // Loop found
      printf("<routing_table> <find_route> Fail to create route. Loop has been detected\n");
      return (struct source_route) {.route = NULL, .length = 0}; // Loop found in route -> route cannot be used
    }

    // Update next dest
    current_node = parent_node;

  }

  if (linkaddr_cmp(&linkaddr_node_addr, &current_node)) { // Arrived to sink
    // Route (from child to sink) is complete -> reverse it and remove sink

    // Alloc space for new array
    int result_length = route_length - 1; // -1 exclude sink node from array (the last node inserted to route tail)
    linkaddr_t* result = (linkaddr_t*) malloc(sizeof(linkaddr_t) * result_length);

    // Reverse the array
    int i = result_length - 1;
    for (i = result_length - 1; i >= 0; i--) {
      result[(result_length - 1) - i] = route[i];
    }

    printf("<routing_table> <find_route> Complete route found\n");

    return (struct source_route) {.route = result, .length = result_length};

  } else {
    printf("<routing_table> <find_route> Route search complete but not start from sink\n");
    return (struct source_route) {.route = NULL, .length = 0}; // Should not happen
  }

}

int check_loop_presence(linkaddr_t *route, int length, linkaddr_t node) {
  int i = 0;
  int count = 0;

  for (i = 0; i < length; i++) {
    if (linkaddr_cmp(&route[i], &node)) {
      count++;
    }
  }

  return count;
};


linkaddr_t* route_add_node(linkaddr_t* array, int length, linkaddr_t node) {

  // Create new array
  linkaddr_t* new_array = (linkaddr_t*) malloc(sizeof(linkaddr_t) * (length + 1));

  // Check success
  if (new_array == NULL) {
    return NULL;
  }

  // Copy entries of old array into new one
  int i = 0;
  for (i = 0; i < length; i++) {
    new_array[i] = array[i];
  }
  // Add new node on tail
  new_array[length] = node;

  // Free space
  free(array);

  // Return new array
  return new_array;
}
