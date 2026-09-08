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
#define  UART_BUAD_USE_BRT      1
#define  UART_BUAD_USE_TMR1     2
#define  UART_BUAD_USE_TMR4     3
#define  UART_BUAD_USE_TMR2     4

#define  UART_BUAD_USE_SEL      UART_BUAD_USE_BRT

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

#if ( UART_BUAD_USE_BRT == UART_BUAD_USE_SEL)		//使用BRT作为UART模块的波特率时钟发生器
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


#elif ( UART_BUAD_USE_TMR1 == UART_BUAD_USE_SEL)		 //使用TMR1作为UART模块的波特率时钟发生器

	 uint8_t  THxValue = 0;
	 uint32_t  BaudRateVlue = 9600;	 
	/*
	 (1)设置UARTx的运行模式
	 */
	 UART_ConfigRunMode(UART_MOD_ASY_8BIT, UART_BAUD_TMR1);
	 UART_EnableReceive();
	/*
	 (2)配置UARTx的波特率
	 */
	 TMR_ConfigTimerClk(TMR1,TMR_CLK_DIV_4);							/*TM =1*/
	 TMR_ConfigRunMode(TMR1, TMR_MODE_TIMING, TMR_TIM_AUTO_8BIT); 		/*设置为8位重装模式*/
	
	 UART_EnableDoubleFrequency(); 							/*波特率使能倍频：SMOD =1*/
	
  #ifdef USE_FORMULA			//使用公式计算定时器的加载值(需要对Systemclock赋值(main.c))，USE_FORMULA 在 选项Option->C51->Preporcessor Symbols->Define中定义
	 THxValue = UART_ConfigBaudRate( BaudRateVlue) ;
  #else 
	 THxValue = 217; 				//使用手册上推荐的加载值(UART章节),对应的系统时钟：24MHz
  #endif
  
	 TMR_ConfigTimerPeriod(TMR1, THxValue, THxValue);			/*配置重装值*/
	 TMR_Start(TMR1);											/*使能定时器*/
	
	 /*
	 (3)配置IO口
	 */
	 GPIO_SET_MUX_MODE(P14CFG,GPIO_P14_MUX_TXD);			/*TXD*/
	 GPIO_SET_MUX_MODE(P13CFG,GPIO_P13_MUX_RXD);	 		/*RXD*/
	 GPIO_SET_PS_MODE(PS_RXD, GPIO_P13);					/*RXD输入选择P13*/

#elif ( UART_BUAD_USE_TMR4 == UART_BUAD_USE_SEL)	//使用TMR4作为UART模块的波特率时钟发生器 

	 uint8_t  THxValue = 0;
	 uint32_t  BaudRateVlue = 9600;	 
	/*
	 (1)设置UARTx的运行模式
	 */
	 UART_ConfigRunMode(UART_MOD_ASY_8BIT, UART_BAUD_TMR4);
	 UART_EnableReceive();
	/*
	 (2)配置UARTx的波特率
	 */
	 TMR_ConfigTimerClk(TMR4,TMR_CLK_DIV_4);							/*TM =1*/
	 TMR_ConfigRunMode(TMR4, TMR_MODE_TIMING, TMR_TIM_AUTO_8BIT); 		/*设置为8位重装模式*/
	
	 UART_EnableDoubleFrequency(); 							/*波特率使能倍频：SMOD =1*/
	
  #ifdef USE_FORMULA			//使用公式计算定时器的加载值(需要对Systemclock赋值(main.c))，USE_FORMULA 在 选项Option->C51->Preporcessor Symbols->Define中定义
	 THxValue = UART_ConfigBaudRate(BaudRateVlue) ;
  #else 
	 THxValue = 217; 				//使用手册上推荐的加载值(UART章节),对应的系统时钟：24MHz
  #endif
  
	 TMR_ConfigTimerPeriod(TMR4, THxValue, THxValue);			/*配置重装值*/
	 TMR_Start(TMR4);											/*使能定时器*/
	
	 /*
	 (3)配置IO口
	 */
	 GPIO_SET_MUX_MODE(P14CFG,GPIO_P14_MUX_TXD);			/*TXD*/
	 GPIO_SET_MUX_MODE(P13CFG,GPIO_P13_MUX_RXD);	 		/*RXD*/
	 GPIO_SET_PS_MODE(PS_RXD, GPIO_P13);					/*RXD输入选择P13*/


#elif ( UART_BUAD_USE_TMR2 == UART_BUAD_USE_SEL)		//使用TMR2作为UART模块的波特率时钟发生器	 
	 uint16_t  TMR2Value = 0;
	 uint32_t  BaudRateVlue = 9600;	 
	/*
	 (1)设置UARTx的运行模式
	 */
	 UART_ConfigRunMode(UART_MOD_ASY_8BIT, UART_BAUD_TMR2);
	 UART_EnableReceive();
	/*
	 (2)配置UARTx的波特率
	 */
	 TMR2_ConfigTimerClk(TMR2_CLK_DIV_12);						/*T2PS =0*/
	 TMR2_ConfigRunMode(TMR2_MODE_TIMING, TMR2_AUTO_LOAD); 		/*设置为8位重装模式*/
	
	 UART_EnableDoubleFrequency(); 							/*波特率使能倍频：SMOD =1*/
	
  #ifdef USE_FORMULA				//使用公式计算定时器的加载值(需要对Systemclock赋值(main.c))，USE_FORMULA 在 选项Option->C51->Preporcessor Symbols->Define中定义
	 TMR2Value = UART_ConfigBaudRate( BaudRateVlue) ;
														//根据需要的波特率计算出下载值后，请再根据计算出的下载值反推出实际的波特率（公式在参考手册"波特率计算"章节）
														//（如果偏差较大会影响通讯，请换一个精度高的波特率）
  #else 
	 TMR2Value = 65523; 		  //根据参考手册的公式计算出。对应的系统时钟：24MHz			
  #endif
  
	 TMR2_ConfigTimerPeriod(TMR2Value);						/*配置重装值*/
	 TMR2_Start();											/*使能定时器*/
	
	 /*
	 (3)配置IO口
	 */
	 GPIO_SET_MUX_MODE(P14CFG,GPIO_P14_MUX_TXD);			/*TXD*/
	 GPIO_SET_MUX_MODE(P13CFG,GPIO_P13_MUX_RXD);	 		/*RXD*/
	 GPIO_SET_PS_MODE(PS_RXD, GPIO_P13);					/*RXD输入选择P13*/
#endif

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






