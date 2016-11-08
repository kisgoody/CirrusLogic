#include "stm8l10x.h"
#include "system.h"
#include "i2c_master_poll.h"

// headset jack action function ------------------------------------------------
// if state has changed
// if something is plugged in, determine it's type
// if nothing is plugged in, inform LAM
uint8_t next_plug_state (uint8_t current_plug_state)
{

	uint8_t temp_state;
	uint8_t i;

	temp_state = current_plug_state;				// latch current state
		//using global Tip_plugged instead of current_plug_state

	if (!Tip_plugged) {												//flag shows not plugged
			if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_1) != RESET){		//true when plugged
				Tip_plugged = 1;								// set flag showing plugged 
				temp_state = 1;
				hs_type_det ();
				DAC_EN = 0; 							//next lamInformation will set LEDs
				pending_en_ch = 1;
				for (i = 0; i < 13; i++) wait_15ms ();			// wait for control port (195 ms)
			} 
	}	else {														//Tip_plugged
			if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_1) == RESET){	//true when not plugged
				//it was previously plugged but now it is not
				Tip_plugged = 0;							// reset flag 
				temp_state = 0;
				pending_en_ch = 1;
				GPIO_ResetBits (GPIOB, GPIO_Pin_6);		// green OFF
				GPIO_ResetBits (GPIOB, GPIO_Pin_5);		// red OFF
				DAC_EN = 0; 							//next lamInformation will set LEDs
				ADC_EN = 0; 							//next lamInformation will set LEDs
				HP_Only = 1;									// no mic
				// unplug
				codec_reg_write (0x00, 0x1b);					//set page
				codec_reg_write (0x70, 0x03);					// Reset Auto HSBIAS Clamp = Current Sense
			} 
	}

	return (temp_state);

} // --------------------------------------------------------------------------


// side button action function ------------------------------------------------
// if button is pressed set the pending_launch flag and change button_state to 0
// if button is released set button_state back to 1
uint8_t next_button_state (uint8_t current_button_state)
{

	uint8_t temp_state;

	temp_state = current_button_state;				// latch current state

	// determine if state must change
	if (temp_state)									// button thought to be released (1)
	{
		// check for press
		if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_4) == RESET)
		{
			wait_15ms ();							// delay (debounce)
			// check if button is still pressed
			if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_4) == RESET)
			{
				temp_state = 0;						// change state to pressed (0)
				pending_launch = 1;				// flag set
			} // if (actually pressed)
		} // if (possibly pressed)
	} else {										// button thought to be pressed (0)
		// check for release
		if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_4) != RESET)
		{
			wait_15ms ();							// delay (debounce)
			// check if button is still released
			if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_4) != RESET)
			{
				temp_state = 1;						// change state to released (1)
			} // if (actually released)
		} // if (possibly released)
	} // if (state)

	return (temp_state);

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

// codec power up function ----------------------------------------------
void codec_pwr_up (void)
{
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x02, 0xA4);					//Release Discharge cap, FILT+
	codec_reg_write (0x02, 0xA7);					//Power Down Control 2
	codec_reg_write (0x01, 0x00);					//power up ASP, Mixer, HP, and DAC
	codec_reg_write (0x00, 0x12);					// set page 0x12
	codec_reg_write (0x07, 0x2C);					//Enable ASP SCLK, LRCK input, SCPOL_IN (ADC and DAC) inverted
	codec_reg_write (0x00, 0x2A);					// set page 0x2A
	codec_reg_write (0x01, 0x0C);					//Enable Ch1/2 DAI
	codec_reg_write (0x00, 0x20);					// set page 0x20
	codec_reg_write (0x01, 0x01);					//unmute analog A and B, not -6dB mode
	DAC_EN = 1;	    											// reset flag

	
} // --------------------------------------------------------------------------

