/* MAIN.C file
 * 
 * Copyright (c) 2015 Cirrus Logic, Inc.
 */
#include "main.h"

// LAM packet header offsets
#define LAM_PKT_TAG_OFFS	0
#define LAM_PKT_LEN_OFFS	1
#define LAM_PKT_TYPE_OFFS	2
#define LAM_PKT_DATA_OFFS	3

#define LAM_PKT_MAX_SZ 132

// LAM protocol packet types
#define ACC_READY	1
#define LAM_READY	2
#define LAM_INFO	0x10
#define APP_DATA	0x12
#define ACC_DATA	0x20
#define ACC_REQUEST_APP_LAUNCH 0x22

// accessory protocol packet types
#define ACC_PROTOCOL_STATUS			1
#define ACC_PROTOCOL_LOAD_FW		2
#define ACC_PROTOCOL_FLASH_BLK	3
#define ACC_PROTOCOL_BOOT_FW		4
#define ACC_PROTOCOL_CANCEL			5

// accessory protocol status codes
#define STATUS_SUCCESS					0
#define STATUS_FW_IS_RUNNING		1
#define STATUS_WAITING_FW_DATA	2
#define STATUS_FAILED						3

// failed status details
#define FAILED_FLASH		1
#define FAILED_CHECKSUM	2
#define FAILED_LENGTH		3
#define FAILED_REQUEST	4
#define FAILED_TIMEOUT	5

#define WAIT_NEXT_PACKET	0
#define LAST_PACKET				1

int _fctcpy(char name);
uint8_t app_button_long_pressed(void);
uint8_t fw_is_present(void);
void load_fw(void);
void wait_4XXms_times(uint8_t n);

extern struct interrupt_vector const FW_ISR_IRQ[32] @ FW_START_ADDR;
extern void (fw_start)(void) @ FW_START_ADDR;

uint8_t lam_ready;
uint8_t app_ready;
uint8_t flashing_in_progress;
uint8_t lam_packet[LAM_PKT_MAX_SZ];

uint16_t wait_unit;
uint8_t wait_cnt;

void main(void)
{
  disableInterrupts();

	/* peripheral initialization */
	CLK_MasterPrescalerConfig(CLK_MasterPrescaler_HSIDiv1);
	serial_init();
	pin_setup();
	
	_fctcpy('F');
	
	lam_ready = 0;
	app_ready = 0;
	
	if ((uint8_t)(RST->SR & (uint8_t)(RST_FLAG_POR_PDR | RST_FLAG_SWIMF | RST_FLAG_ILLOPF | RST_FLAG_IWDGF)) == 0x00) {
		// no any reset/restart flags set, so
		// this is callback from FW on FW update command from app
		lam_ready = 1;
		app_ready = 1;
		
		load_fw();
	} else if (app_button_long_pressed()) {
		//user forced FW flash
		RST->SR = (uint8_t)(RST_FLAG_POR_PDR);//not just powered
		load_fw();
	}

	while (1) {
		flashing_in_progress = 0;
		if (fw_is_present()) {
			// clear all reset/restart flags
			RST->SR = (uint8_t)(RST_FLAG_POR_PDR | RST_FLAG_SWIMF | RST_FLAG_ILLOPF | RST_FLAG_IWDGF);
			
			fw_start();
			//_asm("JPF $88C0"); //jump to FW code
		}

		load_fw();
	}
}

uint8_t fw_is_present(void) {
	uint8_t i;
	for (i = 0; i < 32; i++) {
    if ((FW_ISR_IRQ[i].interrupt_instruction != 0x82) 
				&& (FW_ISR_IRQ[i].interrupt_instruction != 0xAC)) {
			return 0;
		}
	}
	return 1;
}

uint8_t checksum_lam_packet(void) {
	uint8_t i;
	uint8_t chksum = 0;
	for (i = 0; i <= lam_packet[LAM_PKT_LEN_OFFS]; i++) {
		chksum += lam_packet[LAM_PKT_LEN_OFFS+i];
	}
	return (uint8_t)(-chksum);
}

