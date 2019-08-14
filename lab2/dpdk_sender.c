#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

/*
 *  Packet form:
 *  ether_hdr | ip_hdr | udp_hdr | data |
 */

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_rx_pkt_len = ETHER_MAX_LEN}};

static void construct_packet(char *pkt, struct ether_addr d_addr,
                             struct ether_addr s_addr,
                             uint16_t udp_total_length,
                             uint32_t src_ip,
                             uint32_t dst_ip,
                             uint16_t udp_src_port,
                             uint16_t udp_dst_port,
                             uint32_t data_seq)
{
  struct ether_hdr *ether_header = (struct ether_hdr *)pkt;
  ether_addr_copy(&d_addr, &ether_header->d_addr);
  ether_addr_copy(&s_addr, &ether_header->s_addr);
  ether_header->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
  // printf("ether_header finished\n");

  struct ipv4_hdr *ipv4_header = (struct ipv4_hdr *)(ether_header + 1);
  ipv4_header->version_ihl = 0x45;
  ipv4_header->total_length = rte_cpu_to_be_16(udp_total_length + sizeof(struct ipv4_hdr));
  ipv4_header->next_proto_id = 0x11;
  ipv4_header->dst_addr = rte_cpu_to_be_32(dst_ip);
  ipv4_header->src_addr = rte_cpu_to_be_32(src_ip);
  // ipv4_header->dst_addr = 0x0650a8c0;
  // ipv4_header->src_addr = 0x0a50a8c0;
  ipv4_header->time_to_live = 64;
  ipv4_header->packet_id = 0;
  ipv4_header->fragment_offset = 0;
  ipv4_header->type_of_service = 0;
  ipv4_header->hdr_checksum = rte_ipv4_cksum(ipv4_header);
  // printf("ipv4_header finished\n");

  struct udp_hdr *udp_header = (struct udp_hdr *)(ipv4_header + 1);
  udp_header->src_port = rte_cpu_to_be_16(udp_src_port);
  udp_header->dst_port = rte_cpu_to_be_16(udp_dst_port);
  udp_header->dgram_len = rte_cpu_to_be_16(udp_total_length);
  udp_header->dgram_cksum = 0; // for simplicity
  // printf("udp_header finished\n");

  uint32_t *data = (uint32_t *)(udp_header + 1);
  *data = rte_cpu_to_be_32(data_seq);
}

/* basicfwd.c: Basic DPDK skeleton forwarding example. */
/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  int retval;
  uint16_t q;

  if (port >= rte_eth_dev_count())
    return -1;

  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (retval != 0)
    return retval;

  /* Allocate and set up 1 RX queue per Ethernet port. */
  for (q = 0; q < rx_rings; q++)
  {
    retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                                    rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0)
      return retval;
  }

  /* Allocate and set up 1 TX queue per Ethernet port. */
  for (q = 0; q < tx_rings; q++)
  {
    retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                                    rte_eth_dev_socket_id(port), NULL);
    if (retval < 0)
      return retval;
  }

  /* Start the Ethernet port. */
  retval = rte_eth_dev_start(port);
  if (retval < 0)
    return retval;

  /* Display the port MAC address. */
  struct ether_addr addr;
  rte_eth_macaddr_get(port, &addr);
  printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
         " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
         (unsigned)port,
         addr.addr_bytes[0], addr.addr_bytes[1],
         addr.addr_bytes[2], addr.addr_bytes[3],
         addr.addr_bytes[4], addr.addr_bytes[5]);
  /* Enable RX in promiscuous mode for the Ethernet device. */
  rte_eth_promiscuous_enable(port);

  return 0;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[])
{
  struct rte_mempool *mbuf_pool;
  // unsigned nb_ports;
  uint8_t portid;
  /* Initialize the Environment Abstraction Layer (EAL). */
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
  {
    rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    printf("ret < 0!\n");
  }
  argc -= ret;
  argv += ret;
  /* Check that there is an even number of ports to send/receive on. */
  // nb_ports = rte_eth_dev_count();
  // if (nb_ports < 2 || (nb_ports & 1))
  //   rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");
  /* Creates a new mempool in memory to hold the mbufs. */
  mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                      MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (mbuf_pool == NULL)
  {
    printf("NULL!\n");
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
  }
  /* Initialize all ports. */
  // for (portid = 0; portid < nb_ports; portid++)
  portid = 0;
  if (port_init(portid, mbuf_pool) != 0)
    rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu8 "\n",
             portid);
  if (rte_lcore_count() > 1)
    printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");
  /* Call lcore_main on the master core only. */
  // lcore_main(mbuf_pool);

  const uint8_t nb_ports = rte_eth_dev_count();
  uint8_t port = 0;

  /*
   * Check that the port is on the same NUMA node as the polling thread
   * for best performance.
   */

  for (port = 0; port < nb_ports; port++)
    if (rte_eth_dev_socket_id(port) > 0 &&
        rte_eth_dev_socket_id(port) !=
            (int)rte_socket_id())
      printf("WARNING, port %u is on remote NUMA node to "
             "polling thread.\n\tPerformance will "
             "not be optimal.\n",
             port);
  printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
         rte_lcore_id());

  /* Link encap:Ethernet  HWaddr 00:0c:29:e5:df:d6  
     inet addr:192.168.80.10  Bcast:192.168.80.255  Mask:255.255.255.0
    */
  struct rte_mbuf *bufs[BURST_SIZE];
  // int retval = rte_pktmbuf_alloc_bulk(mbuf_pool, bufs, BURST_SIZE);
  // if (retval != 0)
  //   printf("pktmbufs alloc failed\n");

  struct ether_addr dst_mac;
  struct ether_addr src_mac;
  dst_mac.addr_bytes[0] = 0x00;
  dst_mac.addr_bytes[1] = 0x50;
  dst_mac.addr_bytes[2] = 0x56;
  dst_mac.addr_bytes[3] = 0xC0;
  dst_mac.addr_bytes[4] = 0x00;
  dst_mac.addr_bytes[5] = 0x02;
  // dst_mac = {{0x02, 0x00, 0xC0, 0x56, 0x50, 0x00}};
  rte_eth_macaddr_get(port, &src_mac);

  uint32_t src_ip = IPv4(192, 168, 80, 10);
  uint32_t dst_ip = IPv4(192, 168, 80, 6);

  uint16_t src_port = 2333;
  uint16_t dst_port = 2333;

  uint16_t udp_total_length = 64 + 8;

  for (int i = 0; i < BURST_SIZE; i++)
  {
    // printf("i:%d\n", i);
    bufs[i] = rte_pktmbuf_alloc(mbuf_pool);
    if (!bufs[i])
    {
      rte_exit(EXIT_FAILURE, "Cannot alloc mbuf\n");
    }

    char *packet = rte_pktmbuf_mtod(bufs[i], char *);
    rte_pktmbuf_append(bufs[i], sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + 72);
    construct_packet(packet, dst_mac, src_mac,
                     udp_total_length, src_ip, dst_ip, src_port, dst_port, i);
  }

  printf("burst start!\n");
  const uint16_t nb_tx = rte_eth_tx_burst(0, 0,
                                          bufs, BURST_SIZE);

  printf("burst finished!Send packet:%d\n", nb_tx);
  /* Free any unsent packets. */
  if (unlikely(nb_tx < BURST_SIZE))
  {
    uint16_t buf;
    for (buf = nb_tx; buf < BURST_SIZE; buf++)
      rte_pktmbuf_free(bufs[buf]);
  }
  printf("free!\n");
  return 0;
}