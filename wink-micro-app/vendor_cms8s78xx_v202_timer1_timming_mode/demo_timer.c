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

/****************************************************************************/
/** \file demo_timer.c
**
**  
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_timer.h"

/****************************************************************************/
/*	Local pre-processor symbols('#define')
****************************************************************************/

/****************************************************************************/
/*	Global variable definitions(declared in header file with 'extern')
****************************************************************************/

/****************************************************************************/
/*	Local type definitions('typedef')
****************************************************************************/

/****************************************************************************/
/*	Local variable  definitions('static')
****************************************************************************/

/****************************************************************************/
/*	Local function prototypes('static')
****************************************************************************/

/****************************************************************************/
/*	Function implementation - global ('extern') and local('static')
****************************************************************************/
/******************************************************************************
** \brief	 TMR1_Config
** \param [in] 
**            	
** \return  none
** \note  
******************************************************************************/
void TMR1_Config(void)
{
	/*
	(1)设置Timer的运行模式
	*/
	TMR_ConfigRunMode(TMR1, TMR_MODE_TIMING,TMR_TIM_16BIT);	
	/*
	(2)设置Timer 运行时钟
	*/
	TMR_ConfigTimerClk(TMR1, TMR_CLK_DIV_12);						/*Fsys = 24Mhz，Ftimer = 2Mhz,Ttmr=0.5us*/
	/*
	(3)设置Timer周期
	*/	
	TMR_ConfigTimerPeriod(TMR1, (65536-200)>>8, 65536-200); 				// 200*0.5us = 100us,递增计数
		
	/*
	(4)开启中断
	*/
	TMR_EnableOverflowInt(TMR1);

	/*
	(5)设置Timer中断优先级
	*/	
	IRQ_SET_PRIORITY(IRQ_TMR1,IRQ_PRIORITY_LOW);
	IRQ_ALL_ENABLE();	

	/*
	(6)开启Timer
	*/
	TMR_Start(TMR1);
}




























