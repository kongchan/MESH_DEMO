#include "user_time.h"
#include "sntp.h"
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"
#include "c_types.h"



LOCAL os_timer_t sntp_timer;

LOCAL u32 lastSysTime;
LOCAL u32 lastUTC;



u32 ICACHE_FLASH_ATTR user_time_get()
{
	return (system_get_time() / M106 - lastSysTime + lastUTC);
}

void ICACHE_FLASH_ATTR user_check_sntp_stamp(void *arg)
{
	uint32 current_stamp;

	current_stamp = sntp_get_current_timestamp();

	if(current_stamp == 0)
	{
		os_timer_arm(&sntp_timer, 1000, 0);
	} 
	else
	{
		os_timer_disarm(&sntp_timer);
		os_printf("sntp: %d, %s \n",current_stamp,sntp_get_real_time(current_stamp));

		lastSysTime = system_get_time() / M106;
		lastUTC = current_stamp;
	}
}




void ICACHE_FLASH_ATTR  user_time_init()
{
	ip_addr_t *addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));
	sntp_setservername(0, "us.pool.ntp.org"); // set server 0 by domain name
	sntp_setservername(1, "ntp.sjtu.edu.cn"); // set server 1 by domain name
	ipaddr_aton("210.72.145.44", addr);
	sntp_setserver(2, addr); // set server 2 by IP address
	sntp_init();
	os_free(addr);

	os_timer_disarm(&sntp_timer);
	os_timer_setfn(&sntp_timer, (os_timer_func_t *)user_check_sntp_stamp, NULL);

	os_timer_arm(&sntp_timer, 1000, 0);
}
