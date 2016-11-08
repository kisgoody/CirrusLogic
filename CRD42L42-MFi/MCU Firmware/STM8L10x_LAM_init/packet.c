#include "stm8l10x.h"
#include "packet.h"
#include "serial.h"
#include "system.h"
#include "string.h"
#include "stdio.h"
#include "version.h"

uint32_t serial_num @0x9ffc;

// packet state machine traversal function ------------------------------------
uint8_t next_packet (uint8_t current_packet)
{

	static uint8_t pending_packet;
	uint8_t return_packet;
	char snum[10];

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
			return_packet = !Active_Config ? accAudioTerminalStateInformation : accConfigurationInformation;
			//return_packet = accAudioTerminalStateInformation;

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

		packet_send (12, accConfigurationInformation, tx_payload);

		pending_packet = accNameInformation;
		return_packet = accSpecialConfigWait;

	break;

	case accNameInformation:

		strcpy(tx_payload, INFO_NAME);

		packet_send (13, accNameInformation, tx_payload);

		pending_packet = accManufacturerInformation;
		return_packet = accSpecialConfigWait;
		break;


	case accManufacturerInformation:

		strcpy (tx_payload, INFO_MANF);

		packet_send (13, accManufacturerInformation, tx_payload);

		pending_packet = accModelInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accModelInformation:

		strcpy (tx_payload, INFO_MODEL);

		packet_send (8, accModelInformation, tx_payload);

		pending_packet = accSerialNumberInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accSerialNumberInformation:

		sprintf(snum, "%08lX", serial_num);
		memcpy (tx_payload, snum, 8);

		packet_send (9, accSerialNumberInformation, tx_payload);

		pending_packet = accPreferredAppBundleIdentifierInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accPreferredAppBundleIdentifierInformation:

		strcpy (tx_payload, INFO_PREF_APP);

		packet_send (24, accPreferredAppBundleIdentifierInformation, tx_payload);

		pending_packet = accExternalAccessoryProtocolNameInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accExternalAccessoryProtocolNameInformation:

		strcpy (tx_payload, INFO_EA_NAME);

		packet_send (24, accExternalAccessoryProtocolNameInformation, tx_payload);

		pending_packet = accHIDComponentInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accHIDComponentInformation:

		tx_payload[0] = 0x08;			// fixed
		tx_payload[1] = 0x00;			// null (not supported)
		tx_payload[2] = 0x00;			// null (not supported)

		packet_send (4, accHIDComponentInformation, tx_payload);

		pending_packet = accAudioTerminalInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accAudioTerminalInformation:

		tx_payload[0] = 0x02;			// audio output terminal type (jack)
		//tx_payload[0] = 0x01;			// audio output terminal type (headset)
		tx_payload[1] = 0x00;			// audio output terminal latency [15:8]
		tx_payload[2] = 0x04;			// audio output terminal latency [7:0] (4.2 samples)
		tx_payload[3] = 0x01;			// audio output terminal gain/attn. step size (1.0 dB)
		tx_payload[4] = 0x00;			// audio output terminal gain steps 0 indicates 1, cannot be set to zero
		tx_payload[5] = 0x3D;			// audio output terminal attn. steps (62-1) 
		//tx_payload[6] = 0x00;			// audio input terminal type (no microphone)
		//tx_payload[6] = 0x01;			// audio input terminal type (headset microphone)
		tx_payload[6] = 0x02;			// audio input terminal type (headset jack)
		tx_payload[7] = 0x00;			// audio input terminal latency [15:8]
		tx_payload[8] = 0x05;			// audio input terminal latency [7:0] (4.6 samples)
		tx_payload[9] = 0x00;			// audio input terminal gain/attn. step size (not available)
		tx_payload[10] = 0x00;			// audio input terminal gain steps - 1
		tx_payload[11] = 0x00;			// audio input terminal attn. steps - 1

		tx_payload[12] = 0x00;			// preferred audio processing (none)

		packet_send (14, accAudioTerminalInformation, tx_payload);

		pending_packet = accAudioTerminalStateInformation;
		return_packet = accSpecialConfigWait;
		break;

	case accAudioTerminalStateInformation:

		tx_payload[0] = 0x00;	// audio output terminal state (disabled)
		tx_payload[1] = 0x00;	// audio input terminal state (disabled)

		packet_send (3, accAudioTerminalStateInformation, tx_payload);
		pending_en_ch = 0;						// reset pending_launch flag

		return_packet = accSpecialOperational;
		break;

	case accVersionInformation:			// not used in current LAM

		//for (i = 0; i < 33; i++) wait_15ms ();			//**DEBUG Only

		packet_send (1, accVersionInformation, 0);

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
			GPIO_SetBits (GPIOB, GPIO_Pin_6);		// green ON**DEBUG Only
		} else {									// not playing
			GPIO_ResetBits (GPIOB, GPIO_Pin_6);		// green OFF		
		} // if (playing)

		if (packet_payload[4] == 0x01)	// iOS requests record
		{
			GPIO_SetBits (GPIOB, GPIO_Pin_5);		// red ON**DEBUG Only

		} else {									// not recording
			GPIO_ResetBits (GPIOB, GPIO_Pin_5);		// red OFF**DEBUG Only
		} // if (recording)


		break;

	case lamAudioTerminalInformation:
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