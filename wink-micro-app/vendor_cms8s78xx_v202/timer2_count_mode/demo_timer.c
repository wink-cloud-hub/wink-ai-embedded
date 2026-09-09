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
** \brief	 TMR2_Config
** \param [in] 
**            	
** \return  none
** \note  
******************************************************************************/
void TMR2_Config(void)
{
	/*
	(1)设置Timer的运行模式
	*/
	TMR2_ConfigRunMode(TMR2_MODE_COUNT, TMR2_AUTO_LOAD);
	
	GPIO_SET_MUX_MODE(P16CFG,GPIO_MUX_GPIO);
	GPIO_ENABLE_INPUT(P1TRIS, GPIO_PIN_6);
	GPIO_ENABLE_UP(P1UP, GPIO_PIN_6);	
	GPIO_SET_PS_MODE(PS_T2, GPIO_P16_MUX_T2);	
	/*
	(2)设置Timer定时
	*/
	TMR2_ConfigTimerPeriod((65536 - 5));                 			   /* 计数5次*/ 
	/*
	(3)开启中断
	*/
	TMR2_EnableOverflowInt();
	/*
	(4)设置Timer中断优先级
	*/	
	IRQ_SET_PRIORITY(IRQ_TMR2,IRQ_PRIORITY_LOW);
	
	TMR2_AllIntEnable();
	IRQ_ALL_ENABLE();
	/*
	(5)开启Timer
	*/
	TMR2_Start();
}



























