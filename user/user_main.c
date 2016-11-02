#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "user_config.h"
#include "espenc.h"


void user_rf_pre_init( void ) { }

void user_init( void )
{
    static struct station_config config;

    uart_div_modify( 0, UART_CLK_FREQ / ( 115200 ) );
    log("hey?");

    espenc_init();

    //wifi_station_set_hostname( "dweet" );
    wifi_set_opmode_current( STATION_MODE );
}