// codec power down function ----------------------------------------------
void codec_pwr_dn (void)
{
// power-down sequence for CS42L42

	codec_reg_write (0x00, 0x23);					// set page 0x23
	codec_reg_write (0x01, 0x3F);					//CHA_Vol = MUTE
	codec_reg_write (0x03, 0x3F);					//CHB_Vol = MUTE
	codec_reg_write (0x02, 0xFF);					//Mixer ADC Input = MUTE
	codec_reg_write (0x00, 0x20);					// set page 0x20
	codec_reg_write (0x01, 0x0F);					//CHA, CHB = MUTE, FS VOL
	codec_reg_write (0x00, 0x2A);					// set page 0x2A
	codec_reg_write (0x01, 0x00);					//Disable ASP_TX
	codec_reg_write (0x00, 0x12);					// set page 0x12
	codec_reg_write (0x07, 0x00);					//Disable SCLK
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x01, 0xFE);					//Power down HP amp
	codec_reg_write (0x02, 0x8C);					//Power down ASP and SRC
	codec_reg_write (0x00, 0x13);					// set page 0x13
	wait_15ms ();													// delay codec power down

	// read data
	codec_reg_read  (0x08);

	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x02, 0x9C);					//Discharge cap, FILT+
	DAC_EN = 0;														// set flag
	ADC_EN = 1;	    											// reset flag
} // --------------------------------------------------------------------------

// ADC power up function ----------------------------------------------
void ADC_pwr_up (void)
{
	// enable mic
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x74, 0x07);					//HSBIAS set to 2.7V mode
	
	codec_pwr_up();
	// enable ASP
	codec_reg_write (0x00, 0x29);					// set page 0x29
	codec_reg_write (0x04, 0x00);					//Ch1 Bit Start UB
	codec_reg_write (0x05, 0x00);					//Ch1 Bit Start LB = 00
	codec_reg_write (0x0A, 0x00);					//Ch2 Bit Start UB
	codec_reg_write (0x0B, 0x00);					//Ch2 Bit Start LB = 00
	codec_reg_write (0x02, 0x01);					//enable ASP Transmit CH1
	codec_reg_write (0x03, 0x4A);					//RES=24 bits, CH2 and CH1
	codec_reg_write (0x01, 0x01);					//ASP_TX_EN
	codec_reg_write (0x02, 0x00);					//disable ASP Transmit CH2 and CH1
	codec_reg_write (0x02, 0x03);					//enable ASP Transmit CH2 and CH1
	// set volume
	codec_reg_write (0x00, 0x1C);					// set page 0x1C
	codec_reg_write (0x03, 0xC3);					//set HSBIAS_RAMP to slowest
	codec_reg_write (0x00, 0x1D);					// set page 0x1D
	codec_reg_write (0x02, 0x06);					//ADC soft-ramp enable
	codec_reg_write (0x03, 0x06);					//ADC_VOL = 6dB,
	codec_reg_write (0x01, 0x01);					//ADC_DIG_BOOST; +20dB, 
	codec_reg_write (0x00, 0x23);					// set page 0x23
	codec_reg_write (0x02, 0x19);					//Mixer ADC_Vol = -25dB, 
																				//Is this a good sidetone amount?
	ADC_EN = 1;						//reset flag
} // --------------------------------------------------------------------------

// ADC power up function ----------------------------------------------
void ADC_pwr_dn (void)
{
	// disable ADC (if currently enabled)
	codec_reg_write (0x00, 0x1D);					// set page 0x1D
	codec_reg_write (0x03, 0x9F);					//ADC_VOL = Mute, 0X9F is Mute
	codec_reg_write (0x01, 0x00);					//ADC_DIG_BOOST; 0x00 is no boost
	codec_reg_write (0x00, 0x23);					// set page 0x23
	codec_reg_write (0x02, 0x3F);					//Mixer ADC_Vol = Mute
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x74, 0x01);					//Turn off HSBIAS until requested by ADC
	ADC_EN = 0;						//reset flag

	
} // --------------------------------------------------------------------------

