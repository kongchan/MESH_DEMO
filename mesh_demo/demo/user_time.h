#include "sntp.h"
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"
#include "c_types.h"

#define M106 1000000

void ICACHE_FLASH_ATTR user_time_init(void);
u32 ICACHE_FLASH_ATTR user_time_get();
