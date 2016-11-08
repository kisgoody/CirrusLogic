/*	BASIC INTERRUPT VECTOR TABLE FOR STM8 devices
 *	Copyright (c) 2015 Cirrus Logic, Inc.
 */
#include "main.h"

@far @interrupt void NonHandledInterrupt (void)
{
	/* in order to detect unexpected events during development, 
	   it is recommended to set a breakpoint on the following instruction
	*/
	return;
}

extern void _stext();     /* startup routine */
struct interrupt_vector const FW_ISR_IRQ[32] @ FW_START_ADDR;

//redirected interrupt table
struct interrupt_vector const _vectab[] = {
    {0x82, (interrupt_handler_t)_stext}, /* reset */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 1)}, /* trap  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 2)}, /* irq0  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 3)}, /* irq1  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 4)}, /* irq2  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 5)}, /* irq3  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 6)}, /* irq4  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 7)}, /* irq5  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 8)}, /* irq6  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+ 9)}, /* irq7  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+10)}, /* irq8  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+11)}, /* irq9  */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+12)}, /* irq10 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+13)}, /* irq11 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+14)}, /* irq12 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+15)}, /* irq13 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+16)}, /* irq14 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+17)}, /* irq15 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+18)}, /* irq16 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+19)}, /* irq17 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+20)}, /* irq18 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+21)}, /* irq19 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+22)}, /* irq20 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+23)}, /* irq21 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+24)}, /* irq22 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+25)}, /* irq23 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+26)}, /* irq24 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+27)}, /* irq25 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+28)}, /* irq26 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+29)}, /* irq27 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+30)}, /* irq28 */
    {0x82, (interrupt_handler_t)(FW_ISR_IRQ+31)}, /* irq29 */
};
