/**
  ******************************************************************************
  * @file    i2c_opt.h
  * @author  MCD Application Team
  * @version V0.0.3
  * @date    Feb 2010
  * @brief   This file contains definitions for optimized I2C software
  ******************************************************************************
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  *                     COPYRIGHT 2009 STMicroelectronics
  */

/* Define to prevent recursive inclusion */
#ifndef __I2C_OPT_H
#define __I2C_OPT_H

#include "stm8l10x.h"


// ************************** I2C Configuration Variables **************************

/* definition of fast or default standard mode (bus speed up to 400 or 100 kHz) */
//#define FAST_I2C_MODE

/* definition of 10-bit or default 7-bit slave address */
//#define TEN_BITS_ADDRESS

/* uncomment next line when stop request is required between device address sent and read data */
//#define NO_RESTART

#ifdef TEN_BITS_ADDRESS
#define SLAVE_ADDRESS  0x3F0
#else
#define SLAVE_ADDRESS  0x48						//	*** CIRRUS: changed to match CS47L42 ***
#endif

// ************************* Function Declaration ***************************


// void I2C_Init(void);								*** CIRRUS: removed to avoid conflict with standard library ***
void TIM4_Init(void);
void I2C_RandomRead(u8 u8_NumByteToRead, u8 *u8_ReadBuffer);
void I2C_ReadRegister(u8 u8_regAddr, u8 u8_NumByteToRead, u8 *u8_ReadBuffer);
void I2C_WriteRegister(u8 u8_regAddr, u8 u8_NumByteToWrite, u8 *u8_DataBuffer);
void ErrProc(void);

/* flag clearing sequence - uncoment next for peripheral clock under 2MHz */
#define dead_time() { /* _asm("nop"); _asm("nop"); */ }
#define delay(a)          { TIM4_tout= a; while(TIM4_tout); }
#define tout()            (1)//(TIM4_tout)			*** CIRRUS: bypassed to support lengthy transactions ***
#define set_tout_ms(a)    { TIM4_tout= a; }
extern volatile u16 TIM4_tout;					//	*** CIRRUS: changed to volatile to match main.c declaration ***


// ************************* Interrupt handler Declaration ***************************

#ifdef _COSMIC_
@far @interrupt void I2CInterruptHandler (void);
#else
void I2CInterruptHandler (void) interrupt 29;
#endif
#ifdef _COSMIC_
@far @interrupt void TIM4InterruptHandler (void);
#else
void TIM4InterruptHandler (void) interrupt 25;
#endif

// ************************* I2C Example decalaration ***************************
/*
#define MAX_DUMMY 10								*** CIRRUS: removed, does not apply ***
#define LED1  0x10
#define LED2  0x20
#define LED3  0x40
#define LED4  0x80
#define switch_on(msk) GPIOA->ODR &= ~(msk);
#define switch_off(msk) GPIOA->ODR |= (msk);
*/

#endif /* __I2C_OPT_H */

/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