// codec initialization function ----------------------------------------------
void codec_init (void)
{

	uint8_t i;

	GPIO_SetBits (GPIOB, GPIO_Pin_7);				// take codec out of reset

	for (i = 0; i < 3; i++) wait_15ms ();			// wait for control port (40ms)

	codec_reg_write (0x00, 0x10);					// set page 0x10
	codec_reg_write (0x09, 0x02);					// MCLK Control: /256 mode DEF:0x02
	codec_reg_write (0x0A, 0xA4);					// DSR_RATE to ? DEF:0xA4
	codec_reg_write (0x00, 0x12);					// set page 0x12
	codec_reg_write (0x0C, 0x00);					//SCLK_PREDIV = 00 DEF:0x00
	codec_reg_write (0x00, 0x15);					// set page 0x15
	codec_reg_write (0x05, 0x40);					//PLL_DIV_INT = 0x40  DEF:0x40
	codec_reg_write (0x02, 0x00);					//PLL_DIV_FRAC = 0x000000 DEF:0x00
	codec_reg_write (0x03, 0x00);					//PLL_DIV_FRAC = 0x000000 DEF:0x00
	codec_reg_write (0x04, 0x00);					//PLL_DIV_FRAC = 0x000000 DEF:0x00
	codec_reg_write (0x1B, 0x03);					//PLL_MODE = 11 (bypassed) DEF:0x03
	codec_reg_write (0x08, 0x10);					//PLL_DIVOUT = 0x10 DEF:0x10
	codec_reg_write (0x0A, 0x80);					//PLL_CAL_RATIO = 128 DEF:0x80
	codec_reg_write (0x01, 0x01);					//power up PLL 
	codec_reg_write (0x00, 0x12);					// set page 0x12
	codec_reg_write (0x01, 0x01);					//MCLKint = internal PLL DEF:0x00
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x01, 0x00);					//power up ASP, Mixer, HP, and DAC DEF:0xFF
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x75, 0x9F);					//Latch_To_VP = 1 DEF:0xFF
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x07, 0x01);					//SCLK present
	codec_reg_write (0x00, 0x12);					// set page 0x12
	codec_reg_write (0x03, 0x1F);					//FSYNC High time LB. LRCK +Duty = 32 SCLKs
	codec_reg_write (0x04, 0x00);					//FSYNC High time UB DEF:0x00
	codec_reg_write (0x05, 0x3F);					//FSYNC Period LB. LRCK = 64 SCLKs
	codec_reg_write (0x06, 0x00);					//FSYNC Period UB DEF:0x00
	codec_reg_write (0x07, 0x2C);					//Enable ASP SCLK, LRCK input, SCPOL_IN (ADC and DAC) inverted
	codec_reg_write (0x08, 0x0A);					//frame starts high to low, I2S mode, 1 SCLK FSD
	codec_reg_write (0x00, 0x1F);					// set page 0x1F
	codec_reg_write (0x06, 0x02);					//Dac Control 2: Default
	codec_reg_write (0x00, 0x20);					// set page 0x20
	codec_reg_write (0x01, 0x01);					//unmute analog A and B, not -6dB mode
	codec_reg_write (0x00, 0x23);					// set page 0x23
	codec_reg_write (0x01, 0xFF);					//CHA_Vol = MUTE
	codec_reg_write (0x03, 0xFF);					//CHB_Vol = MUTE
	codec_reg_write (0x00, 0x2A);					// set page 0x2A
	codec_reg_write (0x01, 0x0C);					//Enable Ch1/2 DAI
	codec_reg_write (0x02, 0x02);					//Ch1 low 24 bit
	codec_reg_write (0x03, 0x00);					//Ch1 Bit Start UB
	codec_reg_write (0x04, 0x00);					//Ch1 Bit Start LB = 00
	codec_reg_write (0x05, 0x42);					//Ch2 high 24 bit
	codec_reg_write (0x06, 0x00);					//Ch2 Bit Start UB
	codec_reg_write (0x07, 0x00);					//Ch2 Bit Start LB = 00
	codec_reg_write (0x00, 0x26);					// set page 0x26
	codec_reg_write (0x01, 0x00);					//SRC in at don't know
	codec_reg_write (0x09, 0x00);					//SRC out at don't know
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x71, 0xC1);					//Toggle WAKEB_CLEAR
	codec_reg_write (0x71, 0xC0);					//Set WAKE back to normal
	codec_reg_write (0x75, 0x84);					//Set LATCH_TO_VP
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x29, 0x01);					//headset clamp disable
	codec_reg_write (0x02, 0xA7);					//Power Down Control 2

	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x74, 0xA7);					//Misc
	
