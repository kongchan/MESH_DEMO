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


struct espconn user_tcp_conn;


#define  MaxRam 2*1024+4//最大缓存空间


os_timer_t tcp_timer;//串口转网络定时器
os_timer_t udp_timer;//UDP广播定时器

u8 bufTemp[MaxRam];//串口数据缓存空间
u32 num = 0;//串口数据大小

u32 cpu_id;

u8 mqtt_id[11];
u8 mqtt_topic_send[12];

MQTT_Client *mqttClient;


V9881_Data *v9881_data;

extern bool MqttConFlag;

struct softap_config soft_config;
struct station_config sta_config;

u8 temp[256];

struct ip_info IP_Info;



u8 bufMeter[8] = { 0xFE,0x01,0x0F,0x08,0x00,0x00,0x00,0x1C };

bool HttpOK = 0;
bool MqttOK = 0;

uint8 TcpConnectOK = 0;

void  ICACHE_FLASH_ATTR TaskMeter(u8 *buf, u16 len)
{

	V9881_ReadData(v9881_data, buf, num);

	if(MqttOK)
	{
		MQTT_Publish(mqttClient, mqtt_topic_send, v9881_data->jsonString, strlen(v9881_data->jsonString), 0, 0);

		if (MqttOK == 2)
		{
			MQTT_Publish(mqttClient, "/meter/test", v9881_data->jsonString, strlen(v9881_data->jsonString), 0, 0);
		}
		else if (MqttOK == 1)
		{
			MQTT_Publish(mqttClient, "/meter/data", v9881_data->jsonString, strlen(v9881_data->jsonString), 0, 0);
		}
		MqttOK = 0;
	}

	if(HttpOK)
	{
		getPOST(temp, v9881_data->jsonString, HttpOK);
		os_printf("HttpPost:%s\n", temp);

		if(TcpConnectOK)
		{
			espconn_send(&user_tcp_conn, temp, strlen(temp));
		}
		else
		{
			os_printf("Tcp not Connected!!!\r\n");
		}

		HttpOK = 0;
	}

	num = 0;
}


void  ICACHE_FLASH_ATTR  FuncSer2Net()//串口转网络
{
	os_printf("************ into %s ***********\n", __func__);
	if (num > 0)
	{
		if (bufTemp[0] == 0x35 && bufTemp[1] == 0x35 && bufTemp[2] == 0x35)
		{
			TaskSet(bufTemp, num);
		}
		else if (bufTemp[0] == 0xFE)
		{
			TaskMeter(bufTemp, num);
		}
	}
}

