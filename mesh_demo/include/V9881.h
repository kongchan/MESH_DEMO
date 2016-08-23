#ifndef _V9881_H_
#define	_V9881_H_

#include "user_interface.h"
#include "osapi.h"
#include "os_type.h"
#include "c_types.h"


#define offset_W  3
#define offset_U  7
#define offset_I  11
#define offset_Hz 15
#define offset_P  19
#define offset_var  23
#define offset_Va  27
#define offset_F 31


typedef struct  {

	uint32 W;
	uint32 U;
	uint32 I;
	uint32 Hz;

	uint32 P;
	uint32 var;
	uint32 Va;
	uint32 F ;

	u8* jsonString;

} V9881_Data;


void  ICACHE_FLASH_ATTR V9881_ReadData(V9881_Data *v9881_data, u8 *bufSource, u16 len);

#endif