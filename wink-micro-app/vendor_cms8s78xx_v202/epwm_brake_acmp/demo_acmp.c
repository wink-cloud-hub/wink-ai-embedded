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
/** \file demo_acmp.c
**
** 
**
** History:
** 
*****************************************************************************/
/*****************************************************************************/
/* Include files */
/*****************************************************************************/
#include "demo_acmp.h"

/*****************************************************************************/
/* Local pre-processor symbols/macros ('#define') */
/*****************************************************************************/

/*****************************************************************************/
/* Global variable definitions (declared in header file with 'extern') */
/*****************************************************************************/

/*****************************************************************************/
/* Local type definitions ('typedef') */
/*****************************************************************************/

/*****************************************************************************/
/* Local variable definitions ('static') */
/*****************************************************************************/

/*****************************************************************************/
/* Local function prototypes ('static') */
/*****************************************************************************/

/*****************************************************************************/
/* Function implementation - global ('extern') and local ('static') */
/*****************************************************************************/

/******************************************************************************
 ** \brief	 ACMP0_Config
 **
 ** \param [in]  none   
 **
 ** \return none
 ******************************************************************************/
void ACMP0_Config(void)
{
#define NULL  0

	ACMP_ConfigPositive(ACMP0, ACMP_POSSEL_P0);

	ACMP_ConfigNegative(ACMP0, ACMP_NEGSEL_BG,NULL);	

	ACMP_DisableReverseOutput(ACMP0);		

	ACMP_EnableFilter(ACMP0,ACMP_NGCLK_65_TSYS);	

	ACMP_DisableHYS(ACMP0);	

	GPIO_SET_MUX_MODE(P11CFG, GPIO_P11_MUX_C0P0);			
	GPIO_SET_MUX_MODE(P10CFG, GPIO_P10_MUX_C0O);			

	ACMP_Start(ACMP0);	
}

/******************************************************************************
 ** \brief	 ACMP1_Config
 **
 ** \param [in]  none   
 **
 ** \return none
 ******************************************************************************/
void ACMP1_Config(void)
{
#define NULL  0

	ACMP_ConfigPositive(ACMP1, ACMP_POSSEL_P2);

	ACMP_ConfigNegative(ACMP1, ACMP_NEGSEL_BG,NULL);	

	ACMP_DisableReverseOutput(ACMP1);		

	ACMP_EnableFilter(ACMP1,ACMP_NGCLK_65_TSYS);	

	ACMP_EnableHYS(ACMP1,ACMP_HYS_SEL_BOUTH,ACMP_HYS_10);	

	GPIO_SET_MUX_MODE(P00CFG, GPIO_P00_MUX_C1P2);		
	GPIO_SET_MUX_MODE(P24CFG, GPIO_P24_MUX_C1O);			

	ACMP_Start(ACMP1);	
}
