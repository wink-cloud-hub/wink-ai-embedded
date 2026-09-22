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
#include "demo_i2c.h"

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

/*****************************************************************************
 ** \brief	 main
 **
 ** \param [in]  none   
 **
 ** \return 0
 *****************************************************************************/

volatile uint8_t  temp =0;
int main(void)
{		
	
	uint8_t array[10];							/*数据缓存区*/
	uint8_t value;								
	uint8_t datasize;			
	
	SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1);
		
	I2C_Config();							/*设置I2C主控模式*/		

	/*可在Debug模式中使用，发码格式与AT24C256的I2C协议一致，具体可参考AT24C256数据手册*/	
	At24c256_write_byte(0x10, 0x31);			/*写单个数据*/
	value = At24c256_read_byte(0x10);			/*读单个数据*/
	
	for(datasize =0; datasize<5; datasize++ )
	{
		At24c256_write_byte((0x11 + datasize), (0x32 + datasize));
	}
	datasize = 5;
	At24c256_read_str(0x11, array, datasize);	/*连续读数据*/
	
	while(1)
	{	
		;
	}		
}





















