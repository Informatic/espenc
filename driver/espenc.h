#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <netif/etharp.h>

err_t enc28j60_link_output(struct netif *netif, struct pbuf *p);
err_t enc28j60_init(struct netif *netif);
void espenc_init();

#define log(s, ...) os_printf ("[%s:%s:%d] " s "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
