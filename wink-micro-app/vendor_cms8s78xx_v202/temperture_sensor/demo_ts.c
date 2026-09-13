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
/** \file demo_ts.c
**
**  
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_ts.h"

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
 ** \brief	TS_ADC_Config
 **			
 ** \param [in]  none   
 **
 ** \return  none
 ** \note	
 *****************************************************************************/
void TS_ADC_Config(void)
{
	
	ADC_ConfigRunMode(ADC_CLK_DIV_256, ADC_RESULT_LEFT);
	ADC_EnableChannel(ADC_CH_63);	
	ADC_ConfigAN63(ADC_CH_63_TS);
	
	ADC_EnableLDO();
	ADC_ConfigADCVref(ADC_VREF_3V);		
	
	ADC_Start();
}

/*****************************************************************************
 ** \brief	TS_Adjust
 **			
 ** \param [in]  none   
 **
 ** \return  none
 ** \note	
 *****************************************************************************/
void TS_Adjust(void)
{
	volatile uint16_t Count0,Count1,Count3;
	uint16_t TS_ADCValue;	
	uint16_t adjvalue;
		
	TS_REG = 0x00;
	TS_REG |= (1<<6)|(1<<7);
	
	TS_ADC_Config();
	
	for(Count0 =0; Count0<16; Count0++)
	{
		TS_REG = 0xC0;
		TS_REG |= Count0;
		TS_ADCValue = 0;
		
		for(Count1=0; Count1<16; Count1++)
		{	
			for(Count3=0;Count3<500;Count3++);		
			ADC_GO();
			while (ADC_IS_BUSY);
			TS_ADCValue += ADC_GetADCResult();										
		}
		TS_ADCValue = TS_ADCValue >> 4;		
		
		if((TS_ADCValue> 1365-7) && (TS_ADCValue < 1365+7))
		{
				adjvalue  = Count0;
				break;
		}		
	}	
	
	TS_REG = (1<<6)|(1<<7)|(adjvalue);	
}

/*****************************************************************************
 ** \brief	TS_GetTemperature
 **			
 ** \param [in]  none   
 **
 ** \return  
 ** \note	
 *****************************************************************************/
float  TS_GetTemperature(void)
{
	float TemperatureValue;
	volatile uint16_t Count1,Count3;
	uint16_t TS_ADCValue;	
			
	TS_ADCValue = 0;		
	for(Count1=0; Count1<16; Count1++)
	{	
		for(Count3=0;Count3<500;Count3++);
		ADC_GO();
		while (ADC_IS_BUSY);
		TS_ADCValue += ADC_GetADCResult();										
	}
	TS_ADCValue = TS_ADCValue >> 4;		
		
	TemperatureValue = ( ((float)(3 * TS_ADCValue))/4096 - 0.909)/0.0035;
	return TemperatureValue;
}
