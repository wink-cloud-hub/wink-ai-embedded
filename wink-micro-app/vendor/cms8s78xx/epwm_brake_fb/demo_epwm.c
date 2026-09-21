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
/** \file demo_epwm.c
**
** 
**
**	History:
**	
*****************************************************************************/
/****************************************************************************/
/*	include files
*****************************************************************************/
#include "demo_epwm.h"

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
 ** \brief	 EPWM_Config (complementary, no dead time)
 **
 ** \param [in]  none   
 **
 ** \return none
 ******************************************************************************/
void EPWM_Config(void)
{
	EPWM_ConfigRunMode(EPWM_WFG_COMPLEMENTARY|EPWM_OC_INDEPENDENT|EPWM_OCU_SYMMETRIC|EPWM_COUNT_DOWN);
	EPWM_ConfigChannelClk(EPWM0, EPWM_CLK_DIV_1);		
	EPWM_ConfigChannelClk(EPWM2, EPWM_CLK_DIV_1);

	EPWM_ConfigChannelPeriod(EPWM0, 0x12C0);
	EPWM_ConfigChannelPeriod(EPWM2, 0x12C0);

	EPWM_ConfigChannelSymDuty(EPWM0, 0x0960);
	EPWM_ConfigChannelSymDuty(EPWM2, 0x0960);

	EPWM_EnableAutoLoadMode(EPWM_CH_2_MSK|EPWM_CH_0_MSK);
	EPWM_DisableReverseOutput(EPWM_CH_0_MSK|EPWM_CH_1_MSK|EPWM_CH_2_MSK|EPWM_CH_3_MSK);
	EPWM_EnableOutput(EPWM_CH_0_MSK|EPWM_CH_1_MSK|EPWM_CH_2_MSK|EPWM_CH_3_MSK);		
	EPWM_DisableDeadZone(EPWM0);
	EPWM_DisableDeadZone(EPWM2);

	EPWM_ConfigBrakeMode(EPWM_BRK_RECOVER,EPWM_BRK_LOAD_EPWM0);	

	EPWM_EnableFBBrake(EPWM_BRK_FB0,EPWM_BRK_FB_HIGH);
	GPIO_SET_MUX_MODE(P06CFG,GPIO_P06_MUX_FB0);
	GPIO_SET_PS_MODE(PS_FB0,GPIO_P06);

	EPWM_ConfigChannelBrakeLevel(EPWM_CH_0_MSK|EPWM_CH_1_MSK,1);
	EPWM_ConfigChannelBrakeLevel(EPWM_CH_2_MSK|EPWM_CH_3_MSK,0);	
	EPWM_EnableFaultBrake();

	EPWM_EnableZeroInt(EPWM_CH_0_MSK);				
	EPWM_EnableFaultBrakeInt();						
	EPWM_AllIntEnable();							
	IRQ_SET_PRIORITY(IRQ_EPWM,IRQ_PRIORITY_HIGH);
	IRQ_ALL_ENABLE();								

	GPIO_SET_MUX_MODE(P20CFG, GPIO_P20_MUX_PG0);
	GPIO_SET_MUX_MODE(P21CFG, GPIO_P21_MUX_PG1);
	GPIO_SET_MUX_MODE(P22CFG, GPIO_P22_MUX_PG2);
	GPIO_SET_MUX_MODE(P23CFG, GPIO_P23_MUX_PG3);

	EPWM_Start(EPWM_CH_2_MSK|EPWM_CH_0_MSK);
}
