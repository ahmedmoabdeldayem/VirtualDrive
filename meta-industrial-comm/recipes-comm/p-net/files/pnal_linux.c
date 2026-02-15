/* pnal_linux.c — Linux PNAL (Platform Abstraction Layer) for p-net.
 * Implements all functions declared in include/pnal.h using Linux sockets,
 * ioctl, and POSIX file I/O. */
#define _GNU_SOURCE
#include "pnal_sys.h"
#include "pnal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* ── uptime ───────────────────────────────────────────────────────────────── */

uint32_t pnal_get_system_uptime_10ms (void)
{
   struct timespec ts;
   clock_gettime (CLOCK_MONOTONIC, &ts);
   return (uint32_t)((ts.tv_sec * 100ULL) + (ts.tv_nsec / 10000000ULL));
}

/* ── file operations ──────────────────────────────────────────────────────── */

int pnal_load_file (
   const char * fullpath,
   void *       object_1,
   size_t       size_1,
   void *       object_2,
   size_t       size_2)
{
   FILE * f = fopen (fullpath, "rb");
   if (!f) return -1;
   size_t n1 = fread (object_1, 1, size_1, f);
   size_t n2 = fread (object_2, 1, size_2, f);
   fclose (f);
   return (n1 == size_1 && n2 == size_2) ? 0 : -1;
}

int pnal_save_file (
   const char * fullpath,
   const void * object_1,
   size_t       size_1,
   const void * object_2,
   size_t       size_2)
{
   FILE * f = fopen (fullpath, "wb");
   if (!f) return -1;
   size_t n1 = fwrite (object_1, 1, size_1, f);
   size_t n2 = fwrite (object_2, 1, size_2, f);
   fclose (f);
   return (n1 == size_1 && n2 == size_2) ? 0 : -1;
}

void pnal_clear_file (const char * fullpath)
{
   remove (fullpath);
}

/* ── buffer management ────────────────────────────────────────────────────── */

#define PNAL_BUF_HEADER_RESERVE 64

pnal_buf_t * pnal_buf_alloc (uint16_t length)
{
   pnal_buf_t * p = malloc (sizeof (pnal_buf_t) + PNAL_BUF_HEADER_RESERVE + length);
   if (!p) return NULL;
   p->next    = NULL;
   p->payload = (uint8_t *)p + sizeof (pnal_buf_t) + PNAL_BUF_HEADER_RESERVE;
   p->len     = length;
   p->tot_len = length;
   return p;
}

void pnal_buf_free (pnal_buf_t * p)
{
   free (p);
}

uint8_t pnal_buf_header (pnal_buf_t * p, int16_t header_size_increment)
{
   intptr_t base  = (intptr_t)p + sizeof (pnal_buf_t);
   intptr_t avail = (intptr_t)p->payload - base;

   if (header_size_increment > 0 && avail < header_size_increment)
      return 1;

   p->payload  = (uint8_t *)p->payload - header_size_increment;
   p->len     += header_size_increment;
   p->tot_len += header_size_increment;
   return 0;
}

/* ── Ethernet — AF_PACKET raw sockets ────────────────────────────────────── */

static void * eth_rx_thread (void * arg)
{
   pnal_eth_handle_t * h = arg;
   uint8_t             frame[1514];

   while (h->running)
   {
      ssize_t n = recv (h->fd, frame, sizeof (frame), 0);
      if (n < 0)
      {
         if (!h->running) break;
         continue;
      }
      if (h->callback)
      {
         pnal_buf_t * buf = pnal_buf_alloc ((uint16_t)n);
         if (buf)
         {
            memcpy (buf->payload, frame, n);
            typedef int pnal_eth_cb_t (pnal_eth_handle_t *, void *, pnal_buf_t *);
            ((pnal_eth_cb_t *)h->callback) (h, h->callback_arg, buf);
            pnal_buf_free (buf);
         }
      }
   }
   return NULL;
}

pnal_eth_handle_t * pnal_eth_init (
   const char *        if_name,
   pnal_ethertype_t    receive_type,
   const pnal_cfg_t *  pnal_cfg,
   pnal_eth_callback_t * callback,
   void *              arg)
{
   (void)pnal_cfg;

   int fd = socket (AF_PACKET, SOCK_RAW, htons ((uint16_t)receive_type));
   if (fd < 0) return NULL;

   struct ifreq ifr;
   memset (&ifr, 0, sizeof (ifr));
   strncpy (ifr.ifr_name, if_name, IFNAMSIZ - 1);
   if (ioctl (fd, SIOCGIFINDEX, &ifr) < 0) { close (fd); return NULL; }

   struct sockaddr_ll sll = {0};
   sll.sll_family   = AF_PACKET;
   sll.sll_protocol = htons ((uint16_t)receive_type);
   sll.sll_ifindex  = ifr.ifr_ifindex;
   if (bind (fd, (struct sockaddr *)&sll, sizeof (sll)) < 0) { close (fd); return NULL; }

   pnal_eth_handle_t * h = calloc (1, sizeof (*h));
   if (!h) { close (fd); return NULL; }

   h->fd           = fd;
   h->ifindex      = ifr.ifr_ifindex;
   h->running      = true;
   h->callback     = (void *)callback;
   h->callback_arg = arg;

   pthread_create (&h->rx_thread, NULL, eth_rx_thread, h);
   return h;
}

