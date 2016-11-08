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
