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

/*****************************************************************************/
/** \file demo_spi.h
**
**
**
** History:
** - 
*****************************************************************************/
#ifndef __DEMO_SPI_H__
#define __DEMO_SPI_H__
/*****************************************************************************/
/* Include files */
/*****************************************************************************/
#include "cms8s78xx.h"

/*****************************************************************************/
/* Global pre-processor symbols/macros ('#define') */
/*****************************************************************************/
/*----------------------------------------------------------------------------
 **EEPROM  M95256 
 **	
 ---------------------------------------------------------------------------*/
#define	M95256_WREN			0x06
#define M95256_WRDI			0x04
#define	M95256_RDSR			0x05
#define	M95256_WRSR			0x01
#define	M95256_READ			0x03
#define	M95256_WRITE		0x02
#define	M95256_RDID			0x83
#define	M95256_WRID			0x82
#define	M95256_RDLS			0x83
#define	M95256_LID			0x82

/*****************************************************************************/
/* Global type definitions ('typedef') */
/*****************************************************************************/



/*****************************************************************************/
/* Global variable declarations ('extern', definition in C source) */
/*****************************************************************************/


/*****************************************************************************/
/* Global function prototypes ('extern', definition in C source) */
/*****************************************************************************/
/******************************************************************************
 ** \brief	 SPI_Config
 ** \param [in] 
 **            	
 ** \return  none
 ** \note  
 ******************************************************************************/
void SPI_Config(void);

/***************************************************************************
 ** \brief	 SPI_M95256_Write
 **			 
 ** \param [in]  addr: 
 **				 buf:
 ** \return 
 ** \note
***************************************************************************/
void SPI_M95256_Write(uint32_t addr, uint8_t buf);
/***************************************************************************
 ** \brief	 SPI_M95256_Read_Data
 **			
** \param [in]  addr :   
 ** \return 8bit Data
 ** \note
***************************************************************************/
uint8_t SPI_M95256_Read_Data(uint32_t addr);

/***************************************************************************
 ** \brief	 SPI_M95256_Read_SFR
 **			
 ** \param [in]  cmd:	
 ** \return 8bit Data
 ** \note
***************************************************************************/
uint8_t  SPI_M95256_Read_SFR(uint8_t cmd);


#endif /* __DEMO_SPI_H__ */






