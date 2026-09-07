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
/** \file demo_extint.c
**
**  
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_extint.h"

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
 ** \brief	 EXTINT_Config
 ** \param [in] 
 **            	
 ** \return  none
 ** \note  
 ******************************************************************************/
void EXTINT_Config(void)
{
	/*
	(1)设置EXTINT功能
	*/

	EXTINT_ConfigInt(EXTINT0, EXTINT_TRIG_FALLING);		//INT0 下降沿触发中断
	/*
	(2)设置EXTINT IO口
	*/	
	GPIO_SET_MUX_MODE(P30CFG, GPIO_MUX_GPIO);			//设置P30为GPIO模式
	GPIO_ENABLE_INPUT(P3TRIS, GPIO_PIN_0);				//设置为输入模式	
	GPIO_ENABLE_UP(P3UP, GPIO_PIN_0);					//开启P30上拉
	GPIO_SET_PS_MODE(PS_INT0, GPIO_P30_MUX_INT0);		//复用为INT0 输入功能
	/*
	(3)设置EXTINT中断
	*/		
	EXTINT_EnableInt(EXTINT0);
	IRQ_SET_PRIORITY(IRQ_INT0, IRQ_PRIORITY_HIGH);
	IRQ_ALL_ENABLE();							 //开启总中断
	
}
























