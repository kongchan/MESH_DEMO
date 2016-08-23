#ifndef __USER_MAIN__H
#define __USER_MAIN__H

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
#include "user_mqtt.h"
#include "gpio16.h"

#include "mqtt.h"
#include "debug.h"

#include "V9881.h"

#include "tcpclient.h"

#include "http_post.h"

#include "user_key.h"

#include "user_time.h"

void  ICACHE_FLASH_ATTR TaskMeter(u8 *buf, u16 len);
void  ICACHE_FLASH_ATTR  FuncSer2Net();//串口转网络
void ICACHE_FLASH_ATTR  FunMqttStart(MQTT_Client *mqttClient, uint8_t *host);
void  ICACHE_FLASH_ATTR UdpFunc();
void ICACHE_FLASH_ATTR FuncWifiLed();
void ICACHE_FLASH_ATTR  wifi_handle_event_cb(System_Event_t *evt);
void ICACHE_FLASH_ATTR mqtt_user_init(void);

#endif