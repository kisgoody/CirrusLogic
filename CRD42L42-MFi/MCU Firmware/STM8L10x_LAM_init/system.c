#include "stm8l10x.h"
#include "system.h"




// GPIO pin initialization function -------------------------------------------
void pin_setup (void)
{

	// PA2: SPARE button input  (CDB only)
	GPIO_Init (GPIOA, GPIO_Pin_2, GPIO_Mode_In_PU_No_IT);

	// PA6: unused
	GPIO_Init (GPIOA, GPIO_Pin_6, GPIO_Mode_In_PU_No_IT);

	// PB0:may be used to monitor /INT output of codec
	// PB1:may be used to monitor /WAKE output of codec	
	// PB0-1: codec GPIO
	GPIO_Init (GPIOB, (GPIO_Pin_0 | GPIO_Pin_1), GPIO_Mode_In_PU_No_IT);
	
	// PB2: test output  **DEBUG Only
	GPIO_Init (GPIOB, GPIO_Pin_2, GPIO_Mode_Out_PP_Low_Slow);
	// PB3: tied low on CDB, high by internal pullup on CRD
	GPIO_Init (GPIOB, GPIO_Pin_3, GPIO_Mode_In_PU_No_IT);

	// PB4: side button input
	GPIO_Init (GPIOB, GPIO_Pin_4, GPIO_Mode_In_PU_No_IT);

	// PB5, PB6: red LED, green LED outputs
	GPIO_Init (GPIOB, (GPIO_Pin_5 | GPIO_Pin_6), GPIO_Mode_Out_PP_Low_Slow);

	// PB7: codec reset output
	GPIO_Init (GPIOB, GPIO_Pin_7, GPIO_Mode_Out_PP_Low_Slow);

	// PC0, PC1: SDA, SCL
	GPIO_Init (GPIOC, (GPIO_Pin_0 | GPIO_Pin_1), GPIO_Mode_Out_OD_HiZ_Slow);

	/* NOTE: PC2, PC3 (UART RX, TX) configured in serial_init () */

	// PC4: LAM mute input
	GPIO_Init (GPIOC, GPIO_Pin_4, GPIO_Mode_In_PU_No_IT);

	// PD0: LAM programming input
	GPIO_Init (GPIOD, GPIO_Pin_0, GPIO_Mode_In_PU_No_IT);

} // --------------------------------------------------------------------------


// timer 2 initialization function --------------------------------------------
void timer2_init (void)
{

	// enable clock to TIM2
	CLK_PeripheralClockConfig (CLK_Peripheral_TIM2, ENABLE);

	TIM2_DeInit ();									// reset TIM2 registers

	// configure TIM2 for 10 ms
	TIM2_TimeBaseInit (TIM2_Prescaler_128, TIM2_CounterMode_Up, 1250);

	TIM2_ITConfig (TIM2_IT_Update, ENABLE);			// enable interrupt

} // --------------------------------------------------------------------------


// timer 2 overflow interrupt handler -----------------------------------------
void timer2_service (void)
{

	// divide 10-ms timer into one-shot 1-sec timer
	if (timer2_count < 100) timer2_count++; else TIM2_Cmd (DISABLE);

	TIM2_ClearITPendingBit (TIM2_IT_Update);		// clear interrupt flag

} // --------------------------------------------------------------------------


// timer 3 initialization function --------------------------------------------
void timer3_init (void)
{

	// enable clock to TIM3
	CLK_PeripheralClockConfig (CLK_Peripheral_TIM3, ENABLE);

	TIM3_DeInit ();									// reset TIM3 registers

	// configure TIM3 for 15 ms
	TIM3_TimeBaseInit (TIM3_Prescaler_128, TIM3_CounterMode_Up, 1875);

	TIM3_GenerateEvent (TIM3_EventSource_Update);	// force update event
	TIM3_ClearFlag (TIM3_FLAG_Update);				// clear update flag

} // --------------------------------------------------------------------------


// 15-ms delay function -------------------------------------------------------
void wait_15ms (void)
{

	TIM3_Cmd (ENABLE);								// enable TIM3

	// wait for TIM3 to count
	while (TIM3_GetFlagStatus (TIM3_FLAG_Update) == RESET);

	TIM3_Cmd (DISABLE);								// disable TIM3

	TIM3_ClearFlag (TIM3_FLAG_Update);				// clear update flag

} // --------------------------------------------------------------------------
