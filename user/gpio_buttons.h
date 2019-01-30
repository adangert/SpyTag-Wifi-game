#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

#include "user_interface.h"

void ICACHE_FLASH_ATTR SetupGPIO();


//Don't call, use LastGPIOState
unsigned char GetButtons();

//You write.
void HandleButtonEvent( uint8_t state, int button, int down );

extern volatile uint8_t LastGPIOState; //From last "GetButtons()" command.  Will not be updated until after interrupt and all HandleButtonEvent messages have been called.

#endif
