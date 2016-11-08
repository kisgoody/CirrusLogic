#include "stm8l10x.h"
#include "packet.h"
#include "serial.h"
#include "system.h"
#include "string.h"
#include "version.h"

// packet state machine traversal function ------------------------------------
uint8_t next_packet (uint8_t current_packet)
{

	static uint8_t pending_packet;
	uint8_t return_packet;

	@near uint8_t tx_payload[MAX_PACKET_LENGTH - 4];

	switch (current_packet)
	{

	case accReady:

		packet_send (1, accReady, 0);

		timer2_count = 0;				// initialize 1-sec timer
		TIM2_Cmd (ENABLE);				// start 1-sec timer

		return_packet = accSpecialInitWait;
		break;

	case accSpecialInitWait:

		// query if LAM has responded
		if (!LAM_busy)
		{

			// since LAM has responded, move forward with operation or configuration
			return_packet = LAM_Configured? accAudioTerminalStateInformation : accConfigurationInformation;

		} else {

			// since LAM has not responded, issue another accReady packet after 1 sec
			return_packet = (TIM2_GetStatus () == ENABLE) ? accSpecialInitWait : accReady;

		} // if (!LAM_busy)

		break;

	case accConfigurationInformation:

		tx_payload[0] = 0x01;			// LAM protocol version (fixed)
		tx_payload[1] = 0x01;			// accessory power mode (constant)
		tx_payload[2] = 0x01;			// I2S role (host)
		tx_payload[3] = 0x00;			// reserved (fixed)
		tx_payload[4] = 0x00;			// UART baud rate (57600 bps)
		tx_payload[5] = HW_MAJOR_VER;	// hardware major version
		tx_payload[6] = HW_MINOR_VER;	// hardware minor version
		tx_payload[7] = HW_REVISION;	// hardware revision
		tx_payload[8] = FW_MAJOR_VER;	// firmware major version
		tx_payload[9] = FW_MINOR_VER;	// firmware minor version
		tx_payload[10] = FW_REVISION;	// firmware revision

		pending_packet = accAudioTerminalStateInformation;
		return_packet = accSpecialConfigWait;

		FLASH_Unlock(FLASH_MemType_Data);
		while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET);
		LAM_Configured = 1;
		FLASH_Lock(FLASH_MemType_Data);
		
		packet_send (12, accConfigurationInformation, tx_payload);

		break;

	case accAudioTerminalStateInformation:

		if (!Tip_plugged | Optical_Only) {tx_payload[0] = 0x00;}	// audio output terminal state (disabled)
		else {tx_payload[0] = 0x01;}							// audio output terminal state (enabled)
		
		if (HP_Only | Optical_Only) {tx_payload[1] = 0x00;}	// audio input terminal state (disabled)
		else {tx_payload[1] = 0x01;}							// audio input terminal state (enabled)

		packet_send (3, accAudioTerminalStateInformation, tx_payload);
		pending_en_ch = 0;						// reset pending_launch flag

		return_packet = accSpecialOperational;
		break;

	case accRequestAppLaunch:			// request device to launch app

		packet_send (1, accRequestAppLaunch, 0);
		pending_launch = 0;						// reset pending_launch flag
																	//we are not testing that device acknowledged
		return_packet = accSpecialOperational;
		break;

	case accSpecialOperational:
		// LAM sets SDA low when it's time to go to sleep. 
		// Turning off the LED conserves power.
		GPIO_ResetBits (GPIOB, GPIO_Pin_2);		// MCU_IO2 set low **DEBUG Only

		if (GPIO_ReadInputDataBit (GPIOC, GPIO_Pin_4) == RESET){ //SDA=0
			if (LED_ON){																					 //LED still ON	
				if (!WAITING){																			 //timer not started
					WAITING = 1;						//start timer and start waiting
					GPIO_ResetBits (GPIOB, GPIO_Pin_3);				//##DEBUG Only

					timer2_count = 50;				// initialize 1-sec timer for half sec
					TIM2_Cmd (ENABLE);				// start 1-sec timer
				} else {
						if (TIM2_GetStatus () != ENABLE) {
							LED_ON = 0;					//timer is done so turn LED off
							WAITING = 0;						//no longer waiting
							//Tip_plugged = 0;				// required to start up after sleep 
							// pendant board
							GPIO_ResetBits (GPIOB, GPIO_Pin_6);		// green OFF
							GPIO_ResetBits (GPIOB, GPIO_Pin_5);		// red OFF
							halt();                               // reduce power when SDA pad low
						}
					}
			}
		}
		else {
			if (!LED_ON){
				GPIO_SetBits (GPIOB, GPIO_Pin_5);		// red ON
				LED_ON = 1;
			}			
		}		
		GPIO_SetBits (GPIOB, GPIO_Pin_2);		// MCU_IO2 set low **DEBUG Only

		return_packet = accSpecialOperational;
		if (pending_launch)return_packet = accRequestAppLaunch;
		if (pending_en_ch) return_packet = accAudioTerminalStateInformation;
		break;

	case accSpecialConfigWait:

		return_packet = LAM_busy ? accSpecialConfigWait : pending_packet;
		break;

	case accSpecialUnknown:

		return_packet = accSpecialUnknown;
		break;

	default:

		return_packet = accSpecialUnknown;

	} // switch (current_packet);

	return (return_packet);

} // --------------------------------------------------------------------------


