/* pnal_config.h — Linux platform configuration for p-net PNAL.
 * Included early by pnet_api.h so all types here are available
 * before the API structs are declared. */
#pragma once

#include <stdint.h>

/* Maximum Ethernet frame payload size (excluding FCS) */
#define PNAL_BUF_MAX_SIZE       1522U

/* UDP receive buffer depth */
#define PNAL_UDP_MAX_RCV_BUF    4096U

/* PNAL configuration struct — used as a field in pnet_cfg_t (pnet_api.h:1394).
 * Must be defined here because pnet_api.h includes this file before pnal_sys.h. */
typedef struct pnal_cfg
{
   int dummy;
} pnal_cfg_t;
