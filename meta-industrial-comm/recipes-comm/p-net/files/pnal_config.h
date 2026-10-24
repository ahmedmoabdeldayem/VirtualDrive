/* pnal_config.h — Linux platform configuration for p-net PNAL */
#pragma once

#include <stdint.h>

/* Maximum Ethernet frame payload size (excluding FCS) */
#define PNAL_BUF_MAX_SIZE       1522U

/* UDP receive buffer depth */
#define PNAL_UDP_MAX_RCV_BUF    4096U