void ICACHE_FLASH_ATTR  FunMqttStart(MQTT_Client *mqttClient, uint8_t *host)
{
	MQTT_InitConnection(mqttClient, host, MQTT_PORT, DEFAULT_SECURITY);
	//MQTT_InitConnection(&mqttClient, "192.168.11.122", 1880, 0);

	MQTT_InitClient(mqttClient, mqtt_id, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, 1);
	//MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);


	MQTT_InitLWT(mqttClient, "/lwt", mqtt_id, 0, 0);

	MQTT_OnConnected(mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(mqttClient, mqttPublishedCb);
	MQTT_OnData(mqttClient, mqttDataCb);
	MQTT_Connect(mqttClient);
}

//UDP定时任务
void  ICACHE_FLASH_ATTR UdpFunc()
{
	//if (!MqttConFlag)
	//{
	//	os_printf("%d will Restart", system_get_time()/M106);
	//	if (system_get_time() > 0x1312D00)
	//	{
	//		os_printf("%d Restart", cpu_id);
	//		system_restart();
	//		return;
	//	}
	//}

	//if(system_get_time() > 0xD693A400)
	//{
	//	//每1小时重启一次
	//	os_printf("%d Restart", cpu_id);
	//	system_restart();
	//	return ;
	//}
	//os_printf("************ into %s ***********\n", __func__);

	uart0_tx_buffer(bufMeter, 8);
}

void ICACHE_FLASH_ATTR FuncWifiLed()
{
	static bool upFlag;

	upFlag = 1 - upFlag;
	if (upFlag)
	{
		LED_ON(LED1);
	}
	else
	{
		LED_OFF(LED1);
	}
}

void ICACHE_FLASH_ATTR  wifi_handle_event_cb(System_Event_t *evt)
{
	static os_timer_t wifi_led_timer;

	static u8 MaxTimes;

	switch (evt->event) 
	{
	case EVENT_STAMODE_CONNECTED:
		os_printf("connect to ssid %s, channel %d\n",
			evt->event_info.connected.ssid,
			evt->event_info.connected.channel);
		break;


	case EVENT_STAMODE_DISCONNECTED:
		{
			os_printf("disconnect from ssid %s, reason %d\n",
				evt->event_info.disconnected.ssid,
				evt->event_info.disconnected.reason);

			LED_OFF(LED1);
			LED_OFF(LED2);
			LED_OFF(LED3);

			if (evt->event_info.disconnected.reason == 5)
			{
				MaxTimes++;

				if (MaxTimes > 20)
				{
					system_restart();
					return;
				}

				os_timer_disarm(&wifi_led_timer);
				os_timer_setfn(&wifi_led_timer, (os_timer_func_t *)FuncWifiLed, NULL);
				os_timer_arm(&wifi_led_timer, 100, 1);
			}

			break;
		}


	case EVENT_STAMODE_AUTHMODE_CHANGE:
		os_printf("mode: %d -> %d\n",
			evt->event_info.auth_change.old_mode,
			evt->event_info.auth_change.new_mode);
		break;


	case EVENT_STAMODE_GOT_IP:
		{
			LED_ON(LED1);
			
			os_timer_disarm(&wifi_led_timer);

			FunMqttStart(mqttClient, MQTT_HOST);
			user_check_ip();
			user_time_init();
			os_printf("************ got IP successful ****************\n");
			break;
		}

	case EVENT_SOFTAPMODE_STACONNECTED:
		os_printf("station: " MACSTR "join, AID = %d\n",
			MAC2STR(evt->event_info.sta_connected.mac),
			evt->event_info.sta_connected.aid);
		break;  
	case EVENT_SOFTAPMODE_STADISCONNECTED:
		os_printf("station: " MACSTR "leave, AID = %d\n",
			MAC2STR(evt->event_info.sta_disconnected.mac),
			evt->event_info.sta_disconnected.aid);
		break;


	default:
		break;
	}
}

void ICACHE_FLASH_ATTR mqtt_user_init(void)
{

	gpio16_output_conf();
	gpio16_output_set(1);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U , FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U , FUNC_GPIO0);

	wifi_set_event_handler_cb(wifi_handle_event_cb);
	user_key_start();

	uart_init_2(9600, 115200);//设置串口波特率

	mqttClient = (MQTT_Client *)os_zalloc(sizeof(MQTT_Client));

	v9881_data = (V9881_Data *)os_zalloc(sizeof(V9881_Data));
	v9881_data->jsonString=(u8 *)os_zalloc(128);

	os_printf("User app compile time=%s\n",__TIME__);

	cpu_id = system_get_chip_id();

	os_bzero(temp, 12);
	os_sprintf(temp, "%d", cpu_id);

	os_memset(mqtt_id, 0x30, 10);
	mqtt_id[10] = 0;

	os_sprintf(mqtt_id + (10 - strlen(temp)), "%d", cpu_id);

	os_bzero(mqtt_topic_send, 12);
	os_sprintf(mqtt_topic_send, "%s_", mqtt_id);

	os_printf("CPU_ID: %s, SendTopic:%s\r\n", mqtt_id, mqtt_topic_send);

	wifi_softap_get_config(&soft_config);

	if(soft_config.ssid[0] > 57)        //名称已非数字开头，初始化为数字
	{

		os_printf("sta_config reset\n", system_upgrade_userbin_check());

		wifi_set_opmode(3);

		os_memset(sta_config.ssid, 0, 32);
		os_memset(sta_config.password, 0, 64);

		strcpy(sta_config.ssid, STA_SSID);
		strcpy(sta_config.password, STA_PASS);

		sta_config.bssid_set = 0;

		wifi_station_disconnect();
		wifi_station_set_config(&sta_config);
		wifi_station_connect();

		memset(soft_config.ssid, 0, sizeof(soft_config.ssid));
		strcpy(soft_config.ssid, mqtt_id);

		memset(soft_config.password, 0, sizeof(soft_config.password));
		strcpy(soft_config.password, STA_PASS);

		soft_config.max_connection = 4;

		soft_config.authmode = AUTH_WPA2_PSK;
		wifi_softap_set_config(&soft_config);


	}

	os_printf("user.bin: %d\r\n", system_upgrade_userbin_check());
	
	os_timer_disarm(&tcp_timer);
	os_timer_setfn(&tcp_timer, (os_timer_func_t *)FuncSer2Net, NULL);//串口转网络任务

	os_timer_disarm(&udp_timer);
	os_timer_setfn(&udp_timer, (os_timer_func_t *)UdpFunc, NULL);//UDP广播任务

	os_timer_arm(&udp_timer, 1000, 1);

	wifi_set_opmode(1);


}