/*
//codec_reg_write (0x00, 0x11);					// set page 0x11
	//codec_reg_write (0x21, 0xA6);					//set switches for Apple headset
	codec_reg_write (0x00, 0x1C);					// set page 0x1C
	codec_reg_write (0x03, 0xC3);					//set HSBIAS_RAMP to slowest
	codec_reg_write (0x00, 0x1D);					// set page 0x1D
	codec_reg_write (0x03, 0x9F);					//ADC_VOL = Mute
	codec_reg_write (0x01, 0x00);					//ADC_DIG_BOOST; 0x00 is no boost
	codec_reg_write (0x00, 0x23);					// set page 0x23
	codec_reg_write (0x02, 0x3F);					//ADC_Vol = Mute
	codec_reg_write (0x00, 0x29);					// set page 0x29
	codec_reg_write (0x04, 0x00);					//Ch1 Bit Start UB
	codec_reg_write (0x05, 0x00);					//Ch1 Bit Start LB = 00
	codec_reg_write (0x0A, 0x00);					//Ch2 Bit Start UB
	codec_reg_write (0x0B, 0x00);					//Ch2 Bit Start LB = 00
	codec_reg_write (0x02, 0x01);					//enable ASP Transmit CH1
	codec_reg_write (0x03, 0x4A);					//RES=24 bits, CH2 and CH1
	codec_reg_write (0x01, 0x01);					//ASP_TX_EN
	codec_reg_write (0x02, 0x00);					//disable ASP Transmit CH2 and CH1
	codec_reg_write (0x02, 0x03);					//enable ASP Transmit CH2 and CH1
*/

// codec initialize for plug detection function -------------------------------
// The WAKEB pin will be low when unplugged, high when plugged

	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x75, 0x9F);					//Latch_To_VP = 1
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x73, 0xE2);					//Write TIP_SENSE_CTRL for plug type
	codec_reg_write (0x71, 0xA0);					//Unmask WAKEB

	
	DAC_EN = 1; 							//next lamInformation will set LEDs
	for (i = 0; i < 33; i++) wait_15ms ();			// wait for plug detect (500ms)

} // --------------------------------------------------------------------------

