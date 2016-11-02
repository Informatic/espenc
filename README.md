espenc
======

This is a (relatively) proper implementation of enc28j60 driver for LWIP on
ESP8266.

Both receive and transmit works properly now, but it hasn't been tested much
yet. Code needs some major refactoring.

Usage
-----
1. Copy or symlink `driver/espenc.c`, `driver/espenc.h`, `driver/spi.c`,
   `driver/spi_register.h` and `driver/spi.h` somewhere to your project.
2. Add `espenc_init()` in your `user_init()`
4. Connect ENC28J60 to ESP's SPI, connect INT pin to GPIO5 (or change it for
   anything you like in `driver/spi.h`.
3. PROFIT.

TODO
----
 * Move any framework specific code to separate headers
 * Port & test on esp8266-arduino
 * Port & test on Sming

License
-------

As enc28j60 handling code is mostly based on Ethercard, this shall be GPLv2.
