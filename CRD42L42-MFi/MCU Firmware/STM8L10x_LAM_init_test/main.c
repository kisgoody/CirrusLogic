#include "stm8l10x.h"
#include "i2c_master_poll.h"
#include "serial.h"
#include "packet.h"
#include "system.h"
#include "string.h"
#include "iostm8l.h"


volatile uint8_t rx_write_index;					// global UART receive ring buffer write index
uint8_t rx_read_index;								// global UART receive ring buffer read index

@near volatile uint8_t rx_buffer[MAX_UART_LENGTH];	// global UART receive ring buffer
@eeprom uint8_t LAM_Config;					//literal address 0x9FC0


uint8_t LAM_busy;									// global LAM busy flag
uint8_t EA_open;									// global EA session open flag
uint8_t pending_launch;						// global flag indicating need to request launch
uint8_t DAC_EN;								  // global flag indicating DAC enabled
uint8_t ADC_EN;									// global flag indicating ADC enabled
uint8_t LED_ON;									// global flag indicating LED is off for low power mode
uint8_t WAITING;								// global flag indicating 1 sec timer for SDA
uint8_t Active_Config;						// global flag indicating configuration is being performed
uint8_t test_LEN;							// **DEBUG Only

uint8_t pending_en_ch;						// global flag indicating need to change enable state
uint8_t Tip_plugged;	  					// global flag indicating status of 3.5mm jack
uint8_t HP_Only;								  // global flag indicating headphone only (no mic)
uint8_t Optical_Only;						  // global flag indicating optical only (no capability)
uint8_t hs_type_rd; 						  // global flag of most recent headset type reading


volatile uint8_t timer2_count;						// global TIM2 count divider
volatile uint16_t TIM4_tout;						// global TIM4 tick counter (from AN3281)

void unlock_eeprom(void)

{
    char temp;
    temp = FLASH_IAPSR;
    while (!(temp & 0x08))
    {   
        FLASH_DUKR = 0xAE;
        FLASH_DUKR = 0x56;
        temp = FLASH_IAPSR;     
    }
    return;
}



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

	unlock_eeprom ();
	//LAM_Config = 0;						// **DEBUG Only - test 
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
	hs_type_rd = 0x5A;						//vlaue to display before reading
	
	if (!LAM_Config) {	
		Active_Config=1;
		//LAM_Config = 1;						 	// set non-volatile flag indicating configuration complete
	}	
	else Active_Config=0;


	// peripheral initialization ----------------------------------------------

	CLK_MasterPrescalerConfig (CLK_MasterPrescaler_HSIDiv1);

	serial_init ();									// initialize UART
	timer2_init ();									// initialize TIM2
	timer3_init ();									// initialize TIM3

	// enable I2C peripheral clock
	CLK_PeripheralClockConfig (CLK_Peripheral_I2C, ENABLE);

	// configure I2C peripheral
	I2C_Cmd (ENABLE);
	I2C_Init (100000, 0x00A0, I2C_DutyCycle_2, I2C_Ack_Enable, I2C_AcknowledgedAddress_7bit);
	I2C_ITConfig (I2C_IT_ERR, ENABLE);

	pin_setup ();									// initialize GPIO
	EXTI->CR2 = 0x01;
	GPIO_ResetBits (GPIOB, GPIO_Pin_2);		// MCU_PB2 set low **DEBUG Only

	GPIO_ResetBits (GPIOB, GPIO_Pin_7);  // reset codec
	GPIO_SetBits (GPIOB, GPIO_Pin_7);				// take codec out of reset
	//strcpy (test_payload, "1234");	//**DEBUG Only

	//test_LEN = strlen (test_payload) + 1;	//**DEBUG Only
	//test_LEN = test_LEN + 1;	//**DEBUG Only

	//if (LAM_Config) GPIO_ResetBits (GPIOB, GPIO_Pin_3);	//**DEBUG Only
	
	enableInterrupts ();							// enable interrupts
	if (!Active_Config){
		codec_init();
		if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_1) != RESET | GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_3) != RESET){		//true when plugged | true when CRD
			Tip_plugged = 1;								// set flag showing plugged 
			//codec_init();
			hs_type_det ();									//initial detection if plugged at power up
			DAC_EN = 0;									  // will be enabled 
		} else {                        //here because it's not plugged
			Tip_plugged = 0;							// reset flag 
			HP_Only = 1;									// no mic
		}		
	}
/*
				hs_type_det ();									//initial detection if plugged at power up
				serial_print_char (hs_type_rd);			// **DEBUG Only
				codec_init();
				hs_type_det ();									//initial detection if plugged at power up
				serial_print_char (hs_type_rd);			// **DEBUG Only
				codec_pwr_up();
				codec_reg_write (0x00, 0x1D);					// set page 0x1D
				codec_reg_write (0x04, 0x79);					//ADC wind noise filter enabled 
																							//the code is not shutting off the WNF
				codec_reg_write (0x00, 0x23);					// set page 0x23
				codec_reg_write (0x01, 0xFF);					//CHA_Vol = MUTE
				codec_reg_write (0x03, 0xFF);					//CHB_Vol = MUTE
				codec_reg_write (0x00, 0x1D);					// set page 0x1D
//				codec_reg_write (0x02, 0x06);					//ADC soft-ramp enable
				codec_reg_write (0x03, 0x06);					//ADC_VOL = 6dB,
				codec_reg_write (0x01, 0x01);					//ADC_DIG_BOOST; +20dB, 
				codec_reg_write (0x00, 0x23);					// set page 0x23
				codec_reg_write (0x02, 0x00);					//ADC_Vol = 0dB, 
				
				while (1){							//**DEBUG
				}	
*/
	// main loop --------------------------------------------------------------

	while (1)
	{
		if (!Active_Config){
			// if this is a CDB monitor state of plug 
			//Remove the instruction below for the special first time test code.
			//if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_3) == RESET) plug_state = next_plug_state (plug_state);
			// monitor state of side button
			button_state = next_button_state (button_state);
			// monitor state of spare button
			SP_button_st = next_sp_button_st (SP_button_st);
		}

		// determine what packet, if any, must be sent next
		current_packet = next_packet (current_packet);
		
		// determine if configuration just completed
		if (Active_Config) {	
			GPIO_SetBits (GPIOB, GPIO_Pin_5);		// red ON**DEBUG Only
			GPIO_SetBits (GPIOB, GPIO_Pin_6);		// green ON**DEBUG Only
			if (current_packet == accAudioTerminalStateInformation){
				LAM_Config = 1;						 	// set non-volatile flag indicating configuration complete
				GPIO_SetBits (GPIOB, GPIO_Pin_2);		// MCU_IO2 set high **DEBUG Only

				GPIO_ResetBits (GPIOB, GPIO_Pin_5);		// red OFF
				GPIO_ResetBits (GPIOB, GPIO_Pin_6);		// green OFF
				GPIO_ResetBits (GPIOB, GPIO_Pin_2);		// MCU_PB2 set low **DEBUG Only
				GPIO_SetBits (GPIOB, GPIO_Pin_2);		// MCU_PB2 set high **DEBUG Only
				GPIO_ResetBits (GPIOB, GPIO_Pin_2);		// MCU_PB2 set low **DEBUG Only
				Active_Config=0;
				Tip_plugged = 0;							// reset flag 
				HP_Only = 1;									// no mic
			}
		}	

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