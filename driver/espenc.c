#include <lwip/ip_addr.h>
#include "espenc.h"
#include "driver/spi.h"
#include "gpio.h"
#include "mem.h"

static uint8_t Enc28j60Bank;

void chipEnable() {
    // Force CS pin low
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);
    GPIO_OUTPUT_SET(15, 0);
}

void chipDisable() {
    // Return to default CS function
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2); //GPIO15 is HSPI CS pin (Chip Select / Slave Select)
}


uint8_t readOp(uint8_t op, uint8_t addr) {
    if(addr & 0x80)
        return (uint8_t) spi_transaction(HSPI, 3, op >> 5, 5, addr, 0, 0, 16, 0) & 0xff; // Ignore dummy first byte
    else
        return (uint8_t) spi_transaction(HSPI, 3, op >> 5, 5, addr, 0, 0, 8, 0);
}

void writeOp(uint8_t op, uint8_t addr, uint8_t data) {
    spi_transaction(HSPI, 3, op >> 5, 5, addr, 8, data, 0, 0);
}

static void SetBank (uint8_t address) {
    if ((address & BANK_MASK) != Enc28j60Bank) {
        writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL1|ECON1_BSEL0);
        Enc28j60Bank = address & BANK_MASK;
        writeOp(ENC28J60_BIT_FIELD_SET, ECON1, Enc28j60Bank>>5);
    }
}

static uint8_t readRegByte (uint8_t address) {
    SetBank(address);
    return readOp(ENC28J60_READ_CTRL_REG, address);
}

static uint16_t readReg(uint8_t address) {
    return readRegByte(address) + (readRegByte(address+1) << 8);
}

static void writeRegByte (uint8_t address, uint8_t data) {
    SetBank(address);
    writeOp(ENC28J60_WRITE_CTRL_REG, address, data);
}

static void writeReg(uint8_t address, uint16_t data) {
    writeRegByte(address, data);
    writeRegByte(address + 1, data >> 8);
}

static uint16_t readPhyByte (uint8_t address) {
    writeRegByte(MIREGADR, address);
    writeRegByte(MICMD, MICMD_MIIRD);
    while (readRegByte(MISTAT) & MISTAT_BUSY)
        ;
    writeRegByte(MICMD, 0x00);
    return readRegByte(MIRD+1);
}

static void writePhy (uint8_t address, uint16_t data) {
    writeRegByte(MIREGADR, address);
    writeReg(MIWR, data);
    while (readRegByte(MISTAT) & MISTAT_BUSY)
        ;
}

static void writeBuf(uint16_t len, const uint8_t* data) {
    // force CS pin here, as it requires multiple bytes to be sent
    chipEnable();
    if (len != 0) {
        spi_transaction(HSPI, 8, ENC28J60_WRITE_BUF_MEM, 0, 0, 8, 0, 0, 0);
        while (len--) {
            uint8_t nextbyte = *data++;

            while(spi_busy(HSPI));	//wait for SPI transaction to complete
            spi_transaction(HSPI, 0, 0, 0, 0, 8, nextbyte, 0, 0);
     	};
        while(spi_busy(HSPI));	//wait for SPI transaction to complete
    }
    chipDisable();
}

err_t enc28j60_link_output(struct netif *netif, struct pbuf *p) {
    uint8_t retry = 0;
    uint16_t len = p->tot_len;
    log("output, tot_len: %d", p->tot_len);
    uint8_t isUp = (readPhyByte(PHSTAT2) >> 2) & 1;
    log("link is up: %d", isUp);

    writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRST);
    writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);
    writeOp(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF|EIR_TXIF);

    if(retry == 0) {
        writeReg(EWRPT, TXSTART_INIT);
        writeReg(ETXND, TXSTART_INIT+len);
        writeOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
        uint8_t* buffer = (uint8_t*) os_malloc(len);
        log("buf: %d", buffer);
        pbuf_copy_partial(p, buffer, p->tot_len, 0);
        int i = 0;
        for(i = 0; i < 16; i++) {
            log("%02x", buffer[i]);
        }
        writeBuf(len, buffer);
        os_free(buffer);
    }

    writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);

    uint16_t count = 0;
    while ((readRegByte(EIR) & (EIR_TXIF | EIR_TXERIF)) == 0 && ++count < 1000U)
        ;

    if (!(readRegByte(EIR) & EIR_TXERIF) && count < 1000U) {
        // no error; start new transmission
        log("transmission success");
    } else {
        log("transmission failed");
    }

    writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
}

// http://lwip.wikia.com/wiki/Writing_a_device_driver
err_t enc28j60_init(struct netif *netif) {
    log("initializing");
    netif->linkoutput = enc28j60_link_output;
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->mtu = 1500;
    netif->hwaddr_len = 6;
    netif->hwaddr[0] = 0x00;
    netif->hwaddr[1] = 0x11;
    netif->hwaddr[2] = 0x22;
    netif->hwaddr[3] = 0x33;
    netif->hwaddr[4] = 0x44;
    netif->hwaddr[5] = 0x55;

    netif->output = etharp_output;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    log("initializing hardware");
    spi_init(HSPI);
    spi_mode(HSPI, 0, 0);
    writeOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);

    uint8_t estat;
    while(!(estat = readOp(ENC28J60_READ_CTRL_REG, ESTAT)) & ESTAT_CLKRDY) {
        log("estat: %02x", estat);
        os_delay_us(2000); // errata B7/2
    }

    writeReg(ERXST, RXSTART_INIT);
    writeReg(ERXRDPT, RXSTART_INIT);
    writeReg(ERXND, RXSTOP_INIT);
    writeReg(ETXST, TXSTART_INIT);
    writeReg(ETXND, TXSTOP_INIT);

    writeRegByte(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN|ERXFCON_BCEN);
    writeReg(EPMM0, 0x303f);
    writeReg(EPMCS, 0xf7f9);
    writeRegByte(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
    writeRegByte(MACON2, 0x00);
    writeOp(ENC28J60_BIT_FIELD_SET, MACON3,
            MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
    writeReg(MAIPG, 0x0C12);
    writeRegByte(MABBIPG, 0x12);
    writeReg(MAMXFL, MAX_FRAMELEN);
    writeRegByte(MAADR5, netif->hwaddr[0]);
    writeRegByte(MAADR4, netif->hwaddr[1]);
    writeRegByte(MAADR3, netif->hwaddr[2]);
    writeRegByte(MAADR2, netif->hwaddr[3]);
    writeRegByte(MAADR1, netif->hwaddr[4]);
    writeRegByte(MAADR0, netif->hwaddr[5]);
    writePhy(PHCON2, PHCON2_HDLDIS);
    SetBank(ECON1);
    writeOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE);
    writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

    uint8_t rev = readRegByte(EREVID);
    // microchip forgot to step the number on the silcon when they
    // released the revision B7. 6 is now rev B7. We still have
    // to see what they do when they release B8. At the moment
    // there is no B8 out yet
    if (rev > 5) ++rev;
    log("hardware ready, rev: %d", rev);
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
