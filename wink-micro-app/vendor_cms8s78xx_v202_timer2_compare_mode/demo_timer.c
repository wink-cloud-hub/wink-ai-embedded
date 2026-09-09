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
	(1)设置运行模式
	*/
	TMR2_ConfigRunMode(TMR2_MODE_TIMING, TMR2_LOAD_DISBALE);
	TMR2_EnableCompare(TMR2_CC0,TMR2_CMP_MODE_0);	
	TMR2_EnableCompare(TMR2_CC1,TMR2_CMP_MODE_0);		
	TMR2_EnableCompare(TMR2_CC2,TMR2_CMP_MODE_0);	
	TMR2_EnableCompare(TMR2_CC3,TMR2_CMP_MODE_0);		
	
	/*
	(2)设置时钟
	*/
	TMR2_ConfigTimerClk( TMR2_CLK_DIV_12);						/*Fsys = 24Mhz->Ftimer = 2Mhz,Ttmr=0.5us*/
	/*
	(3)设置周期	
	*/	
	TMR2_ConfigTimerPeriod((65536 - 2000)); 		//1ms
		
	/*
	(4)设置比较值
	*/
	TMR2_ConfigCompareValue(TMR2_CC0,(65536-1000));	
	TMR2_ConfigCompareValue(TMR2_CC1,(65536-1000));	
	TMR2_ConfigCompareValue(TMR2_CC2,(65536-1000));	
	TMR2_ConfigCompareValue(TMR2_CC3,(65536-1000));	

	/*
	(5)设置中断
	*/
	TMR2_EnableOverflowInt();
	TMR2_EnableCompareInt(TMR2_CC0);
	TMR2_EnableCompareInt(TMR2_CC1);	
	TMR2_EnableCompareInt(TMR2_CC2);	
	TMR2_EnableCompareInt(TMR2_CC3);	
	TMR2_ConfigCompareIntMode(TMR2_CMP_INT_MODE1);			
	
	/*
	(6)设置优先级
	*/		
	IRQ_SET_PRIORITY(IRQ_TMR2,IRQ_PRIORITY_LOW);	
	TMR2_AllIntEnable();
	IRQ_ALL_ENABLE();	
	
	/*
	(7)设置IO复用
	*/	
	GPIO_SET_MUX_MODE(P00CFG, GPIO_P00_MUX_CC0);
	GPIO_SET_MUX_MODE(P01CFG, GPIO_P01_MUX_CC1);
	GPIO_SET_MUX_MODE(P15CFG, GPIO_P15_MUX_CC2);			
	GPIO_SET_MUX_MODE(P14CFG, GPIO_P14_MUX_CC3);
	
	/*
	(8开启Tiemr2
	*/
	TMR2_Start();

}



























