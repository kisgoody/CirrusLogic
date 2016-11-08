#include "stm8l10x.h"
#include "i2c_master_poll.h"
#include "serial.h"
#include "packet.h"
#include "system.h"
#include "string.h"
#include "iostm8l.h"
#include "version.h"

#define BOOTLOADER 1
#ifndef BOOTLOADER
#include "dev_config.h"
#endif


volatile uint8_t rx_write_index = 0;					// global UART receive ring buffer write index
uint8_t rx_read_index = 0;								// global UART receive ring buffer read index

@near volatile uint8_t rx_buffer[MAX_UART_LENGTH];	// global UART receive ring buffer

uint8_t LAM_busy;									// global LAM busy flag
uint8_t EA_open;									// global EA session open flag
uint8_t pending_launch;						// global flag indicating need to request launch
uint8_t DAC_EN;								  // global flag indicating DAC enabled
uint8_t ADC_EN;									// global flag indicating ADC enabled
uint8_t LED_ON;									// global flag indicating LED is off for low power mode
uint8_t WAITING;								// global flag indicating 1 sec timer for SDA

uint8_t pending_en_ch;						// global flag indicating need to change enable state
uint8_t Tip_plugged;	  					// global flag indicating status of 3.5mm jack
uint8_t HP_Only;								  // global flag indicating headphone only (no mic)
uint8_t Optical_Only;						  // global flag indicating optical only (no capability)

@eeprom const uint8_t FW_Version[3] = {FW_MAJOR_VER, FW_MINOR_VER, FW_REVISION}; //literal address 0x9FC0
@eeprom uint8_t LAM_Configured = 0;

volatile uint8_t timer2_count;						// global TIM2 count divider
volatile uint16_t TIM4_tout;						// global TIM4 tick counter (from AN3281)

void main(void)
{
	uint8_t temp_read;								// local buffer

	uint8_t packet_index;							// receive packet position marker
	uint8_t packet_type;							// receive packet type
	uint8_t packet_length;							// receive packet length

	@near uint8_t packet_payload[MAX_PACKET_LENGTH - 4];

	uint8_t current_packet;							// current packet undergoing transmission to LAM

	uint8_t plug_state; 							// headset jack flag, 1=plugged
	uint8_t button_state;							// side button flag, 1=released
	uint8_t SP_button_st;						  // spare button flag, 1=released

	// system initialization --------------------------------------------------

	packet_index = 0;							// reset packet position marker
	LAM_busy = 0;									// reset LAM busy flag
	EA_open = 0;									// reset EA session open flag
	pending_launch = 0;						// reset pending_launch flag
	ADC_EN = 0;									 // ADC has not been enabled
	DAC_EN = 0;									 // DAC has not been enabled
	LED_ON = 1;									// set flag
	WAITING = 0;									// set flag
	current_packet = accReady;						// initialize packet state machine
	button_state = 1;								// assume side button is released
	SP_button_st = 1;								// assume spare button is released
	Tip_plugged = 0;
	Optical_Only = 1;							// no capability
	HP_Only = 0;
	

	// peripheral initialization ----------------------------------------------

#ifndef BOOTLOADER
	CLK_MasterPrescalerConfig (CLK_MasterPrescaler_HSIDiv1);
	serial_init ();									// initialize UART
#endif
	timer2_init ();									// initialize TIM2
	timer3_init ();									// initialize TIM3

	// enable I2C peripheral clock
	CLK_PeripheralClockConfig (CLK_Peripheral_I2C, ENABLE);

	// configure I2C peripheral
	I2C_Cmd (ENABLE);
	I2C_Init (100000, 0x00A0, I2C_DutyCycle_2, I2C_Ack_Enable, I2C_AcknowledgedAddress_7bit);
	I2C_ITConfig (I2C_IT_ERR, ENABLE);
#ifndef BOOTLOADER
	pin_setup ();									// initialize GPIO
#endif
	EXTI->CR2 = 0x01;							//Change Portx bit 4 ext int to rising
	GPIO_ResetBits (GPIOB, GPIO_Pin_7);  // reset codec
	GPIO_SetBits (GPIOB, GPIO_Pin_7);				// take codec out of reset
	
	enableInterrupts ();							// enable interrupts
	codec_init();
	// PB3: tied low on CDB, high by internal pullup on CRD
	GPIO_Init (GPIOB, GPIO_Pin_3, GPIO_Mode_In_PU_No_IT);
	
	//Revision 1.1.0 was created to default the CRD to AHJ headset type if no headset is connected
	//The routine hs_manual configures the codec to override the CS42L42 switch states
	if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_1) != RESET | GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_3) != RESET){		//true when plugged | true when CRD
		Tip_plugged = 1;								// set flag showing plugged 
		//codec_init();
		hs_type_det ();									//initial detection if plugged at power up
		DAC_EN = 0;									  // will be enabled 
		if (Optical_Only & GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_3) != RESET){		//true when unplugged & true when CRD
			hs_manual ();									  // force to AHJ 
		}
	} else {                        //here because it's not plugged
		Tip_plugged = 0;							// reset flag 
		HP_Only = 1;									// no mic
	}	

// main loop --------------------------------------------------------------

	while (1)
	{
		// if this is a CDB monitor state of plug 
		if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_3) == RESET) plug_state = next_plug_state (plug_state);
		// monitor state of side button
		button_state = next_button_state (button_state);
		// determine what packet, if any, must be sent next
		current_packet = next_packet (current_packet);
		

		// evaluate bytes received from LAM and address potential packets
		if (rx_write_index != rx_read_index)
		{

			// retrieve oldest character from ring buffer and increment read pointer
			temp_read = rx_buffer[rx_read_index++];

			// wrap read pointer
			rx_read_index %= MAX_UART_LENGTH;

			// evaluate received byte and set packet location marker accordingly
			packet_index = packet_receive (packet_index, temp_read, &packet_length, &packet_type, packet_payload);

		} // if (ring buffer not aligned)

	} // while (main loop)

}


#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif