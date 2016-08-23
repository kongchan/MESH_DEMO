#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "upgrade.h"
#include "user_config.h"
#include "gpio.h"


#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,eb-US;q=0.8\r\n\r\n"



u8 URL[32];


extern os_timer_t udp_timer;//UDP¹ã²¥¶¨Ê±Æ÷

struct espconn *pespconn = NULL;
struct upgrade_server_info *upServer = NULL;

static os_timer_t delay_check;
static os_timer_t upgrade_timer;
static struct espconn *pTcpServer = NULL;
static ip_addr_t host_ip;


void ICACHE_FLASH_ATTR response_error()
{
	os_printf("error\r\n");
}

void ICACHE_FLASH_ATTR response_ok()
{
	os_printf("ok\r\n");
}

LOCAL void ICACHE_FLASH_ATTR upDate_rsp(void *arg)
{
  struct upgrade_server_info *server = arg;

  if(server->upgrade_flag == true)
  {
    os_printf("device_upgrade_success\r\n");
    response_ok();
    system_upgrade_reboot();
  }
  else
  {
    os_printf("device_upgrade_failed\r\n");
    response_error();
  }

  os_free(server->url);
  server->url = NULL;
  os_free(server);
  server = NULL;
}


static void ICACHE_FLASH_ATTR upDate_discon_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  uint8_t idTemp = 0;

  if(pespconn->proto.tcp != NULL)
  {
    os_free(pespconn->proto.tcp);
  }
  if(pespconn != NULL)
  {
    os_free(pespconn);
  }
}

bool upFlag=1;

void ICACHE_FLASH_ATTR FuncUpgradeTimer()
{
	upFlag = 1- upFlag;
	if(upFlag)
	{
		LED_ON(LED1);
		LED_ON(LED2);
		LED_ON(LED3);
	}
	else
	{
		LED_OFF(LED1);
		LED_OFF(LED3);
		LED_OFF(LED2);
	}
}

LOCAL void ICACHE_FLASH_ATTR upDate_recv(void *arg, char *pusrdata, unsigned short len)
{
  struct espconn *pespconn = (struct espconn *)arg;
  char temp[32] = {0};
  uint8_t user_bin[12] = {0};
  uint8_t i = 0;

  os_timer_disarm(&delay_check);
  os_printf("+CIPUPDATE:3\r\n");

  os_printf("%s", pusrdata);

  upServer = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

  upServer->upgrade_version[5] = '\0';

  upServer->pespconn = pespconn;

  os_memcpy(upServer->ip, pespconn->proto.tcp->remote_ip, 4);

  upServer->port = pespconn->proto.tcp->remote_port;

  upServer->check_cb = upDate_rsp;
  upServer->check_times = 60000;

  if(upServer->url == NULL)
  {
    upServer->url = (uint8 *) os_zalloc(1024);
  }

  if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1)
  {
    os_memcpy(user_bin, "user2.bin", 10);
  }
  else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
  {
    os_memcpy(user_bin, "user1.bin", 10);
  }

  os_sprintf(upServer->url, "GET /%s HTTP/1.1\r\nHost: %s\r\n"pheadbuffer"", user_bin, URL, IP2STR(upServer->ip));

  os_printf(upServer->url);


  
  

  if(system_upgrade_start(upServer) == false)
  {
	  response_error();
  }
  else
  {

	  os_timer_disarm(&upgrade_timer);
	  os_timer_setfn(&upgrade_timer, (os_timer_func_t *)FuncUpgradeTimer, NULL);
	  os_timer_arm(&upgrade_timer, 100, 1);

	  os_printf("+CIPUPDATE:4\r\n");
  }
}

LOCAL void ICACHE_FLASH_ATTR
upDate_wait(void *arg)
{
  struct espconn *pespconn = arg;
  os_timer_disarm(&delay_check);
  if(pespconn != NULL)
  {
    espconn_disconnect(pespconn);
  }
  else
  {
    response_error();
  }
}

LOCAL void ICACHE_FLASH_ATTR
upDate_sent_cb(void *arg)
{
  struct espconn *pespconn = arg;
  os_timer_disarm(&delay_check);
  os_timer_setfn(&delay_check, (os_timer_func_t *)upDate_wait, pespconn);
  os_timer_arm(&delay_check, 5000, 0);
  os_printf("upDate_sent_cb\r\n");
}

static void ICACHE_FLASH_ATTR
upDate_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  uint8_t user_bin[9] = {0};
  char *temp = NULL;

  os_printf("+CIPUPDATE:2\r\n");


  espconn_regist_disconcb(pespconn, upDate_discon_cb);
  espconn_regist_recvcb(pespconn, upDate_recv);////////
  espconn_regist_sentcb(pespconn, upDate_sent_cb);

  temp = (uint8 *) os_zalloc(512);

  os_sprintf(temp,"GET /v1/device/rom/?is_formsimple=true HTTP/1.1\r\nHost: %s\r\n"pheadbuffer"", URL, IP2STR(pespconn->proto.tcp->remote_ip));

  os_printf(temp);

  espconn_sent(pespconn, temp, os_strlen(temp));
  os_free(temp);
}

static void ICACHE_FLASH_ATTR
upDate_recon_cb(void *arg, sint8 errType)
{
  struct espconn *pespconn = (struct espconn *)arg;

    response_error();
    if(pespconn->proto.tcp != NULL)
    {
      os_free(pespconn->proto.tcp);
    }
    os_free(pespconn);
    os_printf("disconnect\r\n");

    if(upServer != NULL)
    {
      os_free(upServer);
      upServer = NULL;
    }
    response_error();

}


LOCAL void ICACHE_FLASH_ATTR user_esp_platform_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
	struct espconn *pespconn = (struct espconn *) arg;

	static u8 MaxTimes;

	if(ipaddr == NULL)
	{
		response_error();
		return;
	}

	if(host_ip.addr == 0 && ipaddr->addr != 0)
	{

		os_printf("user_esp_platform_dns_found %d.%d.%d.%d\n",
			*((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
			*((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

		if(pespconn->type == ESPCONN_TCP)
		{
			os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);
			espconn_regist_connectcb(pespconn, upDate_connect_cb);
			espconn_regist_reconcb(pespconn, upDate_recon_cb);
			espconn_connect(pespconn);
		}
	}
}


void ICACHE_FLASH_ATTR FuncUpdate(char *ipaddr)
{
  os_timer_disarm(&udp_timer);

  os_memset(URL, 0, 32);
  os_strcpy(URL, ipaddr);

  pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  pespconn->type = ESPCONN_TCP;
  pespconn->state = ESPCONN_NONE;
  pespconn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  pespconn->proto.tcp->local_port = espconn_port();
  pespconn->proto.tcp->remote_port = 80;


  if(ipaddr[0]>'9')
  {
	  espconn_gethostbyname(pespconn, URL, &host_ip, user_esp_platform_dns_found);
  }

  else
  {
	  host_ip.addr = ipaddr_addr(ipaddr);

	  os_printf("+CIPUPDATE:1\r\n");
	  os_memcpy(pespconn->proto.tcp->remote_ip, &host_ip.addr, 4);
	  espconn_regist_connectcb(pespconn, upDate_connect_cb);
	  espconn_regist_reconcb(pespconn, upDate_recon_cb);
	  espconn_connect(pespconn);
  }
}