uint8_t receive_lam_packet(void) {
	uint8_t n = 0;
	uint8_t ret = 0xff;
	lam_packet[LAM_PKT_LEN_OFFS] = 0;
	//we may receive data while not reading it; clear it
	if ((USART->SR & (uint8_t)USART_FLAG_OR) != (uint8_t)0x00) {
		lam_packet[0] = USART->DR;
	}
	while (1) {
		while ((USART->SR & (uint8_t)USART_FLAG_RXNE) == (uint8_t)0x00);
		lam_packet[n] = USART->DR;
		if (lam_packet[LAM_PKT_TAG_OFFS] == 0x55) {
			if (n == LAM_PKT_LEN_OFFS) {
				if (lam_packet[LAM_PKT_LEN_OFFS] == 0 || lam_packet[LAM_PKT_LEN_OFFS] > 129) {
					ret = FAILED_LENGTH;
					break;
				}
			} else if (n == lam_packet[LAM_PKT_LEN_OFFS] + 2) {
				if (checksum_lam_packet() == lam_packet[n]) {
					ret = STATUS_SUCCESS;
				} else {
					ret = FAILED_CHECKSUM;
				}
				break;
			}
			n++;
		}
	}
	return ret;
}

uint8_t receive_lam_packet_with_timeout(void) {
	uint8_t n = 0;
	uint8_t ret = 0x7f;
	lam_packet[LAM_PKT_LEN_OFFS] = 0;
	
	while (1) {
		wait_unit = 0;
		wait_cnt = 0;
		while ((USART->SR & (uint8_t)USART_FLAG_RXNE) == (uint8_t)0x00
						&& (wait_cnt < 21)) {
						if (++wait_unit == 0) {
							++wait_cnt;
						}
		}
		lam_packet[n] = USART->DR;
		if (wait_cnt > 20) { //timeout
			ret = FAILED_TIMEOUT;
			break;
		}
		if (lam_packet[LAM_PKT_TAG_OFFS] == 0x55) {
			if (n == LAM_PKT_LEN_OFFS) {
				if (lam_packet[LAM_PKT_LEN_OFFS] == 0 || lam_packet[LAM_PKT_LEN_OFFS] > 129) {
					ret = FAILED_LENGTH;
					break;
				}
			} else if (n == (lam_packet[LAM_PKT_LEN_OFFS] + 2)) {
				if (checksum_lam_packet() == lam_packet[n]) {
					ret = STATUS_SUCCESS;
				} else {
					ret = FAILED_CHECKSUM;
				}
				break;
			}
			n++;
		}
	}
	return ret;
}

void send_lam_packet(void) {
	uint8_t i;
	
	assert_param(lam_packet[LAM_PKT_LEN_OFFS] > 0 && lam_packet[LAM_PKT_LEN_OFFS] <= 129);
	
	lam_packet[LAM_PKT_TAG_OFFS] = 0x55;
	lam_packet[lam_packet[LAM_PKT_LEN_OFFS] + 2] = checksum_lam_packet();
	
	for (i = 0; i <= lam_packet[LAM_PKT_LEN_OFFS] + 2; i++) {
		while ((USART->SR & (uint8_t)USART_FLAG_TXE) == (uint8_t)0x00);
		USART->DR = lam_packet[i];
	}
}

/**
  * @brief Sends no-payload info or request to lam
	*
	* Used for accessory ready status and app launch request
	*
	* @retval None
	*/
void send_to_lam(uint8_t type) {
	lam_packet[LAM_PKT_LEN_OFFS] = 1;			// no payload
	lam_packet[LAM_PKT_TYPE_OFFS] = type; // packet type
	send_lam_packet();
}

/**
  * @brief Sends status to application
	*
	* Used for reporting results of the requests from app
	*
	* @retval None
	*/
void send_to_app(uint8_t status, uint8_t detail) {
	lam_packet[LAM_PKT_LEN_OFFS] = 3;					// payload + type size
	lam_packet[LAM_PKT_TYPE_OFFS] = ACC_DATA;		// this is accessory protocol data packet type
	lam_packet[LAM_PKT_DATA_OFFS] = ACC_PROTOCOL_STATUS;	// this is accessory status data
	lam_packet[LAM_PKT_DATA_OFFS+1] = status;			// status itself
	lam_packet[LAM_PKT_DATA_OFFS+2] = detail;			// status itself
	send_lam_packet();
}

void erase_fw(void) {
	FLASH_EraseBlock((uint8_t)((FW_START_ADDR - FLASH_START_PHYSICAL_ADDRESS) / FLASH_BLOCK_SIZE));
	FLASH_WaitForLastOperation();
	FLASH_EraseBlock((uint8_t)(((FW_START_ADDR - FLASH_START_PHYSICAL_ADDRESS) / FLASH_BLOCK_SIZE) + 1));
	FLASH_WaitForLastOperation();
}

