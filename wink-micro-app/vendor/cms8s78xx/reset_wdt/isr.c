/*******************************************************************************
* Copyright (C) 2019 China Micro Semiconductor Limited Company. All Rights Reserved.
*
* This software is owned and published by:
* CMS LLC, No 2609-10, Taurus Plaza, TaoyuanRoad, NanshanDistrict, Shenzhen, China.
*
* BY DOWNLOADING, INSTALLING OR USING THIS SOFTWARE, YOU AGREE TO BE BOUND
* BY ALL THE TERMS AND CONDITIONS OF THIS AGREEMENT.
*
* This software contains source code for use with CMS
* components. This software is licensed by CMS to be adapted only
* for use in systems utilizing CMS components. CMS shall not be
* responsible for misuse or illegal use of this software for devices not
* supported herein. CMS is providing this software "AS IS" and will
* not be responsible for issues arising from incorrect user implementation
* of the software.
*
* This software may be replicated in part or whole for the licensed use,
* with the restriction that this Disclaimer and Copyright notice must be
* included with each copy of this software, whether used in part or whole,
* at all times.
*/

#include "cms8s78xx.h"

void INT0_IRQHandler(void) interrupt INT0_VECTOR {}
void Timer0_IRQHandler(void) interrupt TMR0_VECTOR {}
void INT1_IRQHandler(void) interrupt INT1_VECTOR {}
void Timer1_IRQHandler(void) interrupt TMR1_VECTOR {}
void UART0_IRQHandler(void) interrupt UART0_VECTOR {}
void Timer2_IRQHandler(void) interrupt TMR2_VECTOR {}
void P0EI_IRQHandler(void) interrupt P0EI_VECTOR {}
void P1EI_IRQHandler(void) interrupt P1EI_VECTOR {}
void P2EI_IRQHandler(void) interrupt P2EI_VECTOR {}
void P3EI_IRQHandler(void) interrupt P3EI_VECTOR {}
void ACMP_IRQHandler(void) interrupt ACMP_VECTOR {}
void Timer3_IRQHandler(void) interrupt TMR3_VECTOR {}
void Timer4_IRQHandler(void) interrupt TMR4_VECTOR {}
void EPWM_IRQHandler(void) interrupt EPWM_VECTOR {}
void ADC_IRQHandler(void) interrupt ADC_VECTOR {}

void WDT_IRQHandler(void) interrupt WDT_VECTOR 
{
	if(WDT_GetOverflowIntFlag())
	{
		P33 = ~P33;
		WDT_ClearOverflowIntFlag();
	}
}

void I2C_IRQHandler(void) interrupt I2C_VECTOR {}
void SPI_IRQHandler(void) interrupt SPI_VECTOR {}
void LSE_SCM_IRQHandler(void) interrupt LSE_SCM_VECTOR {}
void LVD_IRQHandler(void) interrupt LVD_VECTOR {}
