// pre-compiler definitions

#define MAX_UART_LENGTH		256						// maximum UART ring buffer size

// function declarations

void serial_print_char (uint8_t value);

void serial_service_rx (void);
void serial_init (void);

// global variables

extern volatile uint8_t rx_write_index;
extern uint8_t rx_read_index;

extern @near volatile uint8_t rx_buffer[];