uint8_t flash_fw_block(uint8_t blk_num, uint8_t *fw_data) {
	uint8_t i;
	uint16_t addr;
	
	FLASH_ProgramBlock(blk_num, FLASH_ProgramMode_Standard, fw_data);
	FLASH_WaitForLastOperation();
	//if (FLASH_WaitForLastOperation() != FLASH_Status_Successful_Operation)
		//return STATUS_FAILED;
	
	addr = FLASH_START_PHYSICAL_ADDRESS + blk_num * FLASH_BLOCK_SIZE;
	for (i = 0; i < FLASH_BLOCK_SIZE; i++) {
		if (fw_data[i] != FLASH_ReadByte(addr + i)) {
			return STATUS_FAILED;
		}
	}
	
	return STATUS_WAITING_FW_DATA;
}

uint8_t process_lam_packet(void) {
	lam_ready = 1; //we received a packet so LAM is ready
	if (lam_packet[LAM_PKT_TYPE_OFFS] == APP_DATA) {
		app_ready = 1; //we received app packet so it is ready
		switch (lam_packet[LAM_PKT_DATA_OFFS]) {
			case ACC_PROTOCOL_STATUS:
			case ACC_PROTOCOL_LOAD_FW:
				send_to_app(STATUS_WAITING_FW_DATA, 0);
				break;
				
			case ACC_PROTOCOL_FLASH_BLK:
				if (!flashing_in_progress) {
					erase_fw();
					flashing_in_progress = 1;
				}
				send_to_app(
					flash_fw_block(lam_packet[LAM_PKT_DATA_OFFS+1], &lam_packet[LAM_PKT_DATA_OFFS+2]),
					0
					);
				break;
				
			case ACC_PROTOCOL_BOOT_FW:
			case ACC_PROTOCOL_CANCEL:
				return LAST_PACKET;
			
			default:
				send_to_app(FAILED_REQUEST, lam_packet[LAM_PKT_DATA_OFFS]);
		}
	} else {
		switch (lam_packet[LAM_PKT_TYPE_OFFS]) {
			case LAM_INFO:
				app_ready = lam_packet[LAM_PKT_DATA_OFFS];
				if (!app_ready && flashing_in_progress) {// app has disconnected, abort
					return LAST_PACKET;
				}
				break;
		}
	}
	return WAIT_NEXT_PACKET;
}

void load_fw(void) {
	uint8_t ret;
	
	FLASH_SetProgrammingTime(FLASH_ProgramTime_Standard);
	FLASH_Unlock(FLASH_MemType_Program);
	while (FLASH_GetFlagStatus(FLASH_FLAG_PUL) == RESET);
	FLASH_Unlock(FLASH_MemType_Data);
	while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET);

	while (!lam_ready) {
		send_to_lam(ACC_READY);
		if (receive_lam_packet_with_timeout() == STATUS_SUCCESS) {
			process_lam_packet();
		}
	}

	if (!app_ready) {
		if (RST->SR & (uint8_t)(RST_FLAG_POR_PDR)) { // just powered
			wait_4XXms_times(5); // need delay or otherwise LAM does not send this request
		}
		send_to_lam(ACC_REQUEST_APP_LAUNCH);
	} else {
		send_to_app(STATUS_WAITING_FW_DATA, 0);
	}
	
	while (1) {
		ret = receive_lam_packet();
		if (ret == STATUS_SUCCESS) {
			if (process_lam_packet() == LAST_PACKET) {
				break;
			}
		} else if (app_ready) {
			send_to_app(STATUS_FAILED, ret);
		}
	}

	FLASH_Lock(FLASH_MemType_Program);
	FLASH_Lock(FLASH_MemType_Data);
	FLASH_DeInit();
}

/* ~0.5s delay when CPU clk is 16MHz*/
void wait_4XXms_times(uint8_t n)
{
	for (; n > 0; n--) {
		wait_unit = 0;
		wait_cnt = 0;	
		while (++wait_cnt < 21) {
			while (++wait_unit != 0);
		}
	}
}

uint8_t app_button_long_pressed(void)
{
	uint8_t i;
	// button is pressed?
	if (GPIO_ReadInputDataBit (GPIOA, GPIO_Pin_2) == RESET)
	{
		// check if it is pressed for at least 5 seconds
		for (i = 0; i < 10; i++) {
			wait_4XXms_times(2);
			// check if button was released
			if (GPIO_ReadInputDataBit (GPIOA, GPIO_Pin_2) != RESET)
			{
					return 0; // released within 5 seconds
			}
		}
		return 1; // pressed for at least 5 seconds
	}
	return 0; // not pressed on start
}
