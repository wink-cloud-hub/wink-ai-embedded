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
/** \file demo_uart.c
**
**  
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_uart.h"

/****************************************************************************/
/*	Local pre-processor symbols('#define')
****************************************************************************/

/****************************************************************************/
/*	Global variable definitions(declared in header file with 'extern')
****************************************************************************/

uint32_t Systemclock = 24000000;

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
 ** \brief	 UART_Config
 ** \param [in] 
 **            	
 ** \return  none
 ** \note  
 ******************************************************************************/
void UART_Config(void)
{
	 uint16_t  BRTValue = 0;
	 uint32_t  BaudRateVlue = 9600;
	 
	 /*
	 (1)设置UARTx的运行模式
	 */
	 UART_ConfigRunMode(UART_MOD_ASY_8BIT, UART_BAUD_BRT);
	 UART_EnableReceive();
	 /*
	 (2)配置UARTx的波特率
	 */
	 UART_ConfigBRTClk(BRT_CLK_DIV_1);	
	
	 UART_EnableDoubleFrequency(); 							/*波特率使能倍频：SMOD =1*/
	
  #ifdef USE_FORMULA			//使用公式计算定时器的加载值(需要对Systemclock赋值(main.c))，USE_FORMULA 在 选项Option->C51->Preporcessor Symbols->Define中定义
	 BRTValue = UART_ConfigBaudRate( BaudRateVlue) ;
  #else 
	 BRTValue = 65380; 				//使用手册上推荐的加载值(BRT章节),对应的系统时钟：24MHz
  #endif
 
	 UART_ConfigBRTPeriod(BRTValue);							/*配置重装值*/
	 UART_EnableBRT();											/*使能定时器*/
	 /*
	 (3)配置IO口
	 */ 
	 GPIO_SET_MUX_MODE(P14CFG,GPIO_P14_MUX_TXD);			/*TXD*/
	 GPIO_SET_MUX_MODE(P13CFG,GPIO_P13_MUX_RXD);	 		/*RXD*/
	 GPIO_SET_PS_MODE(PS_RXD,GPIO_P13);						/*RXD输入选择P13*/
	 
	 /*
	 (4)设置UART中断
	 */
	 UART_EnableInt();
	 IRQ_SET_PRIORITY(IRQ_UART0,IRQ_PRIORITY_LOW);	 
	 
	 
}



/******************************************************************************
 ** \brief	 putchar
 ** \param [in] data
 **            	
 ** \return  none
 ** \note   <stdio.h>中需要的函数
 ******************************************************************************/
char putchar (char ch)
{
	SBUF0 = ch;
	while( !(SCON0 & (1<<1)));
	SCON0 &=~(1<<1);		
	return 0;
}

/******************************************************************************
 ** \brief	 putchar
 ** \param [in] none
 **            	
 ** \return  data
 ** \note   <stdio.h>中需要的函数
 ******************************************************************************/
char getchar (void)
{
	while(!(SCON0 & (1<<0)));
	SCON0 &=~(1<<0);
	return  SBUF0;	
}
/********************************************************************************
 ** \brief	 puts 
 **
 ** \param [in]  bytes addr for sending
 **
 ** \return  <stdio.h>中需要的函数
 ******************************************************************************/
int  puts( const char  * s)
{
	while(*(s ++))
		putchar(*s);
	return 0;
}






