#include "key.h"
#include "user_config.h"
#include "osapi.h"

#include "ets_sys.h"

#include "c_types.h"

#include "user_set.h"

static bool led1 = 0;

LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key;

#define USERBIN 0

void ICACHE_FLASH_ATTR user_plug_short_press(void)
{
	os_printf("user_plug_short_press\r\n");

	if(led1 == 0)
	{
		led1 = 1;
		LED_ON(LED3);
	}
	else
	{
		led1 = 0;
		LED_OFF(LED3);
	}
}

void ICACHE_FLASH_ATTR user_plug_long_press(void)
{
	os_printf("user_plug_long_press\r\n");


	FuncSmart();

	//system_restore();
	//system_restart();
}


void ICACHE_FLASH_ATTR user_key_start(void)
{
	single_key = key_init_single(GPIO_ID_PIN(0), PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0, user_plug_long_press, user_plug_short_press);

	keys.key_num = 1;
	keys.single_key = &single_key;

	key_init(&keys);
}