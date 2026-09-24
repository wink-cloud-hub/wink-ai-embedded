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
/** \file main.c
**
** 
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "cms8s78xx.h"

/****************************************************************************/
/*	Local pre-processor symbols('#define')
*****************************************************************************/

/****************************************************************************/
/*	Global variable definitions(declared in header file with 'extern')
*****************************************************************************/
uint32_t  Systemclock = 24000000;

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

/*****************************************************************************
 ** \brief	 main
 **
 ** \param [in]  none   
 **
 ** \return 0
 *****************************************************************************/
int main(void)
{	
	
	uint16_t i;

	/*系统时钟架构可参考手册4.1章节"系统时钟结构"*/
	/*(1)通过选项Options->Debug->Seting中设置时钟Fosc时钟
	    Fosc 可选择:
		(1) HSI(48Mhz)(可选分频：1、2、3、6)
		(2) HSE(高速晶振:8Mhz、16Mhz)
		(3) LSE(低速晶振:32.768Khz)	
		(4) LSI(125Khz)	
	*/
	// Fosc = HSI/2 = 24Mhz;
	
	/*(2)通过选项Options->Debug->Seting中设置时钟Fsys_pre时钟
	   Fsys_pre = Fosc/SYS_PRESCALE(系统时钟预分频: 1、2、4、8)*/
	
	// Fsys_pre = Fosc/1 = 24Mhz;
	
	/*(3)通过寄存器CLKDIV配置Fsys(系统时钟): Fsys = Fsys_pre/分频*/
		
	SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1);
	Systemclock = 24000000;	
	
	/*(4)配置CLO（系统时钟64分频输出）检测系统时钟*/
	GPIO_SET_MUX_MODE(P13CFG, GPIO_P13_MUX_CLO);	//输出系统时钟 64分频
	
	
	GPIO_SET_MUX_MODE(P32CFG, GPIO_MUX_GPIO);
	P32 =0;	
	GPIO_ENABLE_OUTPUT(P3TRIS, GPIO_PIN_2);
		
	while(1)
	{		
		for(i=3000;i>0;i--);
		P32 = ~P32;	
	}	
}


	
	



















