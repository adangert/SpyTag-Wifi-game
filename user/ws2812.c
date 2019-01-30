#include "ws2812.h"
#include "ets_sys.h"
#include "gpio.h"
//#include "mystuff.h"
#include "osapi.h"
#include <commonservices.h>
#define GPIO_OUTPUT_SET(gpio_no, bit_value) \
	gpio_output_set(bit_value<<gpio_no, ((~bit_value)&0x01)<<gpio_no, 1<<gpio_no,0)


//I just used a scope to figure out the right time periods.

/*void  SEND_WS_0()
{
	uint8_t time;
	time = 3; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
	time = 8; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 0 );
}

void  SEND_WS_1()
{
	uint8_t time;
	time = 7; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
	time = 5; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 0 );
}*/

void ICACHE_FLASH_ATTR SEND_WS_0()
{
uint8_t time;
time = 5; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0x1 );
time = 17; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0 );
}

void ICACHE_FLASH_ATTR SEND_WS_1()
{
uint8_t time;
time = 19; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0x1 );
time = 7; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0 );
}

void ICACHE_FLASH_ATTR SEND_WS_2()
{
uint8_t time;
time = 5; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0x4 );
time = 17; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0 );
}

void ICACHE_FLASH_ATTR SEND_WS_3()
{
uint8_t time;
time = 19; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0x4 );
time = 7; while(time--) WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR , 0 );
}

/*
void SEND_WS_0(){
  WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR, 0x1 );
  //GPIO_OUTPUT_SET(2,1);
}

void SEND_WS_1(){
  //GPIO_OUTPUT_SET(2,1);
}*/
/*
void SEND_WS_0()
{
  uint8_t time = 8;
  WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
  WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
  while(time--)
  {
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 0 );
  }
}
void SEND_WS_1()
{
  uint8_t time = 9;
  while(time--)
  {
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
  }
  time = 3;
  while(time--)
  {
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 0 );
  }
}*/

void   WS2812OutBuffer( uint8_t * buffer, uint16_t length, int light_level )
{
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO2);
	uint16_t i;
	//GPIO_OUTPUT_SET(GPIO_ID_PIN(WSGPIO), 1);
//  GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
  GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
	ets_intr_lock();

	for( i = 0; i < length; i++ )
	{
		uint8_t mask = 0x80;
		uint8_t byte = (buffer[i]*light_level);
		while (mask)
		{
			if( byte & mask ) SEND_WS_3(); else SEND_WS_2();
			mask >>= 1;
    }
	}

	ets_intr_unlock();
}