// packet checksum calculator -------------------------------------------------
int8_t packet_checksum (uint8_t packet_length, uint8_t packet_type, uint8_t *packet_payload)
{

	int8_t temp_sum;
	uint8_t i;

	temp_sum = (int8_t)packet_length + (int8_t)packet_type;

	for (i = 0; i < (packet_length - 1); i++) temp_sum += (int8_t)packet_payload[i];

	return (-temp_sum);

} // --------------------------------------------------------------------------


// receive packet processing function -----------------------------------------
void packet_process (uint8_t packet_length, uint8_t packet_type, uint8_t *packet_payload)
{

	@near uint8_t EA_return_packet[MAX_PACKET_LENGTH - 4];

	// take action based on received packet type
	switch (packet_type)
	{

	case lamReady:

		LAM_busy = 0;
		break;

	case lamInformation:

		// update EA session status
		EA_open = packet_payload[0];

		// update playback indicator LEDs
		if (packet_payload[3] == 0x01)				// playing
		{

			// issue codec initialization if necessary
			if (!DAC_EN)
			{
				GPIO_SetBits (GPIOB, GPIO_Pin_6);		// green ON**DEBUG Only

				codec_pwr_up();
				// pendant board
				GPIO_ResetBits (GPIOB, GPIO_Pin_5);		// red OFF
				GPIO_SetBits (GPIOB, GPIO_Pin_6);		// green ON

				
				if (GPIO_ReadInputDataBit (GPIOD, GPIO_Pin_0) == RESET)
					{
						// enable mic test mode for loopback test
						// ADC_test_config
						codec_reg_write (0x00, 0x1D);					// set page 0x1D
						codec_reg_write (0x03, 0x06);					//ADC_VOL = 6dB,
						codec_reg_write (0x01, 0x00);					//no ADC_DIG_BOOST
						codec_reg_write (0x00, 0x23);					// set page 0x23
						codec_reg_write (0x02, 0xFF);					//no sidetone
						while (1){							//**DEBUG
						}	
					}
			} 

		} else {									// not playing
			if (DAC_EN)
			{

				codec_pwr_dn ();
				
				// pendant board
				GPIO_ResetBits (GPIOB, GPIO_Pin_6);		// green OFF
				GPIO_SetBits (GPIOB, GPIO_Pin_5);		// red ON
	
		}
		} // if (playing)

		if (packet_payload[4] == 0x01)	// iOS requests record
		{
			if (!ADC_EN)
			{
				//hs_type_det (); 
				ADC_pwr_up ();
			}

		} else {									// not recording
			if (ADC_EN)
			{
				ADC_pwr_dn ();						//ADC has now been turned off
			}

		} // if (recording)


		break;

	case lamAudioTerminalInformation:

		// apply mute or adjust playback volume 
		if (packet_payload[0])
		{
				//DAC_mute
				codec_reg_write (0x00, 0x23);					// set page 0x23
				codec_reg_write (0x01, 0xFF);					//CHA_Vol = MUTE
				codec_reg_write (0x03, 0xFF);					//CHB_Vol = MUTE
		}	else	{
				//DAC_volume_A_B
				codec_reg_write (0x00, 0x23);					// set page 0x23
				codec_reg_write (0x01, (0 - packet_payload[1]));
				codec_reg_write (0x03, (0 - packet_payload[1]));
		}			
		break;

	case lamExternalAccessoryProtocolSessionData:

		// verify that EA session is open and length is valid
		if (EA_open) {
			if ((packet_payload[0] == EA_STATUS) && (packet_length == 2)) {
				EA_return_packet[0] = EA_STATUS;				// start of packet
				EA_return_packet[1] = EA_STATUS_FW_RUNNING;
				EA_return_packet[2] = FW_Version[0];
				EA_return_packet[3] = FW_Version[1];
				EA_return_packet[4] = FW_Version[2];
				packet_send (6, accExternalAccessoryProtocolSessionData, EA_return_packet);
			} else if ((packet_payload[0] == EA_LOAD_FW) && (packet_length == 2)) {
				_asm("JPF $8000"); //jump to bootloader code
			} else if ((((packet_payload[0] == EA_WRITE) && (packet_length > 5) && (packet_length == (packet_payload[3] + 5))) || ((packet_payload[0] == EA_READ) && (packet_length == 5))) && (packet_payload[3] > 0) && (packet_payload[3] < 125)) {

				// issue write if necessary
				if (packet_payload[0] == EA_WRITE)
				{
	
					// set page
					codec_reg_write (0x00, packet_payload[1]);
	
					// write data
					codec_reg_write_mult (packet_payload[2], packet_payload[3], &packet_payload[4]);
	
				} // if (write)
	
				// populate return packet header
				EA_return_packet[0] = EA_READ;				// start of packet
				EA_return_packet[1] = packet_payload[1];	// page
				EA_return_packet[2] = packet_payload[2];	// address
	
				// return blank packet for page 0xBD, or read packet for all others
				if ((packet_payload[0] == EA_WRITE) && (packet_payload[1] == 0xBD))
				{
	
					// return zero length
					EA_return_packet[3] = 0x00;
	
				} else {
	
					// return matching length
					EA_return_packet[3] = packet_payload[3];
	
					// set page
					codec_reg_write (0x00, packet_payload[1]);
	
					// read data
					codec_reg_read_mult (packet_payload[2], packet_payload[3], &EA_return_packet[4]);
	
				} // if (page 0xBD special case)
	
				// send return packet
				packet_send ((EA_return_packet[3] + 5), accExternalAccessoryProtocolSessionData, EA_return_packet);
	
				// return to user space
				codec_reg_write (0x00, 0x00);
	
			} else { //app waits for some responce
				EA_return_packet[0] = EA_STATUS;
				EA_return_packet[1] = EA_STATUS_FAILED;
				EA_return_packet[2] = EA_FAILED_REQUEST;
				EA_return_packet[3] = packet_payload[0];	// invalid data tag
				packet_send (5, accExternalAccessoryProtocolSessionData, EA_return_packet);
			}
		}
		break;

	case lamHIDReport:

		/* not supported, do nothing */
		break;

	default:

		/* unrecognized packet, do nothing */
		;

	} // switch (packet_type)

} // --------------------------------------------------------------------------


