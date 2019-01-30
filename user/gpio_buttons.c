#include "gpio_buttons.h"
#include "user_interface.h"
#include "c_types.h"
#include <gpio.h>
#include <ets_sys.h>
#include <esp82xxutil.h>

void gpio_pin_intr_state_set(uint32 i, GPIO_INT_TYPE intr_state);

volatile uint8_t LastGPIOState;

static const uint8_t GPID[] = { 0, 12 };
//static const uint8_t GPID[] = { 0, 2, 12, 13, 14, 15, 4, 5 };
static const uint8_t Func[] = { FUNC_GPIO0, FUNC_GPIO2, FUNC_GPIO12, FUNC_GPIO13, FUNC_GPIO14, FUNC_GPIO15, FUNC_GPIO4, FUNC_GPIO5 };
static const int  Periphs[] = { PERIPHS_IO_MUX_GPIO0_U, PERIPHS_IO_MUX_GPIO2_U, PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_MTCK_U, PERIPHS_IO_MUX_MTMS_U, PERIPHS_IO_MUX_MTDO_U, PERIPHS_IO_MUX_GPIO4_U, PERIPHS_IO_MUX_GPIO5_U };


void interupt_test( void * v )
{
	int i;

	uint8_t stat = GetButtons();

	for( i = 0; i < 8; i++ )
	{
		int mask = 1<<i;
		if( (stat & mask) != (LastGPIOState & mask) )
		{
			HandleButtonEvent( stat, i, (stat & mask)?1:0 );
		}
	}
	LastGPIOState = stat;

	//clear interrupt status
	uint32  gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
}


void ICACHE_FLASH_ATTR SetupGPIO()
{
	printf( "SETTING UP GPIO\n" );

	int i;
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	ETS_GPIO_INTR_ATTACH(interupt_test, 0); // GPIO12 interrupt handler
	for( i = 0; i < 8; i++ )
	{
		PIN_FUNC_SELECT(Periphs[i], Func[i]);
		PIN_DIR_INPUT = 1<<GPID[i];
		gpio_pin_intr_state_set(GPIO_ID_PIN(GPID[i]), 3); // Interrupt on any GPIO12 edge
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(GPID[i])); // Clear GPIO12 status
	}
	ETS_GPIO_INTR_ENABLE(); // Disable gpio interrupts
	LastGPIOState = GetButtons();
 	printf( "Setup GPIO Complete\n" );
}


uint8_t GetButtons()
{
	uint8_t ret = 0;
	int i;
	uint32_t pin = PIN_IN;
	int mask = 1;
	for( i = 0; i < 8; i++ )
	{
		ret |= (pin & (1<<GPID[i]))?mask:0;
		mask <<= 1;
	}
	ret ^= ~32; //GPIO15's logic is inverted.  Don't flip it but flip everything else.
	return ret;
}
