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
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "cms8s78xx.h"
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
 ** \brief	 main
 **
 ** \param [in]  none
 **
 ** \return 0
 ******************************************************************************/
int main(void)
{		
	uint8_t i,j;
	
	SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1);
	
	GPIO_SET_MUX_MODE(P32CFG, GPIO_MUX_GPIO);
	P32 =0;	
	GPIO_ENABLE_OUTPUT(P3TRIS, GPIO_PIN_2);
		
	GPIO_SET_MUX_MODE(P33CFG, GPIO_MUX_GPIO);
	P33 =0;	
	GPIO_ENABLE_OUTPUT(P3TRIS, GPIO_PIN_3);
									
	//若Config中WDT配置为ENABLE，WDT将被强制打开，无法关闭。
	
	WDT_Config();
	
	//SYS_EnableWDTReset();	//开启WDT复位系统功能，Config中WDT为SOFTWARECONTROL(软件控制)
	SYS_DisableWDTReset();
	
	while(1)
	{	
		for(i=250;i>0;i--)					
		{
			P33 = ~P33;
			for(j=250;j>0;j--);		
		}
		//WDT_ClearWDT();
	}		
}
