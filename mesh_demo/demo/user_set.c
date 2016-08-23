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

extern u8 temp[256];
extern struct ip_info IP_Info;

#define  MaxRam 2*1024+4

extern u8 bufTemp[MaxRam];//串口数据缓存空间
extern u32 num;//串口数据大小

static u8 bufSmartStop[2] = { 0xF6,0x00 };//停止
static u8 bufSmartStart[2] = { 0xF6,0x01 };//开始
static u8 bufSmartLink[2] = { 0xF6,0x02 };//成功
static u8 bufSmartOver[6] = { 0xF6,0x03,0xc0,0xA8,0x01,0x64 };//完成

static u8 bufModeStatus[2] = { 0xF1,00 };

os_timer_t wifi_timer;
bool led = 1;

void ICACHE_FLASH_ATTR smartconfig_done(sc_status status, void *pdata)
{
	switch (status)
	{
	case SC_STATUS_LINK:
		uart0_tx_buffer(bufSmartLink, 2);
		struct station_config *sta_conf = pdata;
		wifi_station_set_config(sta_conf);
		wifi_station_disconnect();
		wifi_station_connect();
		break;
	case SC_STATUS_LINK_OVER:
		//uart0_tx_buffer(bufSmartOver, 2);
		if (pdata != NULL)
		{
			memcpy(bufSmartOver + 2, (uint8*)pdata, 4);
			uart0_tx_buffer(bufSmartOver, 6);

			os_timer_disarm(&wifi_timer);

			LED_ON(LED1);

		}
		wifi_set_opmode(3);
		smartconfig_stop();
		break;
	}
}



u8 ICACHE_FLASH_ATTR bufCheck(u8 *buf, u8 ck)
{
	u8 add = 0;
	while (*buf != ck)
	{
		buf++;
		add++;
	}
	return add;
}


void ICACHE_FLASH_ATTR User_Wifi_Connect(char *ssid, char *passwd)
{
    struct station_config sta_config;

	os_memset(sta_config.ssid, 0, 32);
	os_memset(sta_config.password, 0, 64);

	strcpy(sta_config.ssid, ssid);
	strcpy(sta_config.password, passwd);

	sta_config.bssid_set = 0;

	wifi_station_disconnect();
	wifi_station_set_config(&sta_config);
	wifi_station_connect();

	system_restart();
}



u8 ICACHE_FLASH_ATTR FuncWifiTimer()
{
	if(led)
	{
		LED_ON(LED1);
		led = 0;
	}
	else
	{
		LED_OFF(LED1);
		led = 1;
	}
}

bool smartFlag = 1;

void ICACHE_FLASH_ATTR FuncSmart()
{
	if(smartFlag)
	{
		smartFlag = 0;

		smartconfig_stop();
		wifi_set_opmode(1);
		smartconfig_set_type(SC_TYPE_ESPTOUCH);
		smartconfig_start(smartconfig_done, 1);


		os_timer_disarm(&wifi_timer);
		os_timer_setfn(&wifi_timer, (os_timer_func_t *)FuncWifiTimer, NULL);
		os_timer_arm(&wifi_timer, 250, 1);

		os_printf("SmartLink Start");
	}
	else 
	{
		smartFlag = 1;

		os_timer_disarm(&wifi_timer);

		User_Wifi_Connect(STA_SSID, STA_PASS);
	}
}

void ICACHE_FLASH_ATTR  scan_done_cb_t2 (void *arg, STATUS status)
{
	{
		uint8 ssid[33];
		char temp[128];
		if (status == OK)
		{
			struct bss_info *bss_link = (struct bss_info *)arg;
			bss_link = bss_link->next.stqe_next;//ignore first

			while (bss_link != NULL)
			{
				os_memset(ssid, 0, 33);
				if (os_strlen(bss_link->ssid) <= 32)
				{
					os_memcpy(ssid, bss_link->ssid, os_strlen(bss_link->ssid));
				}
				else
				{
					os_memcpy(ssid, bss_link->ssid, 32);
				}
				os_sprintf(temp,"+CWLAP:(%d,\"%s\",%d,\""MACSTR"\",%d)\r\n",
					bss_link->authmode, ssid, bss_link->rssi,
					MAC2STR(bss_link->bssid),bss_link->channel);
				uart0_sendStr(temp);
				bss_link = bss_link->next.stqe_next;
			}
		}

	}
}


