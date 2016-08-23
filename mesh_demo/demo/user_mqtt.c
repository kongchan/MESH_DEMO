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
#include "mqtt.h"
#include "debug.h"
#include "user_upgrade.h"

extern bool HttpOK;
extern bool MqttOK;

extern u8 bufMeter[8];
u8 temp[64];

extern u8 mqtt_id[11];

bool MqttConFlag = 0;


void  ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;

	INFO("MQTT: Connected\r\n");

	MqttConFlag = 1;
	LED_ON(LED2);

	MQTT_Subscribe(client, mqtt_id, 0);
	MQTT_Subscribe(client, "/meter/all", 0);

	os_sprintf(temp, "%s connected", mqtt_id);
	MQTT_Publish(client, "/meter/info", temp, strlen(temp), 0, 0);
}

void  ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;

	MqttConFlag = 0;
	LED_OFF(LED2);

	INFO("MQTT: Disconnected\r\n");
}

void ICACHE_FLASH_ATTR  mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}


bool ICACHE_FLASH_ATTR IsStartWith(const char *str1,const char *str2)
{
	while(*str1 == *str2)
	{  
		str1++;
		str2++;
	}
	if(*str1 == '\0')
		return 1;
	else
		return 0;
}


void  ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len + 1);
	char *dataBuf = (char*)os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("MqttReceive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	if(1)
	{

		if(os_strcmp(dataBuf,"ReadData") == 0 )
		{
			INFO("MQTT: Receive MeterData Requestion!!!\r\n");
			MqttOK = 1;
			HttpOK = 1;

			uart0_tx_buffer(bufMeter, 8);
			user_check_ip();
		}

		else if(os_strcmp(dataBuf,"ReadTest") == 0)
		{
			INFO("MQTT: Receive MeterData Requestion!!!\r\n");
			MqttOK = 2;
			uart0_tx_buffer(bufMeter, 8);
		}

		else if(os_strcmp(dataBuf,"ping") == 0)
		{
			os_memset(temp,strlen(temp));
			os_sprintf(temp,"%s_id", mqtt_id);
			MQTT_Publish(client, temp, "OK", 2, 0, 0);
		}

		else if(IsStartWith("upgrade", dataBuf))
		{
			os_sprintf(temp, "%s will Upgrade", mqtt_id);
			MQTT_Publish(client, "/meter/upgreade", temp, strlen(temp), 0, 0);
			FuncUpdate(dataBuf+8);
		}

		else if(os_strcmp(dataBuf,"list") == 0)
		{
			os_sprintf(temp, "%s will Restart", mqtt_id);
			MQTT_Publish(client, "/meter/info", temp, strlen(temp), 0, 0);
		}

		else if (os_strcmp(dataBuf, "restart") == 0)
		{
			MQTT_Publish(client, "/meter/id", mqtt_id, 12, 0, 0);
		}

		else if(os_strcmp(dataBuf, "LED_1_1") == 0)
			LED_ON(LED1);
		else if(os_strcmp(dataBuf, "LED_2_1") == 0)
			LED_ON(LED2);
		else if(os_strcmp(dataBuf, "LED_3_1") == 0)
			LED_ON(LED3);
		else if(os_strcmp(dataBuf, "LED_1_0") == 0)
			LED_OFF(LED1);
		else if(os_strcmp(dataBuf, "LED_2_0") == 0)
			LED_OFF(LED2);
		else if(os_strcmp(dataBuf, "LED_3_0") == 0)
			LED_OFF(LED3);
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

