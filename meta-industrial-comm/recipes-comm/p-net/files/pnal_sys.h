/* pnal_sys.h — Linux platform type definitions for p-net PNAL.
 * Included by pnal.h via #include "pnal_sys.h".
 * Defines the opaque/OS-specific types that pnal.h forward-declares. */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/* Packet buffer — payload immediately follows the struct in the same allocation */
typedef struct pnal_buf
{
   struct pnal_buf * next;
   void *            payload;
   uint16_t          len;
   uint16_t          tot_len;
} pnal_buf_t;

/* pnal_cfg_t is defined in pnal_config.h (included earlier via pnet_api.h) */

/* Forward-declared in pnal.h — completed here */
struct pnal_eth_handle
{
   int        fd;
   int        ifindex;
   pthread_t  rx_thread;
   volatile bool running;
   void *     callback;     /* pnal_eth_callback_t * — void* avoids circular dep */
   void *     callback_arg;
};
