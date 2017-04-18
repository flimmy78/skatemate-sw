/*
   SKATEMATE - Copyright (C) 2016 FHNW Project 3 Team 2
 */

/**
 * @file       defs.h
 * @brief      Thread and debugging definitions
 * @details 
 * @author     Noah Huetter (noahhuetter@gmail.com)
 * @date       23 March 2017
 * 
 *
 * @addtogroup MAIN
 * @{
 */

#ifndef _DEFS_H
#define _DEFS_H

#include "chprintf.h"
#include "usbcfg.h"


#define DEFS_THD_IDLE_WA_SIZE 			0x500
#define DEFS_THD_SHELL_WA_SIZE 			2048

#define DEFS_THD_IDLE_NAME 			"main"
#define DEFS_THD_SHELL_NAME 			"shell"

#define SYSTEM_CORE_CLOCK	72000000 //Hz

#define DBG(X, ...) chprintf(bssusb, X, ##__VA_ARGS__ )
/*===========================================================================*/
/* Test defines. Always uncomment for production                             */
/*===========================================================================*/


#endif

/** @} */