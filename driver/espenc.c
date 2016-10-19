#include <lwip/ip_addr.h>
#include "espenc.h"

err_t enc28j60_link_output(struct netif *netif, struct pbuf *p) {
    log("output, tot_len: %d", p->tot_len);
}

// http://lwip.wikia.com/wiki/Writing_a_device_driver
err_t enc28j60_init(struct netif *netif) {
    log("initializing");
    netif->linkoutput = enc28j60_link_output;
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->mtu = 1500;
    netif->hwaddr_len = 6;
    netif->hwaddr[0] = 0x01;

    netif->output = etharp_output;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
    return ERR_OK;
}

struct netif enc_netif;
ip_addr_t ipaddr;

void espenc_init() {
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);

    struct netif* new_netif = netif_add(&enc_netif, &ipaddr, &ipaddr, &ipaddr, NULL, enc28j60_init,
                                    ethernet_input);
    log("new_netif: %d", new_netif);

    if(new_netif == NULL) {
        // failed?
        return;
    }

    struct netif* n = netif_list;
    while(n) {
        log("network: %d %d; up: %d", n->name[0], n->name[1], netif_is_up(n));
        n = n->next;
    }

    log("etharp: %d", (new_netif->flags & NETIF_FLAG_ETHARP));
    log("dhcp_start(): %d", dhcp_start(new_netif));
}
