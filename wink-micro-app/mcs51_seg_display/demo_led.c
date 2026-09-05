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
/** \file demo_led.c
**
**  
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_led.h"

/****************************************************************************/
/*	Local pre-processor symbols('#define')
****************************************************************************/

/****************************************************************************/
/*	Global variable definitions(declared in header file with 'extern')
****************************************************************************/
/*Bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0*/
/* dp   g    f     e    d   c    b    a  共阴*/
/* 7 	6	 5	   4    3   2    1    0   SEG*/ 

const uint8_t FontTable[12] ={
/*0->0 0 1 1 1 1 1 1 */0x3F,
/*1->0 0 0 0 0 1 1 0 */0x06,
/*2->0 1 0 1 1 0 1 1 */0x5B,	
/*3->0 1 0 0 1 1 1 1 */0x4F,	
/*4->0 1 1 0 0 1 1 0 */0x66,	
/*5->0 1 1 0 1 1 0 1 */0x6D,		
/*6->0 1 1 1 1 1 0 1 */0x7D,		
/*7->0 0 0 0 0 1 1 1 */0x07,		
/*8->0 0 0 0 0 0 0 0 */0x7F,	
/*9->0 1 1 0 1 1 1 1 */0x6F,
/*全亮->1 1 1 1 1 1 1 1 */0xFF,	
/*全灭->0 0 0 0 0 0 0 0 */0x00,	
};



const uint8_t LED_Fount[4][2] = {{0xe, 0x06},{0xD, 0x5B},{0xb,0x4F},{0x7,0x66}};



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
 ** \brief	 LED_Config
 ** \param [in] 
 **            	
 ** \return  none
 ** \note  
 ******************************************************************************/
void LED_Config(void)
{
	/*
	(1)设置LED COM口
	*/
	GPIO_SET_MUX_MODE(P30CFG, GPIO_MUX_GPIO);	//COM0	
	GPIO_SET_MUX_MODE(P31CFG, GPIO_MUX_GPIO);	//COM1		
	GPIO_SET_MUX_MODE(P32CFG, GPIO_MUX_GPIO);	//COM2		
	GPIO_SET_MUX_MODE(P33CFG, GPIO_MUX_GPIO);	//COM3	

	P3 &=~(0xF);						//输出低电平	
	P3TRIS |= 0xF;				//设置COM0~3 输出模式
	P3DR |= 0xF;				//设置COM0~3 输入电流 150mA 
	
	/*
	(2)设置SEG口
	*/
	GPIO_SET_MUX_MODE(P10CFG, GPIO_MUX_GPIO);	//SEG0	-> 数码管：a
	GPIO_SET_MUX_MODE(P11CFG, GPIO_MUX_GPIO);	//SEG1	-> 数码管：b		
	GPIO_SET_MUX_MODE(P12CFG, GPIO_MUX_GPIO);	//SEG2	-> 数码管：c
	GPIO_SET_MUX_MODE(P13CFG, GPIO_MUX_GPIO);	//SEG3	-> 数码管：d	
	GPIO_SET_MUX_MODE(P14CFG, GPIO_MUX_GPIO);	//SEG4	-> 数码管：e
	GPIO_SET_MUX_MODE(P15CFG, GPIO_MUX_GPIO);	//SEG5	-> 数码管：f	
	GPIO_SET_MUX_MODE(P16CFG, GPIO_MUX_GPIO);	//SEG6	-> 数码管：g
	GPIO_SET_MUX_MODE(P17CFG, GPIO_MUX_GPIO);	//SEG7	-> 数码管：dp			
	
	P1 = 0x00;		//输出低电平
	P1TRIS = 0xFF;	//设置为输出模式
	
	LEDSDRP1L = 0x2;		//设置P10~P13输出电流 32.7mA
	LEDSDRP1H = 0x2;		//设置P14~P17输出电流 32.7mA	
}
