//============================================================================
// Product: Board Support Package example
// Last Updated for Version: 7.3.0
// Date of the Last Update:  2023-08-12
//
//                   Q u a n t u m  L e a P s
//                   ------------------------
//                   Modern Embedded Software
//
// Copyright (C) 2005 Quantum Leaps, LLC. All rights reserved.
//
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-QL-commercial
//
// This software is dual-licensed under the terms of the open source GNU
// General Public License version 3 (or any later version), or alternatively,
// under the terms of one of the closed source Quantum Leaps commercial
// licenses.
//
// The terms of the open source GNU General Public License version 3
// can be found at: <www.gnu.org/licenses/gpl-3.0>
//
// The terms of the closed source Quantum Leaps commercial licenses
// can be found at: <www.state-machine.com/licensing>
//
// Redistributions in source code must retain this top-level comment block.
// Plagiarizing this software to sidestep the license obligations is illegal.
//
// Contact information:
// <www.state-machine.com/licensing>
// <info@state-machine.com>
//============================================================================
#ifndef BSP_H_
#define BSP_H_

#include "serial_interface.h"
#include "stdint.h"
#include <stdbool.h>

#define BSP_TICKS_PER_SEC 100U

void BSP_Init(void);
void BSP_displayPaused(uint8_t paused);
void BSP_displayPhilStat(uint8_t n, char const *stat);
void BSP_terminate(int16_t result);

/**
 ***************************************************************************************************
 * @brief   Functions for blinky LED
 **************************************************************************************************/
void BSP_ledOn(void);
void BSP_ledOff(void);

/**
 ***************************************************************************************************
 * @brief   Retrieve Serial IO interface for the UART Interface serial comms channels
 **************************************************************************************************/
const Serial_IO_T *BSP_Get_Serial_IO_Interface_UART();

/**
 ***************************************************************************************************
 * @brief   Perform a reset of the microcontroller
 **************************************************************************************************/
__attribute__((noreturn)) void BSP_SystemReset(void);

/**
 ***************************************************************************************************
 * @brief   Read the microcontroller's RCC CSR Register.
 **************************************************************************************************/
uint32_t BSP_RCC_CSR_Read(void);

/**
 ***************************************************************************************************
 * @brief   Clear reset flags indicated by the RCC CSR Register.
 **************************************************************************************************/
void BSP_RCC_CSR_ClearResetFlags(void);

/**
 ***************************************************************************************************
 * @brief   Read the microcontroller's assigned backup RAM, a single uint32 value, with index.
 *          Backup RAM is some portion of RAM that survives typical processor reset conditions,
 *          for example, a software driven reset of the microcontroller.
 **************************************************************************************************/
bool BSP_Backup_RAM_Read(int index, uint32_t *output);

/**
 ***************************************************************************************************
 * @brief   Write a value to the microcontroller's assigned backup RAM.
 *          See BSP_Backup_RAM_Read(...) for further background.
 **************************************************************************************************/
void BSP_Backup_RAM_Write(int index, uint32_t value);

#endif // BSP_H_
