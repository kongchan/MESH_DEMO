#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"
#include "c_types.h"
#include "gpio.h"
#include "spi_flash.h"
#include "smartconfig.h"
#include "ip_addr.h"

void  ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args);

void  ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args);

void ICACHE_FLASH_ATTR  mqttPublishedCb(uint32_t *args);

void  ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len);
