/*
 * Copyright (c) 2014-2016 Jim Tremblay
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define NOS_PRIVATE
#include "nOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NOS_CONFIG_ISR_STACK_SIZE
 static nOS_Stack _isrStack[NOS_CONFIG_ISR_STACK_SIZE];
#endif

void nOS_InitContext(nOS_Thread *thread, nOS_Stack *stack, size_t ssize, nOS_ThreadEntry entry, void *arg)
{
    /* Stack grow from high to low address */
    nOS_Stack   *tos    = stack + (ssize - 1);
    uint16_t    *tos16;
#if (NOS_CONFIG_DEBUG > 0)
    size_t i;

    for (i = 0; i < ssize; i++) {
        stack[i] = (nOS_Stack)0xFFFFFFFF;
    }
#endif

     tos16   = (uint16_t*)tos;
#if (__MSP430X_LARGE__ > 0)
    *tos16-- = (uint16_t)((nOS_Stack)entry >> 16);  /* PC MSB */
#endif
    *tos16-- = (uint16_t)((nOS_Stack)entry);        /* PC LSB */
    *tos16-- = 0x0008;                              /* SR */
#if (__MSP430X_LARGE__ > 0)
     tos16  -= 1;
#endif
     tos     = (nOS_Stack*)tos16;

#if (NOS_CONFIG_DEBUG > 0)
    *tos-- = (nOS_Stack)0x15151515;                 /* R15 */
    *tos-- = (nOS_Stack)0x14141414;                 /* R14 */
    *tos-- = (nOS_Stack)0x13131313;                 /* R13 */
#else
     tos  -= 3;                                     /* R15 to R13 */
#endif

    *tos-- = (nOS_Stack)arg;                        /* R12 */
#if (NOS_CONFIG_DEBUG > 0)
    *tos-- = (nOS_Stack)0x11111111;                 /* R11 */
    *tos-- = (nOS_Stack)0x10101010;                 /* R10 */
    *tos-- = (nOS_Stack)0x09090909;                 /* R9 */
    *tos-- = (nOS_Stack)0x08080808;                 /* R8 */
    *tos-- = (nOS_Stack)0x07070707;                 /* R7 */
    *tos-- = (nOS_Stack)0x06060606;                 /* R6 */
    *tos-- = (nOS_Stack)0x05050505;                 /* R5 */
    *tos   = (nOS_Stack)0x04040404;                 /* R4 */
#else
     tos  -= 7;                                     /* R11 to R4 */
#endif

    thread->stackPtr = tos;
}

void nOS_SwitchContext(void)
{
    asm volatile (
        /* Simulate an interrupt by pushing SR */
         PUSH_SR"                                   \n"
        "                                           \n"
        /* Push all registers to running thread stack */
         PUSH_CONTEXT"                              \n"
        "                                           \n"
        /* Save stack pointer to running thread structure */
         MOV_X"     &nOS_runningThread,     r12     \n"
         MOV_X"     sp,                     0(r12)  \n"
        "                                           \n"
        /* Copy nOS_highPrioThread to nOS_runningThread */
         MOV_X"     &nOS_highPrioThread,    r12     \n"
         MOV_X"     #nOS_runningThread,     r11     \n"
         MOV_X"     r12,                    0(r11)  \n"
        "                                           \n"
        /* Restore stack pointer from high prio thread structure */
         MOV_X"     @r12,                   sp      \n"
        "                                           \n"
        /* Pop all registers from high prio thread stack */
         POP_CONTEXT
        "                                           \n"
         POP_SR"                                    \n"
         RET_X"                                     \n"
    );
}

nOS_Stack* nOS_EnterIsr (nOS_Stack *sp)
{
#if (NOS_CONFIG_SAFE > 0)
    if (nOS_running)
#endif
    {
        if (nOS_isrNestingCounter == 0) {
            nOS_runningThread->stackPtr = sp;
#ifdef NOS_CONFIG_ISR_STACK_SIZE
            sp = &_isrStack[NOS_CONFIG_ISR_STACK_SIZE-1];
#else
            sp = nOS_idleHandle.stackPtr;
#endif
        }
        nOS_isrNestingCounter++;
    }

    return sp;
}

nOS_Stack* nOS_LeaveIsr (nOS_Stack *sp)
{
#if (NOS_CONFIG_SAFE > 0)
    if (nOS_running)
#endif
    {
        nOS_isrNestingCounter--;
        if (nOS_isrNestingCounter == 0) {
#if (NOS_CONFIG_SCHED_PREEMPTIVE_ENABLE > 0) || (NOS_CONFIG_SCHED_ROUND_ROBIN_ENABLE > 0)
 #if (NOS_CONFIG_SCHED_LOCK_ENABLE > 0)
            if (nOS_lockNestingCounter == 0)
 #endif
            {
 #if (NOS_CONFIG_HIGHEST_THREAD_PRIO == 0)
                nOS_highPrioThread = nOS_GetHeadOfList(&nOS_readyThreadsList);
 #elif (NOS_CONFIG_SCHED_PREEMPTIVE_ENABLE > 0)
                nOS_highPrioThread = nOS_FindHighPrioThread();
 #else
                nOS_highPrioThread = nOS_GetHeadOfList(&nOS_readyThreadsList[nOS_runningThread->prio]);
 #endif
                nOS_runningThread = nOS_highPrioThread;
            }
#endif
            sp = nOS_runningThread->stackPtr;
        }
    }

    return sp;
}

#ifdef __cplusplus
}
#endif
