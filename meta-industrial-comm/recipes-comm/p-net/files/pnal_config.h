/* pnal_config.h — Linux platform configuration for p-net PNAL.
 * Included early by pnet_api.h so all types here are available
 * before the API structs are declared. */
#pragma once

#include <stddef.h>
#include <stdint.h>

/* Maximum Ethernet frame payload size (excluding FCS) */
#define PNAL_BUF_MAX_SIZE       1522U

/* UDP receive buffer depth */
#define PNAL_UDP_MAX_RCV_BUF    4096U

/* Thread configuration sub-struct used inside pnal_cfg_t */
typedef struct pnal_thread_cfg
{
   uint32_t prio;
   size_t   stack_size;
} pnal_thread_cfg_t;

/* PNAL configuration struct — used as a field in pnet_cfg_t (pnet_api.h:1394).
 * Must be defined here because pnet_api.h includes this file before pnal_sys.h.
 * Fields discovered by reading p-net source files that access pnal_cfg members. */
typedef struct pnal_cfg
{
   pnal_thread_cfg_t bg_worker_thread;
   pnal_thread_cfg_t eth_recv_thread;
   pnal_thread_cfg_t main_thread;
} pnal_cfg_t;