//设置任务
void  ICACHE_FLASH_ATTR TaskSet(u8 *buf, u16 len)
{
	struct softap_config soft_config;
	struct station_config sta_config;

	switch (bufTemp[3])
	{

	case 0xF0:
		{
			uart0_sendStr("system_restart()");
			system_restart();
			return ;

		}break;

	case 0xF1:
		{
			if (bufTemp[4] < 4)
			{
				wifi_set_opmode(bufTemp[4]);
				uart0_sendStr("OK");

			}
			else
			{
				os_sprintf(temp, "opmode:%d", wifi_get_opmode());
				uart0_sendStr(temp);

			}
			//	spi_flash_read(ADDR_WIFISET,(u32 *)bufFlash,4096);
			//	uart0_tx_buffer(bufFlash, 4096);
		}break;

	case 0xF2://配置AP信息
		{
			if (num > 5)
			{
				if (bufTemp[4] > 0)
				{
					bufTemp[num] = 0;
					strcpy(soft_config.ssid, bufTemp + 5);

					strcpy(soft_config.password, bufTemp + bufCheck(bufTemp, 0) + 1);

					soft_config.authmode = AUTH_WPA_PSK;

					soft_config.max_connection = 4;

				}
				else
				{
					bufTemp[num] = 0;
					soft_config.authmode = AUTH_OPEN;


					os_bzero(soft_config.ssid, sizeof(soft_config.ssid));
					strcpy(soft_config.ssid, bufTemp + 5);
				}

				os_sprintf(temp, "ssid:\"%s\",passwd:\"%s\",maxConnect:%d\r\n",
					soft_config.ssid, soft_config.password, soft_config.max_connection);
				uart0_sendStr(temp);

				wifi_softap_set_config(&soft_config);
			}
			else
			{
				wifi_softap_get_config(&soft_config);
				os_sprintf(temp, "ssid:\"%s\",passwd:\"%s\",maxConnect:%d\r\n",
					soft_config.ssid, soft_config.password, soft_config.max_connection);
				uart0_sendStr(temp);
			}


		}break;

	case 0xF3://配置STA信息
		{
			if (num > 5)
			{
				bufTemp[num] = 0;

				if (bufTemp[4] > 0)
				{
					User_Wifi_Connect(bufTemp + 5, bufTemp + bufCheck(bufTemp, 0) + 1);
				}
				else
				{
					;
				}
			}

			wifi_softap_get_config(&soft_config);

			os_sprintf(temp, "ssid:\"%s\",passwd:\"%s\",maxConnect:%d\r\n",
				sta_config.ssid, sta_config.password, sta_config.bssid);
			uart0_sendStr(temp);


		}break;

	case 0xF4:
		{
			wifi_station_scan(NULL, scan_done_cb_t2);

		}break;

	case 0xF5:
		{
			wifi_get_ip_info(1, &IP_Info);
			os_sprintf(temp, "AP:%d.%d.%d.%d  ", (u8)(IP_Info.ip.addr) >> 0, (u8)((IP_Info.ip.addr) >> 8), (u8)((IP_Info.ip.addr) >> 16), (u8)((IP_Info.ip.addr) >> 24));
			uart0_sendStr(temp);

			wifi_get_ip_info(0, &IP_Info);
			os_sprintf(temp, "STA:%d.%d.%d.%d\r\n", (u8)(IP_Info.ip.addr) >> 0, (u8)((IP_Info.ip.addr) >> 8), (u8)((IP_Info.ip.addr) >> 16), (u8)((IP_Info.ip.addr) >> 24));
			uart0_sendStr(temp);
		}break;


	case 0xF6:
		{
			if (len > 4)
			{
				if (bufTemp[4] == 0x01)//发01开始配置
				{
					FuncSmart();
				}
				else//发其他停止配置
				{
					smartconfig_stop();
					wifi_set_opmode(3);
					uart0_tx_buffer(bufSmartStop, 2);
				}
			}

		}break;
	}

	num = 0;
}