// headset type detection function --------------------------------------------
// sets Optical_Only if something is plugged but no earpiece is detected
// sets	HP_Only if no mic is detected (headphones only, not a headset)
// the firmware doesn't need to know if the headset is AHJ or OMTP
// because the CS42L42 sets the switches internally
void hs_type_det (void)
{
	uint8_t i;
	uint8_t temp_rd;
	uint8_t auto_done;


	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x75, 0x9F);					//Set Latch_To_VP = 1
	// Configure the automatic headset-type detection.
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x29, 0x01);					//HS Clamp disabled
	codec_reg_write (0x01, 0xFE);					//PDN_ALL = 0
	codec_reg_write (0x21, 0x00);					//Set all switches to open
	codec_reg_write (0x00, 0x1F);					// set page 0x1F
	codec_reg_write (0x06, 0x86);					//Disable HPOUT_CLAMP and HPOUT_PULLDOWN when channels are powered down
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x74, 0x07);					//HSBIAS set to 2.7V mode
	//Wait for HSBIAS to ramp up, set to slowest at 0x1C03
	for (i = 0; i < 13; i++) wait_15ms ();			// wait for control port (195 ms)
	codec_reg_write (0x00, 0x13);					// set page 0x13
	codec_reg_write (0x1B, 0x01);					//HSDET interrupt unmasked
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x20, 0x80);					//HSDET_CTRL = Automatic, Disabled	
													//Wait 100us
	wait_15ms ();
	codec_reg_write (0x1F, 0x77);					//set COMPs 2V, 1V
	codec_reg_write (0x20, 0xC0);					//HSDET_CTRL = Automatic, Enabled (trigger a detect sequence)
													//Wait for interrupt
	// Service the HSDET_AUTO_DONE interrupt.
	codec_reg_write (0x00, 0x13);					// set page 0x13
	do
	{
		temp_rd = codec_reg_read  (0x08);			// read 0x1308
		if ((temp_rd & 0x02) == 0x02){		//test bit 1
			auto_done = 1;									// set flag showing it completed
		} else {
			wait_15ms ();							// delay 
		}	
	} while (!auto_done);					//wait until HSDET logic has finished its detection cycle
	codec_reg_write (0x00, 0x11);					// set page 0x11
	temp_rd = codec_reg_read (0x24);
	// Decode HSDET_TYPE
	
	// unplug
	codec_reg_write (0x00, 0x1b);					//set page
	codec_reg_write (0x70, 0x03);					// Reset Auto HSBIAS Clamp = Current Sense
	
	
	codec_reg_write (0x20, 0x80);					//HSDET mode set to auto, disabled
	// If headset type 1-3 is detected, the switches are set automatically.


	//codec_reg_write (0x00, 0x11);					// set page 0x11
	//temp_rd = codec_reg_read (0x24);			//was 0x24
	// Decode HSDET_TYPE
	Optical_Only = 0;							          //start with assumption
	HP_Only = 0;							          //start with assumption
	if (temp_rd & 0x01){
	} else {
		HP_Only = 1;							          //possibly HP_Only
	}	
	if (temp_rd & 0x02){
		Optical_Only = (temp_rd & 0x1);			//Only set if both bits are 1
	} else {
		HP_Only = 0;							          // a mic is present
	}	
	for (i = 0; i < 8; i++) wait_15ms ();			// wait for control port (120 ms)
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x74, 0x01);					//Turn off HSBIAS until requested by ADC

	
} // --------------------------------------------------------------------------


// Headset Manual Default function --------------------------------------------
void hs_manual (void)
{
	// manually apply AHJ (Type 1) switch states
	codec_reg_write (0x00, 0x1B);					// set page 0x1B
	codec_reg_write (0x75, 0x84);					// Set LATCH_TO_VP
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x02, 0xA7);					// Power Down Control 2
	codec_reg_write (0x00, 0x11);					// set page 0x11
	codec_reg_write (0x21, 0xA6);					// set switches for Apple headset
	codec_reg_write (0x20, 0x00);					// HSDET_CTRL = Manual, Disabled	
	Optical_Only = 0;							        // manually clear
	HP_Only = 0;							            // manually clear

	
} // --------------------------------------------------------------------------


// codec I2C register write function (single) ---------------------------------
void codec_reg_write (uint8_t reg, uint8_t value)
{

	// issue I2C write with length equal to 1
	codec_reg_write_mult (reg, 1, &value);

} // --------------------------------------------------------------------------


// codec I2C register write function (multiple) -------------------------------
void codec_reg_write_mult (uint8_t reg, uint8_t length, uint8_t *payload)
{

	// issue I2C write if programming shunt is absent
	I2C_WriteRegister (reg, length, payload);

} // --------------------------------------------------------------------------


// codec I2C register read function (single) ----------------------------------
uint8_t codec_reg_read (uint8_t reg)
{

	uint8_t temp_read;
	temp_read = 0x00;

	// fetch I2C read with length equal to 1
	codec_reg_read_mult (reg, 1, &temp_read);

	return (temp_read);

} // --------------------------------------------------------------------------


// codec I2C register read function (multiple) --------------------------------
void codec_reg_read_mult (uint8_t reg, uint8_t length, uint8_t *payload)
{

	// fetch I2C read if programming shunt is absent
	I2C_ReadRegister (reg, length, payload);

} // --------------------------------------------------------------------------