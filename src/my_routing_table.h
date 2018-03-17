#ifndef MY_ROUTING_TABLE_H
#define MY_ROUTING_TABLE_H

#include <stdbool.h>
#include "contiki.h"
#include "core/net/linkaddr.h"


/* Routing table structs --------------------------------------------------------------*/


struct routing_table_entry {
  linkaddr_t parent;
  linkaddr_t child;
};

/**
 * Route from sink (not contained into route) to a destination node.
 * NB: the route is allocated dinamically -> call free() once it is no more useful.
 */
struct source_route {
  linkaddr_t* route;
  int length;
};


/* Routing table functions ------------------------------------------------------------*/


/**
 * Initialize the routing table.
 *
 */
void routing_table_init();

/**
 * Get routing table.
 *
 */
struct routing_table_entry* routing_table_get();


/**
 * Update an entry of the routing table replacing
 * the parent of a child node in the <parent, child> pair.
 *
 */
void routing_table_update_entry(const linkaddr_t *parent, const linkaddr_t *child);

/**
 * Return the rounting table parent entry for a child.
 * Input node is the child with respect to the <parent, child> entries saved in the routing table.
 * Return "linkaddr_null" if node does not exist.
 *
 */
linkaddr_t routing_table_get_parent(const linkaddr_t node);

/**
 * Find a routing path to send a "command" packet (from sink to a destination node).
 *
 * Return struct with NULL route if route not exists (cannot be created) or loop is detected.
 */
struct source_route routing_table_find_route_path(const linkaddr_t *dest);

/**
 * Check provided new_node is present more than once in the route array.
 * In this case, a loop is detected in route.
 *
 * Inputs:
 *   route:    the route array to check
 *   length:   length of route array to check
 *   new_node: the last node inserted into route
 *
 * Return true if loop is found, false otherwise.
 *
 */
bool check_loop_presence(const linkaddr_t *route, int length, linkaddr_t new_node);

#endif  // MY_ROUTING_TABLE_H
