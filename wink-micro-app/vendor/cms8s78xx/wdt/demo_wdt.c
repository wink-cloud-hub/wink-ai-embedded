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
/** \file demo_wdt.c
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_wdt.h"

/****************************************************************************/
/*	Local pre-processor symbols('#define')
*****************************************************************************/

/****************************************************************************/
/*	Global variable definitions(declared in header file with 'extern')
*****************************************************************************/

/****************************************************************************/
/*	Local type definitions('typedef')
*****************************************************************************/

/****************************************************************************/
/*	Local variable  definitions('static')
*****************************************************************************/

/****************************************************************************/
/*	Local function prototypes('static')
*****************************************************************************/

/****************************************************************************/
/*	Function implementation - global ('extern') and local('static')
*****************************************************************************/
/******************************************************************************
 ** \brief	 WDT_Config
 **
 ** \param [in]  none   
 **            	
 ** \return none
 ******************************************************************************/
void WDT_Config(void)
{
	/*
	(1) 清看门狗
	*/
	WDT_ClearWDT();
	/*
	(2) 开启WDT溢出时间
	*/
	WDT_ConfigOverflowTime(WDT_CLK_4194304);	// Fsys=24Mhz -> Twdt = 4194304/24 = 174.76ms
	/*
	(3) 设置WDT溢出中断
	*/
	WDT_EnableOverflowInt();

	IRQ_SET_PRIORITY(IRQ_WDT, IRQ_PRIORITY_HIGH);
	IRQ_ALL_ENABLE();
}
