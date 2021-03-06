/*
   SKATEMATE - Copyright (C) 2017 FHNW Project 4 Team 2
 */

/**
 * @file       usbcdc.c
 * @brief      USB CDC methods providing shell access and debugging
 * @details    
 * @author     Noah Huetter (noahhuetter@gmail.com)
 * @date       23 March 2017
 * 
 *
 * @addtogroup USBCDC
 * @brief USB CDC device init functions
 * @{
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "chthreads.h"
#include "shell.h"
#include "defs.h"
#include "util.h"
#include "drv8301.h"
#include "mc_foc.h"

#include "chprintf.h"

#include "usbcfg.h"
#include "usbcdc.h"


/*===========================================================================*/
/* settings                                                                */
/*===========================================================================*/

#define usb_lld_connect_bus(usbp)
#define usb_lld_disconnect_bus(usbp)

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(DEFS_THD_SHELL_WA_SIZE)

#define THD_STATE_NAMES                                                     \
  "READY", "CURRENT", "SUSPENDED", "WTSEM", "WTMTX", "WTCOND", "SLEEPING",  \
  "WTEXIT", "WTOREVT", "WTANDEVT", "SNDMSGQ", "SNDMSG", "WTMSG", "WTQUEUE", \
  "FINAL"

/*===========================================================================*/
/* private data                                                              */
/*===========================================================================*/
static thread_t *shelltp = NULL;
static THD_WORKING_AREA(shellWA, DEFS_THD_SHELL_WA_SIZE);

static const usbcdcParameterStruct_t** mShellVars;

/*===========================================================================*/
/* Module static functions.                                                  */
/*===========================================================================*/
/**
 * @brief      calculates the free stack size of a thread
 * @details    CH_DBG_FILL_THREADS must be true
 *
 * @param      wsp   pointer to the thread
 * @param[in]  size  working area assigned to the thread
 *
 * @return     The thd free stack.
 */
static uint32_t get_thd_free_stack(void *wsp, size_t size)
{
  uint32_t n = 0;
#if CH_DBG_FILL_THREADS
  uint8_t *startp = (uint8_t *)wsp + sizeof(thread_t); // start of working area
  uint8_t *endp = (uint8_t *)wsp + size;
  while (startp < endp)
    if(*startp++ == CH_DBG_STACK_FILL_VALUE) ++n;
#endif
  return n;
}

/**
 * @brief      prints a memory report to the stream
 *
 * @param      chp   pointer to the output stream
 * @param[in]  argc  argument count
 * @param      argv  argument list
 */
static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
	size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreGetStatusX());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

/**
 * @brief      prints a thread report to the output stream
 *
 * @param      chp   pointer to the output stream
 * @param[in]  argc  argument count
 * @param      argv  argument list
 */
static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
	static const char *states[] = {CH_STATE_NAMES};
  thread_t *tp;
  uint32_t time = 0;
  uint32_t idletime = 0;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio     state     time     wa_size     free name\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%08lx %08lx %4lu %9s %8lu %11lu %8lu %s\r\n",
            (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
            (uint32_t)tp->p_prio,
            states[tp->p_state], (uint32_t)chThdGetTicksX(tp), 
            getThdWaSize(tp),
            get_thd_free_stack(tp, getThdWaSize(tp)),
            chRegGetThreadNameX(tp));
    if(strstr(chRegGetThreadNameX(tp), "idle") == NULL) time += (uint32_t)chThdGetTicksX(tp);
    else idletime = (uint32_t)chThdGetTicksX(tp);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
  chprintf(chp, "Overall CPU usge is %.1f%%\r\n", (float)time/(float)idletime*100.0);
  
}
static void cmd_drv(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  drvDumpStatus();
}
static void cmd_drvon(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  drvGateEnable();
}
static void cmd_drvoff(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  drvGateDisable();
}
static void cmd_duty(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  if(argc<3) return;
  mcfSetDuty(atoi(argv[0]),atoi(argv[1]),atoi(argv[2]));
}
static void cmd_setcurr(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  if(argc<1) return;
  mcfSetCurrent(stof(argv[0]));
}
static void cmd_sample(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  mcfStartSample();
}
static void cmd_lock(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  mcfSetMotorLock(1);
}
static void cmd_release(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  mcfSetMotorLock(0);
}
static void cmd_volt(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  mcfDumpData();
}


static void cmd_get(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  uint8_t ctr;
  for(ctr = 0; mShellVars[ctr] != NULL; ctr++)
  {
    chprintf(chp, "%s = %.3f\r\n", mShellVars[ctr]->name, *mShellVars[ctr]->loc);
  }
}
static void cmd_set(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  uint8_t ctr;
  for(ctr = 0; mShellVars[ctr] != NULL; ctr++)
  {
    if(strcmp(mShellVars[ctr]->name, argv[0]) == 0)
    {
      (*mShellVars[ctr]->loc) = stof(argv[1]);
      chprintf(chp, "%s = %.3f\r\n", mShellVars[ctr]->name, *mShellVars[ctr]->loc);
      return;
    }
  }
  chprintf(chp, "%s not found\r\n", argv[0]);
}
/**
 * List of shell commands
 */
static const ShellCommand commands[] = {
		{"mem", cmd_mem},
    {"threads", cmd_threads},
    {"drv", cmd_drv},
    {"drvon", cmd_drvon},
    {"drvoff", cmd_drvoff},
    {"duty", cmd_duty},
    {"get", cmd_get},
    {"set", cmd_set},
    {"lock", cmd_lock},
    {"release", cmd_release},
    {"sample", cmd_sample},
    {"volt", cmd_volt},
    {"setcurr", cmd_setcurr},
		{NULL, NULL}
};

/**
 * shell configuration
 */
static const ShellConfig shell_cfg1 = {
		(BaseSequentialStream *)&DEFS_SHELL_STREAM,
		commands
};

/*===========================================================================*/
/* Module public functions.                                                  */
/*===========================================================================*/
/**
 * @brief      Handle the shell, must be called periodically
 */
void usbcdcHandleShell(void) {
  // if (!shelltp && (DEFS_SHELL_STREAM.config->usbp->state == USB_ACTIVE))
  if (!shelltp)
      // shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
      shelltp = shellCreateStatic(&shell_cfg1, shellWA, sizeof(shellWA), NORMALPRIO);
    else if (chThdTerminatedX(shelltp)) {
      // chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
      shelltp = NULL;           /* Triggers spawning of a new shell.        */
    }
}

/**
 * @brief      Init the usb cdc interface
 */
void usbcdcInit(void) {
  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  // chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);
}

void usbcdcSetShellVars(const usbcdcParameterStruct_t** vars)
{
  mShellVars = vars;
}



/** @} */