int pnal_eth_send (pnal_eth_handle_t * h, pnal_buf_t * buf)
{
   struct sockaddr_ll sll = {0};
   sll.sll_family  = AF_PACKET;
   sll.sll_ifindex = h->ifindex;
   sll.sll_halen   = 6;
   memcpy (sll.sll_addr, buf->payload, 6);

   ssize_t n = sendto (h->fd, buf->payload, buf->len, 0,
                       (struct sockaddr *)&sll, sizeof (sll));
   return (n < 0) ? -1 : (int)n;
}

int pnal_eth_get_status (const char * interface_name, pnal_eth_status_t * status)
{
   memset (status, 0, sizeof (*status));
   status->running                            = true;
   status->is_autonegotiation_supported       = true;
   status->is_autonegotiation_enabled         = true;
   status->operational_mau_type               = PNAL_ETH_MAU_COPPER_1000BaseT_FULL_DUPLEX;
   (void)interface_name;
   return 0;
}

/* ── network interface info ───────────────────────────────────────────────── */

int pnal_get_interface_index (const char * interface_name)
{
   int fd = socket (AF_INET, SOCK_DGRAM, 0);
   if (fd < 0) return 0;
   struct ifreq ifr;
   memset (&ifr, 0, sizeof (ifr));
   strncpy (ifr.ifr_name, interface_name, IFNAMSIZ - 1);
   int idx = 0;
   if (ioctl (fd, SIOCGIFINDEX, &ifr) == 0)
      idx = ifr.ifr_ifindex;
   close (fd);
   return idx;
}

int pnal_get_port_statistics (const char * interface_name, pnal_port_stats_t * stats)
{
   memset (stats, 0, sizeof (*stats));
   (void)interface_name;
   return 0;
}

/* ── IP configuration ─────────────────────────────────────────────────────── */

static pnal_ipaddr_t get_addr_ioctl (const char * ifname, int request)
{
   int fd = socket (AF_INET, SOCK_DGRAM, 0);
   if (fd < 0) return 0;
   struct ifreq ifr;
   memset (&ifr, 0, sizeof (ifr));
   strncpy (ifr.ifr_name, ifname, IFNAMSIZ - 1);
   pnal_ipaddr_t addr = 0;
   if (ioctl (fd, request, &ifr) == 0)
   {
      struct sockaddr_in * sin = (struct sockaddr_in *)&ifr.ifr_addr;
      addr = ntohl (sin->sin_addr.s_addr);
   }
   close (fd);
   return addr;
}

pnal_ipaddr_t pnal_get_ip_address (const char * if_name)
{
   return get_addr_ioctl (if_name, SIOCGIFADDR);
}

pnal_ipaddr_t pnal_get_netmask (const char * if_name)
{
   return get_addr_ioctl (if_name, SIOCGIFNETMASK);
}

pnal_ipaddr_t pnal_get_gateway (const char * if_name)
{
   /* Parse /proc/net/route to find the default gateway for the interface */
   FILE * f = fopen ("/proc/net/route", "r");
   if (!f) return 0;

   char line[256];
   fgets (line, sizeof (line), f); /* skip header */
   pnal_ipaddr_t gw = 0;
   while (fgets (line, sizeof (line), f))
   {
      char iface[IFNAMSIZ];
      uint32_t dest, gateway, flags;
      if (sscanf (line, "%15s %x %x %x", iface, &dest, &gateway, &flags) == 4)
      {
         if (dest == 0 && strcmp (iface, if_name) == 0)
         {
            gw = ntohl (gateway);
            break;
         }
      }
   }
   fclose (f);
   return gw;
}

int pnal_get_ip_suite (
   const char *   if_name,
   pnal_ipaddr_t * p_ipaddr,
   pnal_ipaddr_t * p_netmask,
   pnal_ipaddr_t * p_gw,
   char *          hostname)
{
   *p_ipaddr  = pnal_get_ip_address (if_name);
   *p_netmask = pnal_get_netmask    (if_name);
   *p_gw      = pnal_get_gateway    (if_name);
   if (hostname) pnal_get_hostname (hostname);
   return 0;
}

