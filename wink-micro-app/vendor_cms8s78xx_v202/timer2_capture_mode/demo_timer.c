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
	(1)设置Timer捕获
	*/	
	GPIO_SET_MUX_MODE(P00CFG, GPIO_MUX_GPIO);
	GPIO_ENABLE_INPUT(P0TRIS, GPIO_PIN_0);
	GPIO_SET_PS_MODE(PS_CAP0, GPIO_P00_MUX_CAP0);				/*捕获通道0输入*/
	GPIO_ENABLE_RD(P0RD, GPIO_PIN_0);							/*开下拉*/	
	
	GPIO_SET_MUX_MODE(P01CFG, GPIO_MUX_GPIO);
	GPIO_ENABLE_INPUT(P0TRIS, GPIO_PIN_1);
	GPIO_SET_PS_MODE(PS_CAP1, GPIO_P01_MUX_CAP1);				/*捕获通道1输入*/
	GPIO_ENABLE_RD(P0RD, GPIO_PIN_1);							/*开下拉*/	
	
	GPIO_SET_MUX_MODE(P15CFG, GPIO_MUX_GPIO);
	GPIO_ENABLE_INPUT(P1TRIS, GPIO_PIN_5);
	GPIO_SET_PS_MODE(PS_CAP2, GPIO_P15_MUX_CAP2);				/*捕获通道2输入*/
	GPIO_ENABLE_RD(P1RD, GPIO_PIN_5);							/*开下拉*/		
	
	GPIO_SET_MUX_MODE(P14CFG, GPIO_MUX_GPIO);
	GPIO_ENABLE_INPUT(P1TRIS, GPIO_PIN_4);
	GPIO_SET_PS_MODE(PS_CAP3, GPIO_P14_MUX_CAP3);				/*捕获通道3输入*/
	GPIO_ENABLE_RD(P1RD, GPIO_PIN_4);							/*开下拉*/	
		
	/*
	(2)设置Timer 运行时钟
	*/
	TMR2_ConfigTimerClk( TMR2_CLK_DIV_12);						/*Fsys = 24Mhz，Ftimer = 2Mhz,Ttmr=0.5us*/
	/*
	(3)设置Timer周期
	*/	
	TMR2_ConfigTimerPeriod((65536 - 20000)); 			//10ms
			
	/*
	(4)设置Timer的运行模式
	*/
	
	TMR2_ConfigRunMode(TMR2_MODE_TIMING, TMR2_LOAD_DISBALE);
	TMR2_EnableCapture(TMR2_CC0, TMR2_CAP_EDGE_RISING);
	TMR2_EnableCapture(TMR2_CC1, TMR2_CAP_EDGE_RISING);	
	TMR2_EnableCapture(TMR2_CC2, TMR2_CAP_EDGE_RISING);	
	TMR2_EnableCapture(TMR2_CC3, TMR2_CAP_EDGE_RISING);	
	
	/*
	(5)开启中断
	*/
	TMR2_EnableOverflowInt();
	TMR2_EnableCaptureInt(TMR2_CC0);
	TMR2_EnableCaptureInt(TMR2_CC1);	
	TMR2_EnableCaptureInt(TMR2_CC2);	
	TMR2_EnableCaptureInt(TMR2_CC3);	
	
	/*
	(6)设置Timer中断优先级
	*/	
	IRQ_SET_PRIORITY(IRQ_TMR2,IRQ_PRIORITY_LOW);
	
	TMR2_AllIntEnable();
	IRQ_ALL_ENABLE();	
	/*
	(7)开启Timer
	*/
	TMR2_Start();

}




