// packet transmit function ---------------------------------------------------
void packet_send (uint8_t packet_length, uint8_t packet_type, uint8_t *packet_payload)
{

	uint8_t i;

	// set LAM busy flag for initialization and configuration packets
	if (packet_type < lamInformation) LAM_busy = 1;
	

	// send packet header
	serial_print_char (START_OF_PACKET);
	serial_print_char (packet_length);
	serial_print_char (packet_type);

	// send packet payload
	for (i = 0; i < (packet_length - 1); i++) serial_print_char (packet_payload[i]);

	// send checksum
	serial_print_char ((uint8_t)packet_checksum (packet_length, packet_type, packet_payload));

} // --------------------------------------------------------------------------


// packet receive function ----------------------------------------------------
uint8_t packet_receive (uint8_t packet_index, uint8_t packet_byte, uint8_t *packet_length, uint8_t *packet_type, uint8_t *packet_payload)
{

	uint8_t temp_index;
	temp_index = packet_index;

	// parse packets received from LAM
	switch (temp_index)
	{

	case 0:			// looking for start of packet

		if (packet_byte == START_OF_PACKET) temp_index++;
		break;

	case 1:			// expect next byte to be length field

		*packet_length = packet_byte;
		temp_index++;
		break;

	case 2:			// expect next byte to be type field

		*packet_type = packet_byte;
		temp_index++;
		break;

	default:		// all other bytes could be payload or checksum

		// look for checksum or invalid length, or simply record byte
		if (temp_index == (*packet_length + 2))
		{

			// process packet if checksum was valid
			if (packet_checksum (*packet_length, *packet_type, packet_payload) == (int8_t)packet_byte) packet_process (*packet_length, *packet_type, packet_payload);

			// reset location marker whether packet was valid or not
			temp_index = 0;

		} else if (temp_index >= MAX_PACKET_LENGTH) {

			// reset location marker (presumably off in the weeds)
			temp_index = 0;

		} else {

			// record byte and increment location marker
			packet_payload[(temp_index++) - 3] = packet_byte;

		} // if (temp_index)

	} // switch (temp_index)

	// report updated location marker
	return (temp_index);

} // --------------------------------------------------------------------------