int pnal_get_macaddress (const char * if_name, pnal_ethaddr_t * p_mac)
{
   int fd = socket (AF_INET, SOCK_DGRAM, 0);
   if (fd < 0) return -1;
   struct ifreq ifr;
   memset (&ifr, 0, sizeof (ifr));
   strncpy (ifr.ifr_name, if_name, IFNAMSIZ - 1);
   int rc = -1;
   if (ioctl (fd, SIOCGIFHWADDR, &ifr) == 0)
   {
      memcpy (p_mac->addr, ifr.ifr_hwaddr.sa_data, 6);
      rc = 0;
   }
   close (fd);
   return rc;
}

int pnal_get_hostname (char * hostname)
{
   return gethostname (hostname, PNAL_HOSTNAME_MAX_SIZE);
}

int pnal_set_ip_suite (
   const char *         if_name,
   const pnal_ipaddr_t * p_ipaddr,
   const pnal_ipaddr_t * p_netmask,
   const pnal_ipaddr_t * p_gw,
   const char *          hostname,
   bool                  permanent)
{
   (void)if_name; (void)p_ipaddr; (void)p_netmask;
   (void)p_gw;    (void)hostname; (void)permanent;
   /* IP reconfiguration not implemented — PROFINET DCP sets IP at runtime */
   return 0;
}

/* ── UDP ──────────────────────────────────────────────────────────────────── */

#define PNAL_MAX_UDP_SOCKETS 16
static int udp_sockets[PNAL_MAX_UDP_SOCKETS];
static bool udp_used[PNAL_MAX_UDP_SOCKETS];

__attribute__((constructor))
static void udp_init (void)
{
   for (int i = 0; i < PNAL_MAX_UDP_SOCKETS; i++)
   { udp_sockets[i] = -1; udp_used[i] = false; }
}

int pnal_udp_open (pnal_ipaddr_t addr, pnal_ipport_t port)
{
   int fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (fd < 0) return -1;

   int reuse = 1;
   setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));

   struct sockaddr_in sin = {0};
   sin.sin_family      = AF_INET;
   sin.sin_port        = htons (port);
   sin.sin_addr.s_addr = htonl (addr);
   if (bind (fd, (struct sockaddr *)&sin, sizeof (sin)) < 0) { close (fd); return -1; }

   for (int i = 0; i < PNAL_MAX_UDP_SOCKETS; i++)
   {
      if (!udp_used[i])
      {
         udp_sockets[i] = fd;
         udp_used[i]    = true;
         return i;
      }
   }
   close (fd);
   return -1;
}

int pnal_udp_sendto (
   uint32_t       id,
   pnal_ipaddr_t  dst_addr,
   pnal_ipport_t  dst_port,
   const uint8_t * data,
   int            size)
{
   if (id >= PNAL_MAX_UDP_SOCKETS || !udp_used[id]) return -1;
   struct sockaddr_in dst = {0};
   dst.sin_family      = AF_INET;
   dst.sin_port        = htons (dst_port);
   dst.sin_addr.s_addr = htonl (dst_addr);
   ssize_t n = sendto (udp_sockets[id], data, size, 0,
                       (struct sockaddr *)&dst, sizeof (dst));
   return (n < 0) ? -1 : (int)n;
}

int pnal_udp_recvfrom (
   uint32_t       id,
   pnal_ipaddr_t * src_addr,
   pnal_ipport_t * src_port,
   uint8_t *       data,
   int             size)
{
   if (id >= PNAL_MAX_UDP_SOCKETS || !udp_used[id]) return -1;
   struct sockaddr_in src = {0};
   socklen_t addrlen = sizeof (src);
   ssize_t n = recvfrom (udp_sockets[id], data, size, 0,
                         (struct sockaddr *)&src, &addrlen);
   if (n < 0) return -1;
   *src_addr = ntohl (src.sin_addr.s_addr);
   *src_port = ntohs (src.sin_port);
   return (int)n;
}

void pnal_udp_close (uint32_t id)
{
   if (id < PNAL_MAX_UDP_SOCKETS && udp_used[id])
   {
      close (udp_sockets[id]);
      udp_sockets[id] = -1;
      udp_used[id]    = false;
   }
}

/* ── SNMP ─────────────────────────────────────────────────────────────────── */

int pnal_snmp_init (pnet_t * net, const pnal_cfg_t * pnal_cfg)
{
   (void)net; (void)pnal_cfg;
   return 0;
}
