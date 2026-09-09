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
	TMR_ConfigRunMode(TMR1, TMR_MODE_COUNT,TMR_TIM_16BIT);			//监测T1功能脚的下降沿
		
	GPIO_SET_MUX_MODE(P24CFG,GPIO_MUX_GPIO);			
	GPIO_ENABLE_INPUT(P2TRIS, GPIO_PIN_4);	
	GPIO_ENABLE_UP(P2UP, GPIO_PIN_4);
	GPIO_SET_PS_MODE(PS_T1, GPIO_P24_MUX_T1);			
	/*
	(2)设置Timer周期
	*/	
	TMR_ConfigTimerPeriod(TMR1, (65536-5)>>8 , (65536-5)); 				// 计数5次溢出
		
	/*
	(3)开启中断
	*/
	TMR_EnableOverflowInt(TMR1);

	/*
	(4)设置Timer中断优先级
	*/	
	IRQ_SET_PRIORITY(IRQ_TMR1,IRQ_PRIORITY_LOW);

	IRQ_ALL_ENABLE();	

	/*
	(5)开启Timer
	*/
	TMR_Start(TMR1);
}





























