/*
 * Copyright (c) 2015 Cirrus Logic, Inc.
 */

#include "stm8l10x_conf.h"

/* UART initialization function */
void serial_init(void)
{
	/* Configure TX pin */
	GPIO_Init(GPIOC, GPIO_Pin_3, GPIO_Mode_Out_PP_High_Slow);

	/* Configure RX pin */
	GPIO_Init(GPIOC, GPIO_Pin_2, GPIO_Mode_In_PU_No_IT);

	/* Enable USART clock */
	CLK_PeripheralClockConfig(CLK_Peripheral_USART, ENABLE);

	USART_DeInit();
	/* USART configured as follow:
		  - BaudRate = 57600 baud
		  - Word Length = 8 Bits
		  - One Stop Bit
		  - No parity
		  - Receive and transmit enabled
	*/
	USART_Init((uint32_t)57600, USART_WordLength_8D, USART_StopBits_1, USART_Parity_No, (USART_Mode_TypeDef)(USART_Mode_Rx | USART_Mode_Tx));

	USART_ITConfig (USART_IT_RXNE, ENABLE);
}

/* GPIO pin initialization function */
void pin_setup(void)
{
	// PA2: SPARE button input  (CDB only)
	GPIO_Init (GPIOA, GPIO_Pin_2, GPIO_Mode_In_PU_No_IT);

	// PA6: unused
	GPIO_Init (GPIOA, GPIO_Pin_6, GPIO_Mode_In_PU_No_IT);

	// PB0:may be used to monitor /INT output of codec
	// PB1:may be used to monitor /WAKE output of codec	
	// PB0-1: codec GPIO
	GPIO_Init (GPIOB, (GPIO_Pin_0 | GPIO_Pin_1), GPIO_Mode_In_PU_No_IT);
	
	// PB2, PB3: test output  **DEBUG Only
	GPIO_Init (GPIOB, (GPIO_Pin_2 | GPIO_Pin_3), GPIO_Mode_Out_PP_Low_Slow);

	// PB4: side button input
	GPIO_Init (GPIOB, GPIO_Pin_4, GPIO_Mode_In_PU_No_IT);

	// PB5, PB6: red LED, amber LED outputs
	GPIO_Init (GPIOB, (GPIO_Pin_5 | GPIO_Pin_6), GPIO_Mode_Out_PP_Low_Slow);

	// PB7: codec reset output
	GPIO_Init (GPIOB, GPIO_Pin_7, GPIO_Mode_Out_PP_Low_Slow);

	// PC0, PC1: SDA, SCL
	GPIO_Init (GPIOC, (GPIO_Pin_0 | GPIO_Pin_1), GPIO_Mode_Out_OD_HiZ_Slow);

	/* NOTE: PC2, PC3 (UART RX, TX) configured in serial_init () */

	// PC4: LAM mute input
	GPIO_Init (GPIOC, GPIO_Pin_4, GPIO_Mode_In_PU_IT);

	// PD0: LAM programming input
	GPIO_Init (GPIOD, GPIO_Pin_0, GPIO_Mode_In_PU_No_IT);

	// PD4, PD5: playback indicator LEDs (development board only)
	//GPIO_Init (GPIOD, (GPIO_Pin_4 | GPIO_Pin_5), GPIO_Mode_Out_OD_HiZ_Slow);
}