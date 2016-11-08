#include "stm8l10x.h"
#include "serial.h"

// character print function ---------------------------------------------------
void serial_print_char (uint8_t value)
{

	// wait for transmit empty flag
	while (USART_GetFlagStatus (USART_FLAG_TXE) == RESET);

	// send character
	USART_SendData8 (value);

} // --------------------------------------------------------------------------


// UART receive interrupt handler ---------------------------------------------
void serial_service_rx (void)
{

	// capture received byte and increment write pointer
	rx_buffer[rx_write_index++] = USART_ReceiveData8 ();

	// wrap write pointer
	rx_write_index %= MAX_UART_LENGTH;

} // --------------------------------------------------------------------------


// UART initialization function -----------------------------------------------
void serial_init (void)
{

	/* Configure TX pin */
	GPIO_Init (GPIOC, GPIO_Pin_3, GPIO_Mode_Out_PP_High_Slow);

	/* Configure RX pin */
	GPIO_Init (GPIOC, GPIO_Pin_2, GPIO_Mode_In_PU_No_IT);

	/* Enable USART clock */
	CLK_PeripheralClockConfig (CLK_Peripheral_USART, ENABLE);

	USART_DeInit ();
	/* USART configuration ------------------------------------------------------*/
	/* USART configured as follow:
		  - BaudRate = 57600 baud
		  - Word Length = 8 Bits
		  - One Stop Bit
		  - No parity
		  - Receive and transmit enabled
	*/
	USART_Init ((uint32_t)57600, USART_WordLength_8D, USART_StopBits_1, USART_Parity_No, (USART_Mode_TypeDef)(USART_Mode_Rx | USART_Mode_Tx));

	/* Enable the USART Receive interrupt: this interrupt is generated when the USART receive data register is not empty */
	USART_ITConfig (USART_IT_RXNE, ENABLE);

	rx_write_index = 0;					// line up UART receive ring buffer
	rx_read_index = 0;

} // --------------------------------------------------------------------------