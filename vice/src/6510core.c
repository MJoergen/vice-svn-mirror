/*
 * 6510core.c - MOS6510 emulation core.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <viceteam@t-online.de>
 *
 * DTV sections written by
 *  M.Kiesel <mayne@users.sourceforge.net>
 *  Hannu Nuotio <hannu.nuotio@tut.fi>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

/* This file is included by CPU definition files */
/*  c64dtvcpu.c -> maincpu.c
    c64cpu.c -> maincpu.c
    c128cpu.c -> maincpu.c
    cbm2cpu.c -> maincpu.c
    petcpu.c -> maincpu.c
    plus4cpu.c -> maincpu.c
    drivecpu.c
*/

#ifdef DRIVE_CPU
#define CPU_STR "Drive CPU"
#else
#define CPU_STR "Main CPU"
#endif

#include "traps.h"

#ifndef C64DTV
/* The C64DTV can use different shadow registers for accu read/write. */
/* For standard 6510, this is not the case. */
#define reg_a_write(c) reg_a = (c)
#define reg_a_read  reg_a
#define reg_x_write(c) reg_x = (c)
#define reg_x_read  reg_x
#define reg_y_write(c) reg_y = (c)
#define reg_y_read  reg_y

/* Opcode execution time may vary on the C64DTV. */
#define CLK_RTS 3
#define CLK_RTI 4
#define CLK_BRK 5
#define CLK_ABS_I_STORE2 2
#define CLK_STACK_PUSH 1
#define CLK_STACK_PULL 2
#define CLK_ZERO_I_STORE 2
#define CLK_ZERO_I2 2
/* #define CLK_ABS_RMW2 3 */
/* #define CLK_ABS_I_RMW2 3 */
/* #define CLK_ZERO_RMW 3 */
/* #define CLK_ZERO_I_RMW 4 */
/* #define CLK_IND_X_RMW 3 */
/* #define CLK_IND_Y_RMW1 1 */
/* #define CLK_IND_Y_RMW2 3 */
#define CLK_BRANCH2 1
#define CLK_INT_CYCLE 1
#define CLK_JSR_INT_CYCLE 1
#define CLK_IND_Y_W 2
#define CLK_NOOP_ZERO_X 2

#define IRQ_CYCLES      7
#define NMI_CYCLES      7
#endif /* !C64DTV */
#define RESET_CYCLES    6

/* ------------------------------------------------------------------------- */
/* Backup for non-6509 CPUs.  */

#ifndef LOAD_IND
#define LOAD_IND(a)     LOAD(a)
#endif
#ifndef STORE_IND
#define STORE_IND(a, b)  STORE(a, b)
#endif

/* ------------------------------------------------------------------------- */
/* Backup for non-variable cycle CPUs.  */

#ifndef CLK_ADD
#define CLK_ADD(clock, amount) ((clock) += (amount))
#endif

#ifndef CLK_ADD_DUMMY
#define CLK_ADD_DUMMY(clock, amount) CLK_ADD(clock, amount)
#endif

#ifndef REWIND_FETCH_OPCODE
#define REWIND_FETCH_OPCODE(clock) ((clock) -= 2)
#endif

/* ------------------------------------------------------------------------- */
/* Hook for additional delay.  */

#ifndef CPU_DELAY_CLK
#define CPU_DELAY_CLK
#endif

#ifndef CPU_REFRESH_CLK
#define CPU_REFRESH_CLK
#endif

/* ------------------------------------------------------------------------- */

#ifndef CYCLE_EXACT_ALARM
#define PROCESS_ALARMS                                             \
    while (CLK >= alarm_context_next_pending_clk(ALARM_CONTEXT)) { \
        alarm_context_dispatch(ALARM_CONTEXT, CLK);                \
        CPU_DELAY_CLK                                              \
    }
#else
#define PROCESS_ALARMS
#endif

/* ------------------------------------------------------------------------- */

#define LOCAL_SET_NZ(val) (flag_z = flag_n = (val))

#if defined DRIVE_CPU
#define LOCAL_SET_OVERFLOW(val)               \
    do {                                      \
        if (val) {                            \
            reg_p |= P_OVERFLOW;              \
        } else {                              \
            drivecpu_rotate();                \
            drivecpu_byte_ready_egde_clear(); \
            reg_p &= ~P_OVERFLOW;             \
        }                                     \
    } while (0)
#else
#define LOCAL_SET_OVERFLOW(val)   \
    do {                          \
        if (val) {                \
            reg_p |= P_OVERFLOW;  \
        } else {                  \
            reg_p &= ~P_OVERFLOW; \
        }                         \
    } while (0)
#endif

#define LOCAL_SET_BREAK(val)   \
    do {                       \
        if (val) {             \
            reg_p |= P_BREAK;  \
        } else {               \
            reg_p &= ~P_BREAK; \
        }                      \
    } while (0)

#define LOCAL_SET_DECIMAL(val)   \
    do {                         \
        if (val) {               \
            reg_p |= P_DECIMAL;  \
        } else {                 \
            reg_p &= ~P_DECIMAL; \
        }                        \
    } while (0)

#define LOCAL_SET_INTERRUPT(val)   \
    do {                           \
        if (val) {                 \
            reg_p |= P_INTERRUPT;  \
        } else {                   \
            reg_p &= ~P_INTERRUPT; \
        }                          \
    } while (0)

#define LOCAL_SET_CARRY(val)   \
    do {                       \
        if (val) {             \
            reg_p |= P_CARRY;  \
        } else {               \
            reg_p &= ~P_CARRY; \
        }                      \
    } while (0)

#define LOCAL_SET_SIGN(val)      (flag_n = (val) ? 0x80 : 0)
#define LOCAL_SET_ZERO(val)      (flag_z = !(val))
#define LOCAL_SET_STATUS(val)    (reg_p = ((val) & ~(P_ZERO | P_SIGN)), \
                                  LOCAL_SET_ZERO((val) & P_ZERO),       \
                                  flag_n = (val))

#define LOCAL_OVERFLOW()         (reg_p & P_OVERFLOW)
#define LOCAL_BREAK()            (reg_p & P_BREAK)
#define LOCAL_DECIMAL()          (reg_p & P_DECIMAL)
#define LOCAL_INTERRUPT()        (reg_p & P_INTERRUPT)
#define LOCAL_CARRY()            (reg_p & P_CARRY)
#define LOCAL_SIGN()             (flag_n & 0x80)
#define LOCAL_ZERO()             (!flag_z)
#define LOCAL_STATUS()           (reg_p | (flag_n & 0x80) | P_UNUSED    \
                                  | (LOCAL_ZERO() ? P_ZERO : 0))

#ifdef LAST_OPCODE_INFO

/* If requested, gather some info about the last executed opcode for timing
   purposes.  */

/* Remember the number of the last opcode.  By default, the opcode does not
   delay interrupt and does not change the I flag.  */
#define SET_LAST_OPCODE(x) OPINFO_SET(LAST_OPCODE_INFO, (x), 0, 0, 0)

/* Remember that the last opcode delayed a pending IRQ or NMI by one cycle.  */
#define OPCODE_DELAYS_INTERRUPT() OPINFO_SET_DELAYS_INTERRUPT(LAST_OPCODE_INFO, 1)

/* Remember that the last opcode changed the I flag from 0 to 1, so we have
   to dispatch an IRQ even if the I flag is 0 when we check it.  */
#define OPCODE_DISABLES_IRQ() OPINFO_SET_DISABLES_IRQ(LAST_OPCODE_INFO, 1)

/* Remember that the last opcode changed the I flag from 1 to 0, so we must
   not dispatch an IRQ even if the I flag is 1 when we check it.  */
#define OPCODE_ENABLES_IRQ() OPINFO_SET_ENABLES_IRQ(LAST_OPCODE_INFO, 1)

#else

/* Info about the last opcode is not needed.  */
#define SET_LAST_OPCODE(x)
#define OPCODE_DELAYS_INTERRUPT()
#define OPCODE_DISABLES_IRQ()
#define OPCODE_ENABLES_IRQ()

#endif

#ifdef LAST_OPCODE_ADDR
#define SET_LAST_ADDR(x) LAST_OPCODE_ADDR = (x)
#else
#error "please define LAST_OPCODE_ADDR"
#endif

#ifndef DRIVE_CPU

#ifndef C64DTV
/* Export the local version of the registers.  */
#define EXPORT_REGISTERS()          \
    do {                            \
        GLOBAL_REGS.pc = reg_pc;    \
        GLOBAL_REGS.a = reg_a_read; \
        GLOBAL_REGS.x = reg_x_read; \
        GLOBAL_REGS.y = reg_y_read; \
        GLOBAL_REGS.sp = reg_sp;    \
        GLOBAL_REGS.p = reg_p;      \
        GLOBAL_REGS.n = flag_n;     \
        GLOBAL_REGS.z = flag_z;     \
    } while (0)

/* Import the public version of the registers.  */
#define IMPORT_REGISTERS()                                 \
    do {                                                   \
        reg_a_write(GLOBAL_REGS.a); /*TODO*/               \
        reg_x_write(GLOBAL_REGS.x);                        \
        reg_y_write(GLOBAL_REGS.y);                        \
        reg_sp = GLOBAL_REGS.sp;                           \
        reg_p = GLOBAL_REGS.p;                             \
        flag_n = GLOBAL_REGS.n;                            \
        flag_z = GLOBAL_REGS.z;                            \
        bank_start = bank_limit = 0; /* prevent caching */ \
        JUMP(GLOBAL_REGS.pc);                              \
    } while (0)
#else  /* C64DTV */

/* Export the local version of the registers.  */
#define EXPORT_REGISTERS()                                           \
    do {                                                             \
        GLOBAL_REGS.pc = reg_pc;                                     \
        GLOBAL_REGS.a = dtv_registers[0];                            \
        GLOBAL_REGS.x = dtv_registers[2];                            \
        GLOBAL_REGS.y = dtv_registers[1];                            \
        GLOBAL_REGS.sp = reg_sp;                                     \
        GLOBAL_REGS.p = reg_p;                                       \
        GLOBAL_REGS.n = flag_n;                                      \
        GLOBAL_REGS.z = flag_z;                                      \
        GLOBAL_REGS.r3 = dtv_registers[3];                           \
        GLOBAL_REGS.r4 = dtv_registers[4];                           \
        GLOBAL_REGS.r5 = dtv_registers[5];                           \
        GLOBAL_REGS.r6 = dtv_registers[6];                           \
        GLOBAL_REGS.r7 = dtv_registers[7];                           \
        GLOBAL_REGS.r8 = dtv_registers[8];                           \
        GLOBAL_REGS.r9 = dtv_registers[9];                           \
        GLOBAL_REGS.r10 = dtv_registers[10];                         \
        GLOBAL_REGS.r11 = dtv_registers[11];                         \
        GLOBAL_REGS.r12 = dtv_registers[12];                         \
        GLOBAL_REGS.r13 = dtv_registers[13];                         \
        GLOBAL_REGS.r14 = dtv_registers[14];                         \
        GLOBAL_REGS.r15 = dtv_registers[15];                         \
        GLOBAL_REGS.acm = (reg_a_write_idx << 4) | (reg_a_read_idx); \
        GLOBAL_REGS.yxm = (reg_y_idx << 4) | (reg_x_idx);            \
    } while (0)

/* Import the public version of the registers.  */
#define IMPORT_REGISTERS()                                 \
    do {                                                   \
        dtv_registers[0] = GLOBAL_REGS.a;                  \
        dtv_registers[2] = GLOBAL_REGS.x;                  \
        dtv_registers[1] = GLOBAL_REGS.y;                  \
        reg_sp = GLOBAL_REGS.sp;                           \
        reg_p = GLOBAL_REGS.p;                             \
        flag_n = GLOBAL_REGS.n;                            \
        flag_z = GLOBAL_REGS.z;                            \
        dtv_registers[3] = GLOBAL_REGS.r3;                 \
        dtv_registers[4] = GLOBAL_REGS.r4;                 \
        dtv_registers[5] = GLOBAL_REGS.r5;                 \
        dtv_registers[6] = GLOBAL_REGS.r6;                 \
        dtv_registers[7] = GLOBAL_REGS.r7;                 \
        dtv_registers[8] = GLOBAL_REGS.r8;                 \
        dtv_registers[9] = GLOBAL_REGS.r9;                 \
        dtv_registers[10] = GLOBAL_REGS.r10;               \
        dtv_registers[11] = GLOBAL_REGS.r11;               \
        dtv_registers[12] = GLOBAL_REGS.r12;               \
        dtv_registers[13] = GLOBAL_REGS.r13;               \
        dtv_registers[14] = GLOBAL_REGS.r14;               \
        dtv_registers[15] = GLOBAL_REGS.r15;               \
        reg_a_write_idx = GLOBAL_REGS.acm >> 4;            \
        reg_a_read_idx = GLOBAL_REGS.acm & 0xf;            \
        reg_y_idx = GLOBAL_REGS.yxm >> 4;                  \
        reg_x_idx = GLOBAL_REGS.yxm & 0xf;                 \
        bank_start = bank_limit = 0; /* prevent caching */ \
        JUMP(GLOBAL_REGS.pc);                              \
    } while (0)

#endif /* C64DTV */
#else  /* DRIVE_CPU */
#define IMPORT_REGISTERS()
#define EXPORT_REGISTERS()
#endif /* !DRIVE_CPU */

/* Stack operations. */

/* CAUTION: use STORE/LOAD macros instead of directly accessing the memory,
            else checkpoints may not trigger */
#if 0
#ifndef PUSH
#define PUSH(val) ((PAGE_ONE)[(reg_sp--)] = ((uint8_t)(val)))
#endif
#ifndef PULL
#define PULL()    ((PAGE_ONE)[(++reg_sp)])
#endif
#else
#ifndef PUSH
#define PUSH(val) { STORE(0x100 + reg_sp, (val)); reg_sp--; }
#endif
#ifndef PULL
#define PULL() (++reg_sp, LOAD(0x100 + reg_sp))
#endif
#endif

#ifdef DEBUG
#define TRACE_NMI(clk)                        \
    do {                                      \
        if (TRACEFLG) {                       \
            debug_nmi(CPU_INT_STATUS, (clk)); \
        }                                     \
    } while (0)

#define TRACE_IRQ(clk)                        \
    do {                                      \
        if (TRACEFLG) {                       \
            debug_irq(CPU_INT_STATUS, (clk)); \
        }                                     \
    } while (0)

#define TRACE_BRK()                \
    do {                           \
        if (TRACEFLG) {            \
            debug_text("*** BRK"); \
        }                          \
    } while (0)
#else
#define TRACE_NMI(clk)
#define TRACE_IRQ(clk)
#define TRACE_BRK()
#endif

/* Perform the interrupts in `int_kind'.  If we have both NMI and IRQ,
   execute NMI. NMI can take over an in progress IRQ.  */
/* FIXME: LOCAL_STATUS() should check byte ready first.  */
#define DO_INTERRUPT(int_kind)                                                                 \
    do {                                                                                       \
        uint8_t ik = (int_kind);                                                               \
                                                                                               \
        if (ik & (IK_IRQ | IK_IRQPEND | IK_NMI)) {                                             \
            if (((ik & IK_NMI)                                                                 \
                 && interrupt_check_nmi_delay(CPU_INT_STATUS, CLK))                            \
                || ((ik & (IK_IRQ | IK_IRQPEND)) && (!LOCAL_INTERRUPT()                        \
                                                     || OPINFO_DISABLES_IRQ(LAST_OPCODE_INFO)) \
                    && interrupt_check_irq_delay(CPU_INT_STATUS, CLK))) {                      \
                if (monitor_mask[CALLER] & (MI_STEP)) {                                        \
                    monitor_check_icount_interrupt();                                          \
                }                                                                              \
                if (NMI_CYCLES == 7) {                                                         \
                    FETCH_PARAM_DUMMY(reg_pc);   /* dummy reads */                             \
                    CLK_ADD(CLK, 1);                                                           \
                    FETCH_PARAM_DUMMY(reg_pc);                                                 \
                    CLK_ADD(CLK, 1);                                                           \
                }                                                                              \
                LOCAL_SET_BREAK(0);                                                            \
                PUSH(reg_pc >> 8);                                                             \
                PUSH(reg_pc & 0xff);                                                           \
                CLK_ADD(CLK, 2);                                                               \
                PUSH(LOCAL_STATUS());                                                          \
                CLK_ADD(CLK, 1);                                                               \
                LOCAL_SET_INTERRUPT(1);                                                        \
                CPU_DELAY_CLK; /* process alarms for cartridge freeze */                       \
                PROCESS_ALARMS;                                                                \
                if ((CPU_INT_STATUS->global_pending_int & IK_NMI)                              \
                    && (CLK >= (CPU_INT_STATUS->nmi_clk + INTERRUPT_DELAY))) {                 \
                    TRACE_NMI(CLK - NMI_CYCLES + 2);                                           \
                    interrupt_ack_nmi(CPU_INT_STATUS);                                         \
                    JUMP(LOAD_ADDR(0xfffa));                                                   \
                } else {                                                                       \
                    TRACE_IRQ(CLK - IRQ_CYCLES + 2);                                           \
                    interrupt_ack_irq(CPU_INT_STATUS);                                         \
                    JUMP(LOAD_ADDR(0xfffe));                                                   \
                }                                                                              \
                SET_LAST_OPCODE(0);                                                            \
                CLK_ADD(CLK, 2);                                                               \
            }                                                                                  \
        }                                                                                      \
        if (ik & (IK_TRAP | IK_RESET)) {                                                       \
            if (ik & IK_TRAP) {                                                                \
                EXPORT_REGISTERS();                                                            \
                interrupt_do_trap(CPU_INT_STATUS, (uint16_t)reg_pc);                           \
                IMPORT_REGISTERS();                                                            \
                if (CPU_INT_STATUS->global_pending_int & IK_RESET) {                           \
                    ik |= IK_RESET;                                                            \
                }                                                                              \
            }                                                                                  \
            if (ik & IK_RESET) {                                                               \
                interrupt_ack_reset(CPU_INT_STATUS);                                           \
                cpu_reset();                                                                   \
                bank_start = bank_limit = 0; /* prevent caching */                             \
                LOCAL_SET_INTERRUPT(1);                                                        \
                cpu_is_jammed = 0;                                                             \
                JUMP(LOAD_ADDR(0xfffc));                                                       \
                DMA_ON_RESET;                                                                  \
            }                                                                                  \
        }                                                                                      \
        if (ik & (IK_MONITOR | IK_DMA)) {                                                      \
            if (ik & IK_MONITOR) {                                                             \
                if (monitor_force_import(CALLER)) {                                            \
                    IMPORT_REGISTERS();                                                        \
                }                                                                              \
                if (monitor_mask[CALLER]) {                                                    \
                    EXPORT_REGISTERS();                                                        \
                }                                                                              \
                if (monitor_mask[CALLER] & (MI_STEP)) {                                        \
                    monitor_check_icount((uint16_t)reg_pc);                                    \
                    IMPORT_REGISTERS();                                                        \
                }                                                                              \
                if (monitor_mask[CALLER] & (MI_BREAK)) {                                       \
                    if (monitor_check_breakpoints(CALLER, (uint16_t)reg_pc)) {                 \
                        monitor_startup(CALLER);                                               \
                        IMPORT_REGISTERS();                                                    \
                    }                                                                          \
                }                                                                              \
                if (monitor_mask[CALLER] & (MI_WATCH)) {                                       \
                    monitor_check_watchpoints(LAST_OPCODE_ADDR, (uint16_t)reg_pc);             \
                    IMPORT_REGISTERS();                                                        \
                }                                                                              \
            }                                                                                  \
            if (ik & IK_DMA) {                                                                 \
                EXPORT_REGISTERS();                                                            \
                DMA_FUNC;                                                                      \
                interrupt_ack_dma(CPU_INT_STATUS);                                             \
                IMPORT_REGISTERS();                                                            \
            }                                                                                  \
        }                                                                                      \
    } while (0)

/* ------------------------------------------------------------------------- */

/* FIXME: we need to check if this shortcut will not make checkpoints not
          trigger in certain cases. */
#if 0
#define FETCH_PARAM(addr) ((((int)(addr)) < bank_limit) ? bank_base[(addr)] : LOAD(addr))
#define FETCH_PARAM_DUMMY(addr) ((((int)(addr)) < bank_limit) ? bank_base[(addr)] : LOAD_DUMMY(addr))
#else
#define FETCH_PARAM(addr) (LOAD(addr))
#define FETCH_PARAM_DUMMY(addr) (LOAD_DUMMY(addr))
#endif

/* Addressing modes.  For convenience, page boundary crossing cycles and
   ``idle'' memory reads are handled here as well. */

#define LOAD_ABS(addr) LOAD(addr)

#define LOAD_ABS_X(addr)                                                \
    ((((addr) & 0xff) + reg_x_read) > 0xff                              \
     ? (LOAD_DUMMY(((addr) & 0xff00) | (((addr) + reg_x_read) & 0xff)), \
        CLK_ADD(CLK, CLK_INT_CYCLE),                                    \
        LOAD((addr) + reg_x_read))                                      \
     : LOAD((addr) + reg_x_read))

#define NOOP_LOAD_ABS_X(addr)                                           \
    ((((addr) & 0xff) + reg_x_read) > 0xff                              \
     ? (LOAD_DUMMY(((addr) & 0xff00) | (((addr) + reg_x_read) & 0xff)), \
        CLK_ADD(CLK, CLK_INT_CYCLE),                                    \
        LOAD_DUMMY((addr) + reg_x_read))                                \
     : LOAD_DUMMY((addr) + reg_x_read))

#define LOAD_ABS_X_RMW(addr)                                         \
    (LOAD_DUMMY(((addr) & 0xff00) | (((addr) + reg_x_read) & 0xff)), \
     CLK_ADD(CLK, CLK_INT_CYCLE),                                    \
     LOAD((addr) + reg_x_read))

#define LOAD_ABS_Y(addr)                                                \
    ((((addr) & 0xff) + reg_y_read) > 0xff                              \
     ? (LOAD_DUMMY(((addr) & 0xff00) | (((addr) + reg_y_read) & 0xff)), \
        CLK_ADD(CLK, CLK_INT_CYCLE),                                    \
        LOAD((addr) + reg_y_read))                                      \
     : LOAD((addr) + reg_y_read))

#define LOAD_ABS_Y_RMW(addr)                                         \
    (LOAD_DUMMY(((addr) & 0xff00) | (((addr) + reg_y_read) & 0xff)), \
     CLK_ADD(CLK, CLK_INT_CYCLE),                                    \
     LOAD((addr) + reg_y_read))

#if 0
#define LOAD_IND_X(addr) (CLK_ADD(CLK, 3), LOAD(LOAD_ZERO_ADDR((addr) + reg_x_read)))
#else
#define LOAD_IND_X(addr)                                        \
    (CLK_ADD(CLK, 3),                                           \
    LOAD_ZERO_DUMMY(addr),                                      \
    tmpa = LOAD_ZERO((addr + reg_x_read) & 0xff),               \
    tmpa |= (LOAD_ZERO(((addr + reg_x_read) + 1) & 0xff) << 8), \
    LOAD(tmpa))

#endif

#if 0
#define LOAD_IND_Y(addr)                                                    \
    (CLK_ADD(CLK, 2), ((LOAD_ZERO_ADDR((addr)) & 0xff) + reg_y_read) > 0xff \
     ? (LOAD((LOAD_ZERO_ADDR((addr)) & 0xff00)                              \
             | ((LOAD_ZERO_ADDR((addr)) + reg_y_read) & 0xff)),             \
        CLK_ADD(CLK, CLK_INT_CYCLE),                                        \
        LOAD(LOAD_ZERO_ADDR((addr)) + reg_y_read))                          \
     : LOAD(LOAD_ZERO_ADDR((addr)) + reg_y_read))
#else
#define LOAD_IND_Y(addr)                                            \
    (CLK_ADD(CLK, 2),                                               \
    tmpa = LOAD_ZERO(addr),                                         \
    tmpa |= (LOAD_ZERO((addr + 1)) << 8),                           \
    ((((tmpa & 0xff) + reg_y_read) > 0xff) ?                        \
        (CLK_ADD(CLK, CLK_INT_CYCLE),                               \
        LOAD_DUMMY((tmpa & 0xff00) | ((tmpa + reg_y_read) & 0xff)), \
        LOAD(tmpa + reg_y_read)) :                                  \
        LOAD(tmpa + reg_y_read)))
#endif

#if 0
#define LOAD_ZERO_X(addr) (LOAD_ZERO((addr) + reg_x_read))
#define NOOP_LOAD_ZERO_X(addr) (LOAD_ZERO((addr) + reg_x_read))

#define LOAD_ZERO_Y(addr) (LOAD_ZERO((addr) + reg_y_read))
#else
#define LOAD_ZERO_X(addr)               \
    (LOAD_ZERO_DUMMY(addr),             \
    LOAD_ZERO(addr + reg_x_read))

#define NOOP_LOAD_ZERO_X(addr)          \
    (LOAD_ZERO_DUMMY(addr),             \
    LOAD_ZERO_DUMMY(addr + reg_x_read))

#define LOAD_ZERO_Y(addr)               \
    (LOAD_ZERO_DUMMY(addr),             \
    LOAD_ZERO(addr + reg_y_read))
#endif

#if 0
#define LOAD_IND_Y_BANK(addr)                                               \
    (CLK_ADD(CLK, 2), ((LOAD_ZERO_ADDR((addr)) & 0xff) + reg_y_read) > 0xff \
     ? (LOAD_IND((LOAD_ZERO_ADDR((addr)) & 0xff00)                          \
                 | ((LOAD_ZERO_ADDR((addr)) + reg_y_read) & 0xff)),         \
        CLK_ADD(CLK, CLK_INT_CYCLE),                                        \
        LOAD_IND(LOAD_ZERO_ADDR((addr)) + reg_y_read))                      \
     : LOAD_IND(LOAD_ZERO_ADDR((addr)) + reg_y_read))
#else
#define LOAD_IND_Y_BANK(addr)                                           \
    (CLK_ADD(CLK, 2),                                                   \
    tmpa = LOAD_ZERO(addr),                                             \
    tmpa |= (LOAD_ZERO(addr + 1) << 8),                                 \
    ((((tmpa & 0xff) + reg_y_read) > 0xff) ?                            \
        (CLK_ADD(CLK, CLK_INT_CYCLE),                                   \
        LOAD_DUMMY((tmpa & 0xff00) | ((tmpa + reg_y_read) & 0xff)),     \
        LOAD_IND(tmpa + reg_y_read)) :                                  \
        LOAD_IND(tmpa + reg_y_read)))
#endif

#define STORE_ABS(addr, value, inc) \
    do {                            \
        CLK_ADD(CLK, (inc));        \
        STORE((addr), (value));     \
    } while (0)

#define STORE_ABS_X(addr, value, inc)                                   \
    do {                                                                \
        CLK_ADD(CLK, (inc) - 2);                                        \
        LOAD_DUMMY((((addr) + reg_x_read) & 0xff) | ((addr) & 0xff00)); \
        CLK_ADD(CLK, 2);                                                \
        STORE((addr) + reg_x_read, (value));                            \
    } while (0)

#define STORE_ABS_X_RMW(addr, value, inc)    \
    do {                                     \
        CLK_ADD(CLK, (inc));                 \
        STORE((addr) + reg_x_read, (value)); \
    } while (0)

#define STORE_ABS_SH_X(addr, value, inc)                                \
    do {                                                                \
        unsigned int tmp2;                                              \
                                                                        \
        CLK_ADD(CLK, (inc) - 2);                                        \
        LOAD_DUMMY((((addr) + reg_x_read) & 0xff) | ((addr) & 0xff00)); \
        CLK_ADD(CLK, 2);                                                \
        tmp2 = (addr) + reg_x_read;                                     \
        if (((addr) & 0xff) + reg_x_read > 0xff) {                      \
            tmp2 = (tmp2 & 0xff) | ((value) << 8);                      \
        }                                                               \
        STORE(tmp2, (value));                                           \
    } while (0)

#define STORE_ABS_Y(addr, value, inc)                                   \
    do {                                                                \
        CLK_ADD(CLK, (inc) - 2);                                        \
        LOAD_DUMMY((((addr) + reg_y_read) & 0xff) | ((addr) & 0xff00)); \
        CLK_ADD(CLK, 2);                                                \
        STORE((addr) + reg_y_read, (value));                            \
    } while (0)

#define STORE_ABS_Y_RMW(addr, value, inc)    \
    do {                                     \
        CLK_ADD(CLK, (inc));                 \
        STORE((addr) + reg_y_read, (value)); \
    } while (0)

#define STORE_ABS_SH_Y(addr, value, inc)                                \
    do {                                                                \
        unsigned int tmp2;                                              \
                                                                        \
        CLK_ADD(CLK, (inc) - 2);                                        \
        LOAD_DUMMY((((addr) + reg_y_read) & 0xff) | ((addr) & 0xff00)); \
        CLK_ADD(CLK, 2);                                                \
        tmp2 = (addr) + reg_y_read;                                     \
        if (((addr) & 0xff) + reg_y_read > 0xff) {                      \
            tmp2 = (tmp2 & 0xff) | ((value) << 8);                      \
        }                                                               \
        STORE(tmp2, (value));                                           \
    } while (0)

#define INC_PC(value)   (reg_pc += (value))

/* macros to perform the dummy writes for RMW instructions. revert to the
   functions that do not actually perform the write when weirdo behaviour
   shows up */

#if 1

#define DUMMY_STORE_ABS_RMW(addr, value) \
    do {                                 \
        STORE_DUMMY(addr, value);        \
    } while (0)

#define DUMMY_STORE_ABS_X_RMW(addr, value)         \
    do {                                           \
        STORE_DUMMY((addr) + reg_x_read, (value)); \
    } while (0)

#define DUMMY_STORE_ABS_Y_RMW(addr, value)         \
    do {                                           \
        STORE_DUMMY((addr) + reg_y_read, (value)); \
    } while (0)
#else

/* FIXME: trigger write checkpoints */
#define DUMMY_STORE_ABS_RMW(addr, value)
#define DUMMY_STORE_ABS_X_RMW(addr, value)
#define DUMMY_STORE_ABS_Y_RMW(addr, value)

#endif
/* ------------------------------------------------------------------------- */

/* Opcodes.  */

/*
   A couple of caveats about PC:

   - the VIC-II emulation requires PC to be incremented before the first
     write access (this is not (very) important when writing to the zero
     page);

   - `p0', `p1' and `p2' can only be used *before* incrementing PC: some
     machines (eg. the C128) might depend on this.
*/

#define ADC(value, clk_inc, pc_inc)                                                                 \
    do {                                                                                            \
        unsigned int tmp_value;                                                                     \
        unsigned int tmp;                                                                           \
                                                                                                    \
        tmp_value = (value);                                                                        \
        CLK_ADD(CLK, (clk_inc));                                                                    \
                                                                                                    \
        if (LOCAL_DECIMAL()) {                                                                      \
            tmp = (reg_a_read & 0xf) + (tmp_value & 0xf) + (reg_p & 0x1);                           \
            if (tmp > 0x9) {                                                                        \
                tmp += 0x6;                                                                         \
            }                                                                                       \
            if (tmp <= 0x0f) {                                                                      \
                tmp = (tmp & 0xf) + (reg_a_read & 0xf0) + (tmp_value & 0xf0);                       \
            } else {                                                                                \
                tmp = (tmp & 0xf) + (reg_a_read & 0xf0) + (tmp_value & 0xf0) + 0x10;                \
            }                                                                                       \
            LOCAL_SET_ZERO(!((reg_a_read + tmp_value + (reg_p & 0x1)) & 0xff));                     \
            LOCAL_SET_SIGN(tmp & 0x80);                                                             \
            LOCAL_SET_OVERFLOW(((reg_a_read ^ tmp) & 0x80)  && !((reg_a_read ^ tmp_value) & 0x80)); \
            if ((tmp & 0x1f0) > 0x90) {                                                             \
                tmp += 0x60;                                                                        \
            }                                                                                       \
            LOCAL_SET_CARRY((tmp & 0xff0) > 0xf0);                                                  \
        } else {                                                                                    \
            tmp = tmp_value + reg_a_read + (reg_p & P_CARRY);                                       \
            LOCAL_SET_NZ(tmp & 0xff);                                                               \
            LOCAL_SET_OVERFLOW(!((reg_a_read ^ tmp_value) & 0x80)  && ((reg_a_read ^ tmp) & 0x80)); \
            LOCAL_SET_CARRY(tmp > 0xff);                                                            \
        }                                                                                           \
        reg_a_write(tmp);                                                                           \
        INC_PC(pc_inc);                                                                             \
    } while (0)

#define ANC(value, pc_inc)                       \
    do {                                         \
        uint8_t tmp = (uint8_t)(reg_a_read & (value)); \
        reg_a_write(tmp);                        \
        LOCAL_SET_NZ(tmp);                       \
        LOCAL_SET_CARRY(LOCAL_SIGN());           \
        INC_PC(pc_inc);                          \
    } while (0)

#define AND(value, clk_inc, pc_inc)              \
    do {                                         \
        uint8_t tmp = (uint8_t)(reg_a_read & (value)); \
        reg_a_write(tmp);                        \
        LOCAL_SET_NZ(tmp);                       \
        CLK_ADD(CLK, (clk_inc));                 \
        INC_PC(pc_inc);                          \
    } while (0)


/*
The result of the ANE opcode is A = ((A | CONST) & X & IMM), with CONST apparently
being both chip- and temperature dependent. There is also a dependency on the RDY
line, ie somehow bit4 and bit0 are affected in the cycle when a DMA starts.

The commonly used value for CONST in various documents is 0xee, which is however
not to be taken for granted (as it is unstable). see here:
http://visual6502.org/wiki/index.php?title=6502_Opcode_8B_(XAA,_ANE)

as seen in the list, there are several possible values, and its origin is still
kinda unknown. instead of the commonly used 0xee we use 0xef here, since this
appears to work with all known occurances of this opcode in real code:

known occurances of this opcode in actual code are:

- spectipede (original tape), use of ANE is unstable. bits 7,6,5,0 MUST be set
  in the magic constant (that makes it not work with the common 0xee, but 0xef
  works)
- turrican 3 (by smash designs), use of ANE is unstable. bits 6,1,0 MUST be set
  in the magic constant (that makes it not work with the common 0xee, but 0xef
  works)
- the ocean/imagine tape loader (yie ar kung fu, rambo first blood part ii,
  comic bakery), use of ANE is stable.

also see here:

https://sourceforge.net/tracker/?func=detail&aid=2110948&group_id=223021&atid=1057617

FIXME: in the unlikely event that other code surfaces that depends on another
CONST value, it probably has to be made configureable somehow if no value can
be found that works for both.

FIXME: perhaps we really have to add some randomness to (some) bits
*/

#define ANE_MAGIC       0xef
#define ANE_RDY_MAGIC   0xee

#ifndef ANE
#define ANE(value, pc_inc)                                                          \
    do {                                                                            \
        uint8_t tmp = ((reg_a_read | ANE_MAGIC) & reg_x_read & ((uint8_t)(value))); \
        reg_a_write(tmp);                                                           \
        LOCAL_SET_NZ(tmp);                                                          \
        INC_PC(pc_inc);                                                             \
    } while (0)
#endif

/* The fanciest opcode ever... ARR! */
#define ARR(value, pc_inc)                                          \
    do {                                                            \
        unsigned int tmp;                                           \
                                                                    \
        tmp = reg_a_read & (value);                                 \
        if (LOCAL_DECIMAL()) {                                      \
            int tmp_2 = tmp;                                        \
            tmp_2 |= (reg_p & P_CARRY) << 8;                        \
            tmp_2 >>= 1;                                            \
            LOCAL_SET_SIGN(LOCAL_CARRY());                          \
            LOCAL_SET_ZERO(!tmp_2);                                 \
            LOCAL_SET_OVERFLOW((tmp_2 ^ tmp) & 0x40);               \
            if (((tmp & 0xf) + (tmp & 0x1)) > 0x5) {                \
                tmp_2 = (tmp_2 & 0xf0) | ((tmp_2 + 0x6) & 0xf);     \
            }                                                       \
            if (((tmp & 0xf0) + (tmp & 0x10)) > 0x50) {             \
                tmp_2 = (tmp_2 & 0x0f) | ((tmp_2 + 0x60) & 0xf0);   \
                LOCAL_SET_CARRY(1);                                 \
            } else {                                                \
                LOCAL_SET_CARRY(0);                                 \
            }                                                       \
            reg_a_write(tmp_2);                                     \
        } else {                                                    \
            tmp |= (reg_p & P_CARRY) << 8;                          \
            tmp >>= 1;                                              \
            LOCAL_SET_NZ(tmp);                                      \
            LOCAL_SET_CARRY(tmp & 0x40);                            \
            LOCAL_SET_OVERFLOW((tmp & 0x40) ^ ((tmp & 0x20) << 1)); \
            reg_a_write(tmp);                                       \
        }                                                           \
        INC_PC(pc_inc);                                             \
    } while (0)

#define ASL(addr, pc_inc, load_func, store_func, dummy_func) \
    do {                                                  \
        unsigned int tmp_value, tmp_addr;                 \
                                                          \
        tmp_addr = (addr);                                \
        tmp_value = load_func(tmp_addr);                  \
        CLK_ADD(CLK, 1);                                  \
        CLK_ADD_DUMMY(CLK, 1);                            \
        dummy_func(tmp_addr, tmp_value);                  \
        LOCAL_SET_CARRY(tmp_value & 0x80);                \
        tmp_value = (tmp_value << 1) & 0xff;              \
        LOCAL_SET_NZ(tmp_value);                          \
        INC_PC(pc_inc);                                   \
        store_func(tmp_addr, tmp_value, 1);               \
    } while (0)

#define ASL_A()                      \
    do {                             \
        uint8_t tmp = reg_a_read;       \
        LOCAL_SET_CARRY(tmp & 0x80); \
        tmp <<= 1;                   \
        reg_a_write(tmp);            \
        LOCAL_SET_NZ(tmp);           \
        INC_PC(1);                   \
    } while (0)

#define ASR(value, pc_inc)                       \
    do {                                         \
        unsigned int tmp = reg_a_read & (value); \
        LOCAL_SET_CARRY(tmp & 0x01);             \
        tmp >>= 1;                               \
        reg_a_write(tmp);                        \
        LOCAL_SET_NZ(tmp);                       \
        INC_PC(pc_inc);                          \
    } while (0)

#define BIT(value, pc_inc)                   \
    do {                                     \
        unsigned int tmp;                    \
                                             \
        tmp = (value);                       \
        CLK_ADD(CLK, 1);                     \
        LOCAL_SET_SIGN(tmp & 0x80);          \
        LOCAL_SET_OVERFLOW(tmp & 0x40);      \
        LOCAL_SET_ZERO(!(tmp & reg_a_read)); \
        INC_PC(pc_inc);                      \
    } while (0)

#ifndef C64DTV
#define BRANCH(cond, value)                                               \
    do {                                                                  \
        INC_PC(2);                                                        \
                                                                          \
        if (cond) {                                                       \
            unsigned int dest_addr = reg_pc + (signed char)(value);       \
                                                                          \
            FETCH_PARAM_DUMMY(reg_pc);                                    \
            CLK_ADD(CLK, CLK_BRANCH2);                                    \
            if ((reg_pc ^ dest_addr) & 0xff00) {                          \
                LOAD_DUMMY((reg_pc & 0xff00) | (dest_addr & 0xff));       \
                CLK_ADD(CLK, CLK_BRANCH2);                                \
            } else {                                                      \
                OPCODE_DELAYS_INTERRUPT();                                \
            }                                                             \
            JUMP(dest_addr & 0xffff);                                     \
        }                                                                 \
    } while (0)
#endif

#define BRK()                                                                                     \
    do {                                                                                          \
        EXPORT_REGISTERS();                                                                       \
        INC_PC(2);                                                                                \
        LOCAL_SET_BREAK(1);                                                                       \
        PUSH(reg_pc >> 8);                                                                        \
        PUSH(reg_pc & 0xff);                                                                      \
        CLK_ADD(CLK, CLK_BRK - 3);                                                                \
        PUSH(LOCAL_STATUS());                                                                     \
        CLK_ADD(CLK, 1);                                                                          \
        CPU_DELAY_CLK  /* process alarms for cartridge freeze */                                  \
        PROCESS_ALARMS                                                                            \
        if ((CPU_INT_STATUS->global_pending_int & IK_NMI)                                         \
            && (CLK >= (CPU_INT_STATUS->nmi_clk + INTERRUPT_DELAY))) {                            \
            LOCAL_SET_INTERRUPT(1);                                                               \
            TRACE_NMI(CLK - CLK_BRK);                                                             \
            if (monitor_mask[CALLER] & (MI_STEP)) {                                               \
                monitor_check_icount_interrupt();                                                 \
            }                                                                                     \
            interrupt_ack_nmi(CPU_INT_STATUS);                                                    \
            JUMP(LOAD_ADDR(0xfffa));                                                              \
        } else if ((CPU_INT_STATUS->global_pending_int & (IK_IRQ | IK_IRQPEND))                   \
                 && !LOCAL_INTERRUPT() && (CLK >= (CPU_INT_STATUS->irq_clk + INTERRUPT_DELAY))) { \
            LOCAL_SET_INTERRUPT(1);                                                               \
            TRACE_IRQ(CLK - CLK_BRK);                                                             \
            if (monitor_mask[CALLER] & (MI_STEP)) {                                               \
                monitor_check_icount_interrupt();                                                 \
            }                                                                                     \
            interrupt_ack_irq(CPU_INT_STATUS);                                                    \
            JUMP(LOAD_ADDR(0xfffe));                                                              \
        } else {                                                                                  \
            TRACE_BRK();                                                                          \
            LOCAL_SET_INTERRUPT(1);                                                               \
            JUMP(LOAD_ADDR(0xfffe));                                                              \
        }                                                                                         \
        CLK_ADD(CLK, 2);                                                                          \
    } while (0)

#define CLC()               \
    do {                    \
        INC_PC(1);          \
        LOCAL_SET_CARRY(0); \
    } while (0)

#define CLD()                 \
    do {                      \
        INC_PC(1);            \
        LOCAL_SET_DECIMAL(0); \
    } while (0)

#define CLI()                     \
    do {                          \
        INC_PC(1);                \
        if (LOCAL_INTERRUPT()) {  \
            OPCODE_ENABLES_IRQ(); \
        }                         \
        LOCAL_SET_INTERRUPT(0);   \
    } while (0)

#define CLV()                  \
    do {                       \
        INC_PC(1);             \
        LOCAL_SET_OVERFLOW(0); \
    } while (0)

#define CMP(value, clk_inc, pc_inc)   \
    do {                              \
        unsigned int tmp;             \
                                      \
        tmp = reg_a_read - (value);   \
        LOCAL_SET_CARRY(tmp < 0x100); \
        LOCAL_SET_NZ(tmp & 0xff);     \
        CLK_ADD(CLK, (clk_inc));      \
        INC_PC(pc_inc);               \
    } while (0)

#define CPX(value, clk_inc, pc_inc)   \
    do {                              \
        unsigned int tmp;             \
                                      \
        tmp = reg_x_read - (value);   \
        LOCAL_SET_CARRY(tmp < 0x100); \
        LOCAL_SET_NZ(tmp & 0xff);     \
        CLK_ADD(CLK, (clk_inc));      \
        INC_PC(pc_inc);               \
    } while (0)

#define CPY(value, clk_inc, pc_inc)   \
    do {                              \
        unsigned int tmp;             \
                                      \
        tmp = reg_y_read - (value);   \
        LOCAL_SET_CARRY(tmp < 0x100); \
        LOCAL_SET_NZ(tmp & 0xff);     \
        CLK_ADD(CLK, (clk_inc));      \
        INC_PC(pc_inc);               \
    } while (0)

#define DCP(addr, clk_inc1, pc_inc, load_func, store_func, dummy_func) \
    do {                                                             \
        unsigned int tmp, tmp_addr;                                  \
                                                                     \
        tmp_addr = (addr);                                           \
        CLK_ADD(CLK, (clk_inc1));                                    \
        tmp = load_func(tmp_addr);                                   \
        CLK_ADD(CLK, 1);                                             \
        CLK_ADD_DUMMY(CLK, 1);                                       \
        dummy_func(tmp_addr, tmp);                                   \
        tmp = (tmp - 1) & 0xff;                                      \
        LOCAL_SET_CARRY(reg_a_read >= tmp);                          \
        LOCAL_SET_NZ((reg_a_read - tmp));                            \
        INC_PC(pc_inc);                                              \
        store_func(tmp_addr, tmp, 1);                                \
    } while (0)

#define DCP_IND_Y(addr)                                                     \
    do {                                                                    \
        unsigned int tmp;                                                   \
        unsigned int tmp_addr = LOAD_ZERO_ADDR(addr);                       \
                                                                            \
        CLK_ADD(CLK, 2);                                                    \
        LOAD_DUMMY((tmp_addr & 0xff00) | ((tmp_addr + reg_y_read) & 0xff)); \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        tmp_addr += reg_y_read;                                             \
        tmp = LOAD(tmp_addr);                                               \
        CLK_ADD(CLK, 1);                                                    \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        DUMMY_STORE_ABS_RMW(tmp_addr, tmp);                                 \
        tmp = (tmp - 1) & 0xff;                                             \
        LOCAL_SET_CARRY(reg_a_read >= tmp);                                 \
        LOCAL_SET_NZ((reg_a_read - tmp));                                   \
        INC_PC(2);                                                          \
        STORE_ABS(tmp_addr, tmp, 1);                                        \
    } while (0)

#define DEC(addr, pc_inc, load_func, store_func, dummy_func) \
    do {                                                  \
        unsigned int tmp, tmp_addr;                       \
                                                          \
        tmp_addr = (addr);                                \
        tmp = load_func(tmp_addr);                        \
        CLK_ADD(CLK, 1);                                  \
        CLK_ADD_DUMMY(CLK, 1);                            \
        dummy_func(tmp_addr, tmp);                        \
        tmp = (tmp - 1) & 0xff;                           \
        LOCAL_SET_NZ(tmp);                                \
        INC_PC(pc_inc);                                   \
        store_func(tmp_addr, tmp, 1);                     \
    } while (0)

#define DEX()                        \
    do {                             \
        reg_x_write(reg_x_read - 1); \
        LOCAL_SET_NZ(reg_x_read);    \
        INC_PC(1);                   \
    } while (0)

#define DEY()                        \
    do {                             \
        reg_y_write(reg_y_read - 1); \
        LOCAL_SET_NZ(reg_y_read);    \
        INC_PC(1);                   \
    } while (0)

#define EOR(value, clk_inc, pc_inc)              \
    do {                                         \
        uint8_t tmp = (uint8_t)(reg_a_read ^ (value)); \
        reg_a_write(tmp);                        \
        LOCAL_SET_NZ(tmp);                       \
        CLK_ADD(CLK, (clk_inc));                 \
        INC_PC(pc_inc);                          \
    } while (0)

#define INC(addr, pc_inc, load_func, store_func, dummy_func) \
    do {                                                              \
        unsigned int tmp, tmp_addr;                                   \
                                                                      \
        tmp_addr = (addr);                                            \
        tmp = load_func(tmp_addr);                                    \
        CLK_ADD(CLK, 1);                                              \
        CLK_ADD_DUMMY(CLK, 1);                                        \
        dummy_func(tmp_addr, tmp);                                    \
        tmp = (tmp + 1) & 0xff;                                       \
        LOCAL_SET_NZ(tmp);                                            \
        INC_PC(pc_inc);                                               \
        store_func(tmp_addr, tmp, 1);                                 \
    } while (0)

#define INX()                        \
    do {                             \
        reg_x_write(reg_x_read + 1); \
        LOCAL_SET_NZ(reg_x_read);    \
        INC_PC(1);                   \
    } while (0)

#define INY()                        \
    do {                             \
        reg_y_write(reg_y_read + 1); \
        LOCAL_SET_NZ(reg_y_read);    \
        INC_PC(1);                   \
    } while (0)

#define ISB(addr, clk_inc1, pc_inc, load_func, store_func, dummy_func) \
    do {                                                             \
        uint8_t my_src;                                              \
        int my_addr = (addr);                                        \
                                                                     \
        CLK_ADD(CLK, (clk_inc1));                                    \
        my_src = load_func(my_addr);                                 \
        CLK_ADD(CLK, 1);                                             \
        CLK_ADD_DUMMY(CLK, 1);                                       \
        dummy_func(my_addr, my_src);                                 \
        my_src = (my_src + 1) & 0xff;                                \
        SBC(my_src, 0, 0);                                           \
        INC_PC(pc_inc);                                              \
        store_func(my_addr, my_src, 1);                              \
    } while (0)

#define ISB_IND_Y(addr)                                                   \
    do {                                                                  \
        uint8_t my_src;                                                   \
        int my_addr = LOAD_ZERO_ADDR(addr);                               \
                                                                          \
        CLK_ADD(CLK, 2);                                                  \
        LOAD_DUMMY((my_addr & 0xff00) | ((my_addr + reg_y_read) & 0xff)); \
        CLK_ADD_DUMMY(CLK, 1);                                            \
        my_addr += reg_y_read;                                            \
        my_src = LOAD(my_addr);                                           \
        CLK_ADD(CLK, 1);                                                  \
        CLK_ADD_DUMMY(CLK, 1);                                            \
        DUMMY_STORE_ABS_RMW(my_addr, my_src);                             \
        my_src = (my_src + 1) & 0xff;                                     \
        SBC(my_src, 0, 0);                                                \
        INC_PC(2);                                                        \
        STORE_ABS(my_addr, my_src, 1);                                    \
    } while (0)

/* The 0x02 JAM opcode is also used to patch the ROM.  The function trap_handler()
   returns nonzero if this is not a patch, but a `real' JAM instruction. */

#define JAM_02()                                                                         \
    do {                                                                                 \
        uint32_t trap_result;                                                            \
        EXPORT_REGISTERS();                                                              \
        if (!ROM_TRAP_ALLOWED() || (trap_result = ROM_TRAP_HANDLER()) == (uint32_t)-1) { \
            cpu_is_jammed = 1;                                                           \
            REWIND_FETCH_OPCODE(CLK);                                                    \
            JAM();                                                                       \
        } else {                                                                         \
            if (trap_result) {                                                           \
                REWIND_FETCH_OPCODE(CLK);                                                \
                SET_OPCODE(trap_result);                                                 \
                IMPORT_REGISTERS();                                                      \
                goto trap_skipped;                                                       \
            } else {                                                                     \
                IMPORT_REGISTERS();                                                      \
            }                                                                            \
        }                                                                                \
    } while (0)

#define JMP(addr)   \
    do {            \
        JUMP(addr); \
    } while (0)

#define JMP_IND()                                                    \
    do {                                                             \
        uint16_t dest_addr;                                              \
        dest_addr = LOAD(p2);                                        \
        CLK_ADD(CLK, 1);                                             \
        dest_addr |= (LOAD((p2 & 0xff00) | ((p2 + 1) & 0xff)) << 8); \
        CLK_ADD(CLK, 1);                                             \
        JUMP(dest_addr);                                             \
    } while (0)

/* HACK: fix JSR MSB in monitor CPU history */
#if defined(FEATURE_CPUMEMHISTORY) && !defined(DRIVE_CPU)
#define JSR_FIXUP_MSB(x)    monitor_cpuhistory_fix_p2(x)
#else
#define JSR_FIXUP_MSB(x)
#endif

#define JSR()                                         \
    do {                                              \
        uint8_t addr_msb;                             \
        unsigned int tmp_addr;                        \
                                                      \
        LOAD_DUMMY(0x100 + reg_sp);                   \
        CLK_ADD(CLK, 1);                              \
        INC_PC(2);                                    \
        CLK_ADD(CLK, 2);                              \
        PUSH(((reg_pc) >> 8) & 0xff);                 \
        PUSH((reg_pc) & 0xff);                        \
        addr_msb = LOAD(reg_pc);                      \
        JSR_FIXUP_MSB(addr_msb);                      \
        tmp_addr = (p1 | (addr_msb << 8));            \
        CLK_ADD(CLK, CLK_JSR_INT_CYCLE);              \
        JUMP(tmp_addr);                               \
    } while (0)

#define LAS(value, clk_inc, pc_inc) \
    do {                            \
        reg_sp = reg_sp & (value);  \
        reg_x_write(reg_sp);        \
        reg_a_write(reg_sp);        \
        LOCAL_SET_NZ(reg_sp);       \
        CLK_ADD(CLK, (clk_inc));    \
        INC_PC(pc_inc);             \
    } while (0)

#define LAX(value, clk_inc, pc_inc) \
    do {                            \
        uint8_t tmp = (value);         \
        reg_x_write(tmp);           \
        reg_a_write(tmp);           \
        LOCAL_SET_NZ(tmp);          \
        CLK_ADD(CLK, (clk_inc));    \
        INC_PC(pc_inc);             \
    } while (0)

#define LDA(value, clk_inc, pc_inc) \
    do {                            \
        uint8_t tmp = (uint8_t)(value);   \
        reg_a_write(tmp);           \
        CLK_ADD(CLK, (clk_inc));    \
        LOCAL_SET_NZ(tmp);          \
        INC_PC(pc_inc);             \
    } while (0)

#define LDX(value, clk_inc, pc_inc) \
    do {                            \
        reg_x_write((uint8_t)(value)); \
        LOCAL_SET_NZ(reg_x_read);   \
        CLK_ADD(CLK, (clk_inc));    \
        INC_PC(pc_inc);             \
    } while (0)

#define LDY(value, clk_inc, pc_inc) \
    do {                            \
        reg_y_write((uint8_t)(value)); \
        LOCAL_SET_NZ(reg_y_read);   \
        CLK_ADD(CLK, (clk_inc));    \
        INC_PC(pc_inc);             \
    } while (0)

#define LSR(addr, pc_inc, load_func, store_func, dummy_func) \
    do {                                                  \
        unsigned int tmp, tmp_addr;                       \
                                                          \
        tmp_addr = (addr);                                \
        tmp = load_func(tmp_addr);                        \
        CLK_ADD(CLK, 1);                                  \
        CLK_ADD_DUMMY(CLK, 1);                            \
        dummy_func(tmp_addr, tmp);                        \
        LOCAL_SET_CARRY(tmp & 0x01);                      \
        tmp >>= 1;                                        \
        LOCAL_SET_NZ(tmp);                                \
        INC_PC(pc_inc);                                   \
        store_func(tmp_addr, tmp, 1);                     \
    } while (0)

#define LSR_A()                        \
    do {                               \
        unsigned int tmp = reg_a_read; \
        LOCAL_SET_CARRY(tmp & 0x01);   \
        tmp >>= 1;                     \
        reg_a_write(tmp);              \
        LOCAL_SET_NZ(tmp);             \
        INC_PC(1);                     \
    } while (0)

/*
The result of the LXA opcode is A = X = ((A | CONST) & IMM), with CONST apparently
being both chip- and temperature dependent. There is also a dependency on the RDY
line, ie somehow bit4 and bit0 are affected in the cycle when a DMA starts.

The commonly used value for CONST in various documents is 0xee, which is however
not to be taken for granted (as it is unstable).

FIXME: in the unlikely event that other code surfaces that depends on another
CONST value, it probably has to be made configureable somehow if no value can
be found that works for both.

FIXME: perhaps we really have to add some randomness to (some) bits
*/

#define LXA_MAGIC       0xee    /* needs to be 0xee for wizball */
#define LXA_RDY_MAGIC   0xee

#ifndef LXA
#define LXA(value, pc_inc)                                             \
    do {                                                               \
        uint8_t tmp = ((reg_a_read | LXA_MAGIC) & ((uint8_t)(value))); \
        reg_x_write(tmp);                                              \
        reg_a_write(tmp);                                              \
        LOCAL_SET_NZ(tmp);                                             \
        INC_PC(pc_inc);                                                \
    } while (0)
#endif

#define ORA(value, clk_inc, pc_inc)              \
    do {                                         \
        uint8_t tmp = (uint8_t)(reg_a_read | (value)); \
        reg_a_write(tmp);                        \
        LOCAL_SET_NZ(tmp);                       \
        CLK_ADD(CLK, (clk_inc));                 \
        INC_PC(pc_inc);                          \
    } while (0)

#define NOOP(clk_inc, pc_inc) (CLK_ADD(CLK, (clk_inc)), INC_PC(pc_inc))

#define NOOP_IMM(pc_inc) INC_PC(pc_inc)

#define NOOP_ABS()       \
    do {                 \
        LOAD(p2);        \
        CLK_ADD(CLK, 1); \
        INC_PC(3);       \
    } while (0)

#define NOOP_ABS_X()     \
    do {                 \
        NOOP_LOAD_ABS_X(p2);  \
        CLK_ADD(CLK, 1); \
        INC_PC(3);       \
    } while (0)

#define NOP()  NOOP_IMM(1)

#define PHA()                         \
    do {                              \
        CLK_ADD(CLK, CLK_STACK_PUSH); \
        PUSH(reg_a_read);             \
        INC_PC(1);                    \
    } while (0)

#define PHP()                           \
    do {                                \
        CLK_ADD(CLK, CLK_STACK_PUSH);   \
        PUSH(LOCAL_STATUS() | P_BREAK); \
        INC_PC(1);                      \
    } while (0)

#define PLA()                         \
    do {                              \
        uint8_t tmp;                  \
        CLK_ADD(CLK, CLK_STACK_PULL); \
        LOAD_DUMMY(0x100 + reg_sp);   \
        tmp = PULL();                 \
        reg_a_write(tmp);             \
        LOCAL_SET_NZ(tmp);            \
        INC_PC(1);                    \
    } while (0)

/* FIXME: Rotate disk before executing LOCAL_SET_STATUS().  */
#define PLP()                                                 \
    do {                                                      \
        uint8_t s;                                            \
        LOAD_DUMMY(0x100 + reg_sp);                           \
        s = PULL();                                           \
                                                              \
        if (!(s & P_INTERRUPT) && LOCAL_INTERRUPT()) {        \
            OPCODE_ENABLES_IRQ();                             \
        } else if ((s & P_INTERRUPT) && !LOCAL_INTERRUPT()) { \
            OPCODE_DISABLES_IRQ();                            \
        }                                                     \
        CLK_ADD(CLK, CLK_STACK_PULL);                         \
        LOCAL_SET_STATUS(s);                                  \
        INC_PC(1);                                            \
    } while (0)

#define RLA(addr, clk_inc1, pc_inc, load_func, store_func, dummy_func) \
    do {                                                             \
        unsigned int tmp, tmp2, tmp_addr;                            \
                                                                     \
        tmp_addr = (addr);                                           \
        CLK_ADD(CLK, (clk_inc1));                                    \
        tmp = load_func(tmp_addr);                                   \
        CLK_ADD(CLK, 1);                                             \
        CLK_ADD_DUMMY(CLK, 1);                                       \
        dummy_func(tmp_addr, tmp);                                   \
        tmp = ((tmp << 1) | (reg_p & P_CARRY));                      \
        LOCAL_SET_CARRY(tmp & 0x100);                                \
        tmp2 = reg_a_read & tmp;                                     \
        reg_a_write(tmp2);                                           \
        LOCAL_SET_NZ(tmp2);                                          \
        INC_PC(pc_inc);                                              \
        store_func(tmp_addr, tmp, 1);                                \
    } while (0)

#define RLA_IND_Y(addr)                                                     \
    do {                                                                    \
        unsigned int tmp, tmp2;                                             \
        unsigned int tmp_addr = LOAD_ZERO_ADDR(addr);                       \
                                                                            \
        CLK_ADD(CLK, 2);                                                    \
        LOAD_DUMMY((tmp_addr & 0xff00) | ((tmp_addr + reg_y_read) & 0xff)); \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        tmp_addr += reg_y_read;                                             \
        tmp = LOAD(tmp_addr);                                               \
        CLK_ADD(CLK, 1);                                                    \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        DUMMY_STORE_ABS_RMW(tmp_addr, tmp);                                 \
        tmp = ((tmp << 1) | (reg_p & P_CARRY));                             \
        LOCAL_SET_CARRY(tmp & 0x100);                                       \
        tmp2 = reg_a_read & tmp;                                            \
        reg_a_write(tmp2);                                                  \
        LOCAL_SET_NZ(tmp2);                                                 \
        INC_PC(2);                                                          \
        STORE_ABS(tmp_addr, tmp, 1);                                        \
    } while (0)

#define ROL(addr, pc_inc, load_func, store_func, dummy_func) \
    do {                                                  \
        unsigned int tmp, tmp_addr;                       \
                                                          \
        tmp_addr = (addr);                                \
        tmp = load_func(tmp_addr);                        \
        CLK_ADD(CLK, 1);                                  \
        CLK_ADD_DUMMY(CLK, 1);                            \
        dummy_func(tmp_addr, tmp);                        \
        tmp = (tmp << 1) | (reg_p & P_CARRY);             \
        LOCAL_SET_CARRY(tmp & 0x100);                     \
        LOCAL_SET_NZ(tmp & 0xff);                         \
        INC_PC(pc_inc);                                   \
        store_func(tmp_addr, tmp, 1);                     \
    } while (0)

#define ROL_A()                             \
    do {                                    \
        unsigned int tmp = reg_a_read << 1; \
                                            \
        tmp |= (reg_p & P_CARRY);           \
        reg_a_write(tmp);                   \
        LOCAL_SET_NZ(tmp);                  \
        LOCAL_SET_CARRY(tmp & 0x100);       \
        INC_PC(1);                          \
    } while (0)

#define ROR(addr, pc_inc, load_func, store_func, dummy_func) \
    do {                                                  \
        unsigned int src, tmp_addr;                       \
                                                          \
        tmp_addr = (addr);                                \
        src = load_func(tmp_addr);                        \
        CLK_ADD(CLK, 1);                                  \
        CLK_ADD_DUMMY(CLK, 1);                            \
        dummy_func(tmp_addr, src);                        \
        if (reg_p & P_CARRY) {                            \
            src |= 0x100;                                 \
        }                                                 \
        LOCAL_SET_CARRY(src & 0x01);                      \
        src >>= 1;                                        \
        LOCAL_SET_NZ(src);                                \
        INC_PC(pc_inc);                                   \
        store_func(tmp_addr, src, 1);                     \
    } while (0)

#define ROR_A()                              \
    do {                                     \
        unsigned int tmp = reg_a_read, tmp2; \
        tmp2 = (tmp >> 1) | (reg_p << 7);    \
        LOCAL_SET_CARRY(tmp & 0x01);         \
        reg_a_write(tmp2);                   \
        LOCAL_SET_NZ(tmp2);                  \
        INC_PC(1);                           \
    } while (0)

#define RRA(addr, clk_inc1, pc_inc, load_func, store_func, dummy_func) \
    do {                                                             \
        uint8_t src;                                                 \
        unsigned int my_temp, tmp_addr;                              \
                                                                     \
        tmp_addr = (addr);                                           \
        CLK_ADD(CLK, (clk_inc1));                                    \
        src = load_func(tmp_addr);                                   \
        CLK_ADD(CLK, 1);                                             \
        CLK_ADD_DUMMY(CLK, 1);                                       \
        dummy_func(tmp_addr, src);                                   \
        my_temp = src >> 1;                                          \
        if (reg_p & P_CARRY) {                                       \
            my_temp |= 0x80;                                         \
        }                                                            \
        LOCAL_SET_CARRY(src & 0x1);                                  \
        INC_PC(pc_inc);                                              \
        ADC(my_temp, 0, 0);                                          \
        store_func(tmp_addr, my_temp, 1);                            \
    } while (0)

#define RRA_IND_Y(addr)                                                           \
    do {                                                                          \
        uint8_t src;                                                              \
        unsigned int my_tmp_addr;                                                 \
        unsigned int my_temp;                                                     \
                                                                                  \
        my_tmp_addr = LOAD_ZERO_ADDR(addr);                                       \
        CLK_ADD(CLK, 2);                                                          \
        LOAD_DUMMY((my_tmp_addr & 0xff00) | ((my_tmp_addr + reg_y_read) & 0xff)); \
        CLK_ADD_DUMMY(CLK, 1);                                                    \
        my_tmp_addr += reg_y_read;                                                \
        src = LOAD(my_tmp_addr);                                                  \
        CLK_ADD(CLK, 1);                                                          \
        CLK_ADD_DUMMY(CLK, 1);                                                    \
        DUMMY_STORE_ABS_RMW(my_tmp_addr, src);                                    \
        INC_PC(2);                                                                \
        my_temp = src >> 1;                                                       \
        if (reg_p & P_CARRY) {                                                    \
            my_temp |= 0x80;                                                      \
        }                                                                         \
        LOCAL_SET_CARRY(src & 0x1);                                               \
        ADC(my_temp, 0, 0);                                                       \
        STORE_ABS(my_tmp_addr, my_temp, 1);                                       \
    } while (0)

/* RTI does must not use `OPCODE_ENABLES_IRQ()' even if the I flag changes
   from 1 to 0 because the value of I is set 3 cycles before the end of the
   opcode, and thus the 6510 has enough time to call the interrupt routine as
   soon as the opcode ends, if necessary.  */
/* FIXME: Rotate disk before executing LOCAL_SET_STATUS().  */
#define RTI()                           \
    do {                                \
        uint16_t tmp;                   \
                                        \
        CLK_ADD(CLK, CLK_RTI);          \
        LOAD_DUMMY(0x100 + reg_sp);     \
        tmp = (uint16_t)PULL();         \
        LOCAL_SET_STATUS((uint8_t)tmp); \
        tmp = (uint16_t)PULL();         \
        tmp |= (uint16_t)PULL() << 8;   \
        JUMP(tmp);                      \
    } while (0)

#define RTS()                        \
    do {                             \
        uint16_t tmp;                \
                                     \
        CLK_ADD(CLK, CLK_RTS);       \
        LOAD_DUMMY(0x100 + reg_sp);  \
        tmp = PULL();                \
        tmp = tmp | (PULL() << 8);   \
        JUMP(tmp);                   \
        FETCH_PARAM(reg_pc);         \
        CLK_ADD(CLK, CLK_INT_CYCLE); \
        INC_PC(1);                   \
    } while (0)

#define SAX(addr, clk_inc1, clk_inc2, pc_inc) \
    do {                                      \
        unsigned int tmp;                     \
                                              \
        CLK_ADD(CLK, (clk_inc1));             \
        tmp = (addr);                         \
        CLK_ADD(CLK, (clk_inc2));             \
        INC_PC(pc_inc);                       \
        STORE(tmp, reg_a_read & reg_x_read);  \
    } while (0)

#define SAX_ZERO(addr, clk_inc, pc_inc)              \
    do {                                             \
        CLK_ADD(CLK, (clk_inc));                     \
        STORE_ZERO((addr), reg_a_read & reg_x_read); \
        INC_PC(pc_inc);                              \
    } while (0)

#define SBC(value, clk_inc, pc_inc)                                                         \
    do {                                                                                    \
        uint16_t src, tmp;                                                                      \
                                                                                            \
        src = (uint16_t)(value);                                                                \
        CLK_ADD(CLK, (clk_inc));                                                            \
        tmp = reg_a_read - src - ((reg_p & P_CARRY) ? 0 : 1);                               \
        if (reg_p & P_DECIMAL) {                                                            \
            unsigned int tmp_a;                                                             \
            tmp_a = (reg_a_read & 0xf) - (src & 0xf) - ((reg_p & P_CARRY) ? 0 : 1);         \
            if (tmp_a & 0x10) {                                                             \
                tmp_a = ((tmp_a - 6) & 0xf) | ((reg_a_read & 0xf0) - (src & 0xf0) - 0x10);  \
            } else {                                                                        \
                tmp_a = (tmp_a & 0xf) | ((reg_a_read & 0xf0) - (src & 0xf0));               \
            }                                                                               \
            if (tmp_a & 0x100) {                                                            \
                tmp_a -= 0x60;                                                              \
            }                                                                               \
            LOCAL_SET_CARRY(tmp < 0x100);                                                   \
            LOCAL_SET_NZ(tmp & 0xff);                                                       \
            LOCAL_SET_OVERFLOW(((reg_a_read ^ tmp) & 0x80) && ((reg_a_read ^ src) & 0x80)); \
            reg_a_write((uint8_t) tmp_a);                                                      \
        } else {                                                                            \
            LOCAL_SET_NZ(tmp & 0xff);                                                       \
            LOCAL_SET_CARRY(tmp < 0x100);                                                   \
            LOCAL_SET_OVERFLOW(((reg_a_read ^ tmp) & 0x80) && ((reg_a_read ^ src) & 0x80)); \
            reg_a_write((uint8_t) tmp);                                                        \
        }                                                                                   \
        INC_PC(pc_inc);                                                                     \
    } while (0)

#define SBX(value, pc_inc)                     \
    do {                                       \
        unsigned int tmp;                      \
                                               \
        tmp = (value);                         \
        INC_PC(pc_inc);                        \
        tmp = (reg_a_read & reg_x_read) - tmp; \
        LOCAL_SET_CARRY(tmp < 0x100);          \
        reg_x_write(tmp & 0xff);               \
        LOCAL_SET_NZ(reg_x_read);              \
    } while (0)

#undef SEC    /* defined in time.h on SunOS. */
#define SEC()               \
    do {                    \
        LOCAL_SET_CARRY(1); \
        INC_PC(1);          \
    } while (0)

#define SED()                 \
    do {                      \
        LOCAL_SET_DECIMAL(1); \
        INC_PC(1);            \
    } while (0)

#define SEI()                      \
    do {                           \
        if (!LOCAL_INTERRUPT()) {  \
            OPCODE_DISABLES_IRQ(); \
        }                          \
        LOCAL_SET_INTERRUPT(1);    \
        INC_PC(1);                 \
    } while (0)

#define SHA_ABS_Y(addr)                                                                    \
    do {                                                                                   \
        unsigned int tmp;                                                                  \
                                                                                           \
        tmp = (addr);                                                                      \
        INC_PC(3);                                                                         \
        STORE_ABS_SH_Y(tmp, reg_a_read & reg_x_read & ((tmp >> 8) + 1), CLK_ABS_I_STORE2); \
    } while (0)

#define SHA_IND_Y(addr)                                     \
    do {                                                    \
        unsigned int tmp;                                   \
        uint8_t val;                                        \
                                                            \
        tmp = LOAD_ZERO_ADDR(addr);                         \
        CLK_ADD(CLK, 2);                                    \
        LOAD((tmp & 0xff00) | ((tmp + reg_y_read) & 0xff)); \
        CLK_ADD(CLK, CLK_IND_Y_W);                          \
        val = reg_a_read & reg_x_read & ((tmp >> 8) + 1);   \
        if ((tmp & 0xff) + reg_y_read > 0xff) {             \
            tmp = ((tmp + reg_y_read) & 0xff) | (val << 8); \
        } else {                                            \
            tmp += reg_y_read;                              \
        }                                                   \
        INC_PC(2);                                          \
        STORE(tmp, val);                                    \
    } while (0)

#define SHX_ABS_Y(addr)                                                       \
    do {                                                                      \
        unsigned int tmp;                                                     \
                                                                              \
        tmp = (addr);                                                         \
        INC_PC(3);                                                            \
        STORE_ABS_SH_Y(tmp, reg_x_read & ((tmp >> 8) + 1), CLK_ABS_I_STORE2); \
    } while (0)

#define SHY_ABS_X(addr)                                                       \
    do {                                                                      \
        unsigned int tmp;                                                     \
                                                                              \
        tmp = (addr);                                                         \
        INC_PC(3);                                                            \
        STORE_ABS_SH_X(tmp, reg_y_read & ((tmp >> 8) + 1), CLK_ABS_I_STORE2); \
    } while (0)

#define SHS_ABS_Y(addr)                                                                    \
    do {                                                                                   \
        int tmp = (addr);                                                                  \
                                                                                           \
        INC_PC(3);                                                                         \
        STORE_ABS_SH_Y(tmp, reg_a_read & reg_x_read & ((tmp >> 8) + 1), CLK_ABS_I_STORE2); \
        reg_sp = reg_a_read & reg_x_read;                                                  \
    } while (0)

#define SLO(addr, clk_inc1, pc_inc, load_func, store_func, dummy_func) \
    do {                                                             \
        uint8_t tmp, tmp2;                                           \
        int tmp_addr;                                                \
                                                                     \
        tmp_addr = (addr);                                           \
        CLK_ADD(CLK, (clk_inc1));                                    \
        tmp = load_func(tmp_addr);                                   \
        CLK_ADD(CLK, 1);                                             \
        CLK_ADD_DUMMY(CLK, 1);                                       \
        dummy_func(tmp_addr, tmp);                                   \
        LOCAL_SET_CARRY(tmp & 0x80);                                 \
        tmp <<= 1;                                                   \
        tmp2 = reg_a_read | tmp;                                     \
        reg_a_write(tmp2);                                           \
        LOCAL_SET_NZ(tmp2);                                          \
        INC_PC(pc_inc);                                              \
        store_func(tmp_addr, tmp, 1);                                \
    } while (0)

#define SLO_IND_Y(addr)                                                     \
    do {                                                                    \
        uint8_t tmp, tmp2;                                                  \
        unsigned int tmp_addr;                                              \
                                                                            \
        tmp_addr = LOAD_ZERO_ADDR(addr);                                    \
        CLK_ADD(CLK, 2);                                                    \
        LOAD_DUMMY((tmp_addr & 0xff00) | ((tmp_addr + reg_y_read) & 0xff)); \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        tmp_addr += reg_y_read;                                             \
        tmp = LOAD(tmp_addr);                                               \
        CLK_ADD(CLK, 1);                                                    \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        DUMMY_STORE_ABS_RMW(tmp_addr, tmp);                                 \
        LOCAL_SET_CARRY(tmp & 0x80);                                        \
        tmp <<= 1;                                                          \
        tmp2 = reg_a_read | tmp;                                            \
        reg_a_write(tmp2);                                                  \
        LOCAL_SET_NZ(tmp2);                                                 \
        INC_PC(2);                                                          \
        STORE_ABS(tmp_addr, tmp, 1);                                        \
    } while (0)

#define SRE(addr, clk_inc1, pc_inc, load_func, store_func, dummy_func) \
    do {                                                             \
        unsigned int tmp, tmp2;                                      \
        unsigned int tmp_addr;                                       \
                                                                     \
        tmp_addr = (addr);                                           \
        CLK_ADD(CLK, (clk_inc1));                                    \
        tmp = load_func(tmp_addr);                                   \
        CLK_ADD(CLK, 1);                                             \
        CLK_ADD_DUMMY(CLK, 1);                                       \
        dummy_func(tmp_addr, tmp);                                   \
        LOCAL_SET_CARRY(tmp & 0x01);                                 \
        tmp >>= 1;                                                   \
        tmp2 = reg_a_read ^ tmp;                                     \
        reg_a_write(tmp2);                                           \
        LOCAL_SET_NZ(tmp2);                                          \
        INC_PC(pc_inc);                                              \
        store_func(tmp_addr, tmp, 1);                                \
    } while (0)

#define SRE_IND_Y(addr)                                                     \
    do {                                                                    \
        unsigned int tmp, tmp2;                                             \
        unsigned int tmp_addr = LOAD_ZERO_ADDR(addr);                       \
                                                                            \
        CLK_ADD(CLK, 2);                                                    \
        LOAD_DUMMY((tmp_addr & 0xff00) | ((tmp_addr + reg_y_read) & 0xff)); \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        tmp_addr += reg_y_read;                                             \
        tmp = LOAD(tmp_addr);                                               \
        CLK_ADD(CLK, 1);                                                    \
        CLK_ADD_DUMMY(CLK, 1);                                              \
        DUMMY_STORE_ABS_RMW(tmp_addr, tmp);                                 \
        LOCAL_SET_CARRY(tmp & 0x01);                                        \
        tmp >>= 1;                                                          \
        tmp2 = reg_a_read ^ tmp;                                            \
        reg_a_write(tmp2);                                                  \
        LOCAL_SET_NZ(tmp2);                                                 \
        INC_PC(2);                                                          \
        STORE_ABS(tmp_addr, tmp, 1);                                        \
    } while (0)

#define STA(addr, clk_inc1, clk_inc2, pc_inc, store_func) \
    do {                                                  \
        unsigned int tmp;                                 \
                                                          \
        CLK_ADD(CLK, (clk_inc1));                         \
        tmp = (addr);                                     \
        INC_PC(pc_inc);                                   \
        store_func(tmp, reg_a_read, clk_inc2);            \
    } while (0)

#define STA_ZERO(addr, clk_inc, pc_inc) \
    do {                                \
        CLK_ADD(CLK, (clk_inc));        \
        STORE_ZERO((addr), reg_a_read); \
        INC_PC(pc_inc);                 \
    } while (0)

#define STA_IND_Y(addr)                                         \
    do {                                                        \
        unsigned int tmp;                                       \
                                                                \
        tmp = LOAD_ZERO_ADDR(addr);                             \
        CLK_ADD(CLK, 2);                                        \
        LOAD_DUMMY((tmp & 0xff00) | ((tmp + reg_y_read) & 0xff)); \
        CLK_ADD(CLK, CLK_IND_Y_W);                              \
        INC_PC(2);                                              \
        STORE_IND(tmp + reg_y_read, reg_a_read);                \
    } while (0)

#define STX(addr, clk_inc, pc_inc) \
    do {                           \
        unsigned int tmp;          \
                                   \
        tmp = (addr);              \
        CLK_ADD(CLK, (clk_inc));   \
        INC_PC(pc_inc);            \
        STORE(tmp, reg_x_read);    \
    } while (0)

#define STX_ZERO(addr, clk_inc, pc_inc) \
    do {                                \
        CLK_ADD(CLK, (clk_inc));        \
        STORE_ZERO((addr), reg_x_read); \
        INC_PC(pc_inc);                 \
    } while (0)

#define STY(addr, clk_inc, pc_inc) \
    do {                           \
        unsigned int tmp;          \
                                   \
        tmp = (addr);              \
        CLK_ADD(CLK, (clk_inc));   \
        INC_PC(pc_inc);            \
        STORE(tmp, reg_y_read);    \
    } while (0)

#define STY_ZERO(addr, clk_inc, pc_inc) \
    do {                                \
        CLK_ADD(CLK, (clk_inc));        \
        STORE_ZERO((addr), reg_y_read); \
        INC_PC(pc_inc);                 \
    } while (0)

#define TAX()                     \
    do {                          \
        reg_x_write(reg_a_read);  \
        LOCAL_SET_NZ(reg_x_read); \
        INC_PC(1);                \
    } while (0)

#define TAY()                     \
    do {                          \
        reg_y_write(reg_a_read);  \
        LOCAL_SET_NZ(reg_y_read); \
        INC_PC(1);                \
    } while (0)

#define TSX()                 \
    do {                      \
        reg_x_write(reg_sp);  \
        LOCAL_SET_NZ(reg_sp); \
        INC_PC(1);            \
    } while (0)

#define TXA()                     \
    do {                          \
        reg_a_write(reg_x_read);  \
        LOCAL_SET_NZ(reg_x_read); \
        INC_PC(1);                \
    } while (0)

#define TXS()                \
    do {                     \
        reg_sp = reg_x_read; \
        INC_PC(1);           \
    } while (0)

#define TYA()                     \
    do {                          \
        reg_a_write(reg_y_read);  \
        LOCAL_SET_NZ(reg_y_read); \
        INC_PC(1);                \
    } while (0)


/* ------------------------------------------------------------------------- */

static const uint8_t fetch_tab[] = {
            /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
    /* $00 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $00 */
    /* $10 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $10 */
    /* $20 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $20 */
    /* $30 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $30 */
    /* $40 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $40 */
    /* $50 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $50 */
    /* $60 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $60 */
    /* $70 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $70 */
    /* $80 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $80 */
    /* $90 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $90 */
    /* $A0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $A0 */
    /* $B0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $B0 */
    /* $C0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $C0 */
    /* $D0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, /* $D0 */
    /* $E0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, /* $E0 */
    /* $F0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1  /* $F0 */
};

#ifndef C64DTV  /* C64DTV opcode_t & fetch are defined in c64dtvcpu.c */

#ifdef CPU_8502  /* 8502 specific opcode fetch */

static const uint8_t rewind_fetch_tab[] = {
            /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
    /* $00 */  1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $00 */
    /* $10 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $10 */
    /* $20 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $20 */
    /* $30 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $30 */
    /* $40 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $40 */
    /* $50 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $50 */
    /* $60 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $60 */
    /* $70 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $70 */
    /* $80 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $80 */
    /* $90 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $90 */
    /* $A0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $A0 */
    /* $B0 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $B0 */
    /* $C0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $C0 */
    /* $D0 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $D0 */
    /* $E0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $E0 */
    /* $F0 */  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* $F0 */
};

#if !defined WORDS_BIGENDIAN && defined ALLOW_UNALIGNED_ACCESS

#define opcode_t uint32_t

#define FETCH_OPCODE(o)                                                \
    do {                                                               \
        if (((int)reg_pc) < bank_limit) {                              \
            o = (*((uint32_t *)(bank_base + reg_pc)) & 0xffffff);         \
            if (rewind_fetch_tab[o & 0xff]) {                          \
                opcode_cycle[0] = vicii_check_memory_refresh(CLK);     \
                CLK_ADD(CLK, 1);                                       \
                opcode_cycle[1] = vicii_check_memory_refresh(CLK);     \
                CLK_ADD(CLK, 1);                                       \
            } else {                                                   \
                opcode_cycle[0] = 0;                                   \
                opcode_cycle[1] = 0;                                   \
                CLK_ADD(CLK, 2);                                       \
            }                                                          \
            if (fetch_tab[o & 0xff]) {                                 \
                CLK_ADD(CLK, 1);                                       \
            }                                                          \
        } else {                                                       \
            maincpu_stretch = 0;                                       \
            o = LOAD(reg_pc);                                          \
            if (rewind_fetch_tab[o & 0xff]) {                          \
                opcode_cycle[0] = maincpu_stretch;                     \
                if (opcode_cycle[0] == 0) {                            \
                    opcode_cycle[0] = vicii_check_memory_refresh(CLK); \
                }                                                      \
                CLK_ADD(CLK, 1);                                       \
                maincpu_stretch = 0;                                   \
                o |= LOAD(reg_pc + 1) << 8;                            \
                opcode_cycle[1] = maincpu_stretch;                     \
                if (opcode_cycle[1] == 0) {                            \
                    opcode_cycle[1] = vicii_check_memory_refresh(CLK); \
                }                                                      \
                CLK_ADD(CLK, 1);                                       \
            } else {                                                   \
                CLK_ADD(CLK, 1);                                       \
                o |= LOAD(reg_pc + 1) << 8;                            \
                CLK_ADD(CLK, 1);                                       \
            }                                                          \
            if (fetch_tab[o & 0xff]) {                                 \
                o |= (LOAD(reg_pc + 2) << 16);                         \
                CLK_ADD(CLK, 1);                                       \
            }                                                          \
        }                                                              \
    } while (0)

#define p0 (opcode & 0xff)
#define p1 ((opcode >> 8) & 0xff)
#define p2 (opcode >> 8)

#else /* WORDS_BIGENDIAN || !ALLOW_UNALIGNED_ACCESS */

#define opcode_t         \
    struct {             \
        uint8_t ins;        \
        union {          \
            uint8_t op8[2]; \
            uint16_t op16;   \
        } op;            \
    }

#define FETCH_OPCODE(o)                                                                   \
    do {                                                                                  \
        if (((int)reg_pc) < bank_limit) {                                                 \
            (o).ins = *(bank_base + reg_pc);                                              \
            (o).op.op16 = (*(bank_base + reg_pc + 1) | (*(bank_base + reg_pc + 2) << 8)); \
            if (rewind_fetch_tab[(o).ins]) {                                              \
                opcode_cycle[0] = vicii_check_memory_refresh(CLK);                        \
                CLK_ADD(CLK, 1);                                                          \
                opcode_cycle[1] = vicii_check_memory_refresh(CLK);                        \
                CLK_ADD(CLK, 1);                                                          \
            } else {                                                                      \
                opcode_cycle[0] = 0;                                                      \
                opcode_cycle[1] = 0;                                                      \
                CLK_ADD(CLK, 2);                                                          \
            }                                                                             \
            if (fetch_tab[(o).ins]) {                                                     \
                CLK_ADD(CLK, 1);                                                          \
            }                                                                             \
        } else {                                                                          \
            maincpu_stretch = 0;                                                          \
            (o).ins = LOAD(reg_pc);                                                       \
            if (rewind_fetch_tab[(o).ins]) {                                              \
                opcode_cycle[0] = maincpu_stretch;                                        \
                if (opcode_cycle[0] == 0) {                                               \
                    opcode_cycle[0] = vicii_check_memory_refresh(CLK);                    \
                }                                                                         \
                CLK_ADD(CLK, 1);                                                          \
                maincpu_stretch = 0;                                                      \
                (o).op.op16 = LOAD(reg_pc + 1);                                           \
                opcode_cycle[1] = maincpu_stretch;                                        \
                if (opcode_cycle[1] == 0) {                                               \
                    opcode_cycle[1] = vicii_check_memory_refresh(CLK);                    \
                }                                                                         \
                CLK_ADD(CLK, 1);                                                          \
            } else {                                                                      \
                CLK_ADD(CLK, 1);                                                          \
                (o).op.op16 = LOAD(reg_pc + 1);                                           \
                CLK_ADD(CLK, 1);                                                          \
            }                                                                             \
            if (fetch_tab[(o).ins]) {                                                     \
                (o).op.op16 |= (LOAD(reg_pc + 2) << 8);                                   \
                CLK_ADD(CLK, 1);                                                          \
            }                                                                             \
        }                                                                                 \
    } while (0)

#define p0 (opcode.ins)
#define p2 (opcode.op.op16)

#ifdef WORDS_BIGENDIAN
#  define p1 (opcode.op.op8[1])
#else
#  define p1 (opcode.op.op8[0])
#endif

#endif /* !WORDS_BIGENDIAN */

#else /* !CPU_8502 */

#if !defined WORDS_BIGENDIAN && defined ALLOW_UNALIGNED_ACCESS

#define opcode_t uint32_t

#define FETCH_OPCODE(o)                                        \
    do {                                                       \
        if (((int)reg_pc) < bank_limit) {                      \
            o = (*((uint32_t *)(bank_base + reg_pc)) & 0xffffff); \
            CLK_ADD(CLK, 2);                                   \
            if (fetch_tab[o & 0xff]) {                         \
                CLK_ADD(CLK, 1);                               \
            }                                                  \
        } else {                                               \
            o = LOAD(reg_pc);                                  \
            CLK_ADD(CLK, 1);                                   \
            o |= LOAD(reg_pc + 1) << 8;                        \
            CLK_ADD(CLK, 1);                                   \
            if (fetch_tab[o & 0xff]) {                         \
                o |= (LOAD(reg_pc + 2) << 16);                 \
                CLK_ADD(CLK, 1);                               \
            }                                                  \
        }                                                      \
    } while (0)

#define p0 (opcode & 0xff)
#define p1 ((opcode >> 8) & 0xff)
#define p2 (opcode >> 8)

#else /* WORDS_BIGENDIAN || !ALLOW_UNALIGNED_ACCESS */

#define opcode_t         \
    struct {             \
        uint8_t ins;        \
        union {          \
            uint8_t op8[2]; \
            uint16_t op16;   \
        } op;            \
    }

#define FETCH_OPCODE(o)                                                                   \
    do {                                                                                  \
        if (((int)reg_pc) < bank_limit) {                                                 \
            (o).ins = *(bank_base + reg_pc);                                              \
            (o).op.op16 = (*(bank_base + reg_pc + 1) | (*(bank_base + reg_pc + 2) << 8)); \
            CLK_ADD(CLK, 2);                                                              \
            if (fetch_tab[(o).ins]) {                                                     \
                CLK_ADD(CLK, 1);                                                          \
            }                                                                             \
        } else {                                                                          \
            (o).ins = LOAD(reg_pc);                                                       \
            CLK_ADD(CLK, 1);                                                              \
            (o).op.op16 = LOAD(reg_pc + 1);                                               \
            CLK_ADD(CLK, 1);                                                              \
            if (fetch_tab[(o).ins]) {                                                     \
                (o).op.op16 |= (LOAD(reg_pc + 2) << 8);                                   \
                CLK_ADD(CLK, 1);                                                          \
            }                                                                             \
        }                                                                                 \
    } while (0)

#define p0 (opcode.ins)
#define p2 (opcode.op.op16)

#ifdef WORDS_BIGENDIAN
#  define p1 (opcode.op.op8[1])
#else
#  define p1 (opcode.op.op8[0])
#endif

#endif /* !WORDS_BIGENDIAN */
#endif

/*  SET_OPCODE for traps */
#if !defined WORDS_BIGENDIAN && defined ALLOW_UNALIGNED_ACCESS
#define SET_OPCODE(o) (opcode) = o
#else
#if !defined WORDS_BIGENDIAN
#define SET_OPCODE(o)                          \
    do {                                       \
        opcode.ins = (o) & 0xff;               \
        opcode.op.op8[0] = ((o) >> 8) & 0xff;  \
        opcode.op.op8[1] = ((o) >> 16) & 0xff; \
    } while (0)
#else
#define SET_OPCODE(o)                          \
    do {                                       \
        opcode.ins = (o) & 0xff;               \
        opcode.op.op8[1] = ((o) >> 8) & 0xff;  \
        opcode.op.op8[0] = ((o) >> 16) & 0xff; \
    } while (0)
#endif

#endif

#endif /* !C64DTV */

/* ------------------------------------------------------------------------ */

/* Here, the CPU is emulated. */

{
    static int cpu_is_jammed = 0;
    unsigned int tmpa; /* needed for some of the opcode macros */

    /* handle 8502 fast mode refresh cycles */
    CPU_REFRESH_CLK

    /* handle any extra cpu switches */
#ifdef CHECK_AND_RUN_ALTERNATE_CPU
    CHECK_AND_RUN_ALTERNATE_CPU
#endif

    CPU_DELAY_CLK

    PROCESS_ALARMS

    /* HACK: when the CPU is jammed, no interrupts are served, the only way
       to recover is reset. so we clear the interrupt flags and force
       acknowledging them here in this case. */
    if (cpu_is_jammed) {
        interrupt_ack_irq(CPU_INT_STATUS);
        CPU_INT_STATUS->global_pending_int &= ~(IK_IRQ | IK_NMI);
        if (CPU_INT_STATUS->global_pending_int & IK_RESET) {
            cpu_is_jammed = 0;
        }
    }

    {
        enum cpu_int pending_interrupt;

        if (!(CPU_INT_STATUS->global_pending_int & IK_IRQ)
            && (CPU_INT_STATUS->global_pending_int & IK_IRQPEND)
            && CPU_INT_STATUS->irq_pending_clk <= CLK) {
            interrupt_ack_irq(CPU_INT_STATUS);
        }

        pending_interrupt = CPU_INT_STATUS->global_pending_int;
        if (pending_interrupt != IK_NONE) {
            DO_INTERRUPT(pending_interrupt);
            if (!(CPU_INT_STATUS->global_pending_int & IK_IRQ)
                && CPU_INT_STATUS->global_pending_int & IK_IRQPEND) {
                CPU_INT_STATUS->global_pending_int &= ~IK_IRQPEND;
            }
            CPU_DELAY_CLK

            PROCESS_ALARMS
        }
    }

    {
        opcode_t opcode;
#ifdef DEBUG
        CLOCK debug_clk;
#ifdef DRIVE_CPU
        debug_clk = CLK;
#else
        debug_clk = maincpu_clk;
#endif
#endif

#ifdef FEATURE_CPUMEMHISTORY
        CLOCK history_clk;
#ifndef DRIVE_CPU
        history_clk = maincpu_clk;
        memmap_state |= (MEMMAP_STATE_INSTR | MEMMAP_STATE_OPCODE);
#else
        history_clk = CLK;
#endif
#endif
        SET_LAST_ADDR(reg_pc);

        FETCH_OPCODE(opcode);

#ifdef FEATURE_CPUMEMHISTORY
#ifndef DRIVE_CPU
#ifndef C64DTV
        /* HACK to cope with FETCH_OPCODE optimization in x64 */
        if (((int)reg_pc) < bank_limit) {
            memmap_mark_read(reg_pc);
        }
#endif
#endif
        /* If reg_pc >= bank_limit  then JSR (0x20) hasn't load p2 yet.
           The earlier LOAD(reg_pc+2) hack can break stealing badly.
           The fixing is now handled in JSR(). */
        monitor_cpuhistory_store(history_clk, reg_pc, p0, p1, p2 >> 8, reg_a_read, reg_x_read, reg_y_read, reg_sp, LOCAL_STATUS(), origin);
#ifndef DRIVE_CPU
        memmap_state &= ~(MEMMAP_STATE_INSTR | MEMMAP_STATE_OPCODE);
#endif
#endif

#ifdef DEBUG
#ifdef DRIVE_CPU
        if (TRACEFLG) {
            uint8_t op = (uint8_t)(p0);
            uint8_t lo = (uint8_t)(p1);
            uint8_t hi = (uint8_t)(p2 >> 8);

            debug_drive((uint32_t)(reg_pc), debug_clk,
                        mon_disassemble_to_string(e_disk8_space,
                                                  reg_pc, op,
                                                  lo, hi, 0, 1, "6502"),
                        reg_a_read, reg_x_read, reg_y_read, reg_sp, drv->mynumber + 8);
        }
#else
        if (TRACEFLG) {
            uint8_t op = (uint8_t)(p0);
            uint8_t lo = (uint8_t)(p1);
            uint8_t hi = (uint8_t)(p2 >> 8);

            if (op == 0x20) {
                hi = LOAD(reg_pc + 2);  /* FIXME: this triggers watchpoints eventually */
            }

            debug_maincpu((uint32_t)(reg_pc), debug_clk,
                          mon_disassemble_to_string(e_comp_space,
                                                    reg_pc, op,
                                                    lo, hi, 0, 1, "6502"),
                          reg_a_read, reg_x_read, reg_y_read, reg_sp);
        }
        if (debug.perform_break_into_monitor) {
            monitor_startup_trap();
            debug.perform_break_into_monitor = 0;
        }
#endif
#endif

trap_skipped:
        SET_LAST_OPCODE(p0);

        switch (p0) {
            case 0x00:          /* BRK */
                BRK();
                break;

            case 0x01:          /* ORA ($nn,X) */
                ORA(LOAD_IND_X(p1), 1, 2);
                break;

            case 0x02:          /* JAM - also used for traps */
                STATIC_ASSERT(TRAP_OPCODE == 0x02);
                JAM_02();
                break;

            case 0x22:          /* JAM */
            case 0x52:          /* JAM */
            case 0x62:          /* JAM */
            case 0x72:          /* JAM */
            case 0x92:          /* JAM */
            case 0xb2:          /* JAM */
            case 0xd2:          /* JAM */
            case 0xf2:          /* JAM */
#ifndef C64DTV
            case 0x12:          /* JAM */
            case 0x32:          /* JAM */
            case 0x42:          /* JAM */
#endif
                cpu_is_jammed = 1;
                REWIND_FETCH_OPCODE(CLK);
                JAM();
                break;

#ifdef C64DTV
            /* These opcodes are defined in c64/c64dtvcpu.c */
            case 0x12:          /* BRA */
                BRANCH(1, p1);
                break;

            case 0x32:          /* SAC */
                SAC(p1);
                break;

            case 0x42:          /* SIR */
                SIR(p1);
                break;
#endif

            case 0x03:          /* SLO ($nn,X) */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                SLO(LOAD_ZERO_ADDR(p1 + reg_x_read), 2, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x04:          /* NOOP $nn */
            case 0x44:          /* NOOP $nn */
            case 0x64:          /* NOOP $nn */
                NOOP(1, 2);
                break;

            case 0x05:          /* ORA $nn */
                ORA(LOAD_ZERO(p1), 1, 2);
                break;

            case 0x06:          /* ASL $nn */
                ASL(p1, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x07:          /* SLO $nn */
                SLO(p1, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x08:          /* PHP */
#ifdef DRIVE_CPU
                drivecpu_rotate();
                if (drivecpu_byte_ready()) {
                    drivecpu_byte_ready_egde_clear();
                    LOCAL_SET_OVERFLOW(1);
                }
#endif
                PHP();
                break;

            case 0x09:          /* ORA #$nn */
                ORA(p1, 0, 2);
                break;

            case 0x0a:          /* ASL A */
                ASL_A();
                break;

            case 0x0b:          /* ANC #$nn */
            case 0x2b:          /* ANC #$nn */
                ANC(p1, 2);
                break;

            case 0x0c:          /* NOOP $nnnn */
                NOOP_ABS();
                break;

            case 0x0d:          /* ORA $nnnn */
                ORA(LOAD(p2), 1, 3);
                break;

            case 0x0e:          /* ASL $nnnn */
                ASL(p2, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x0f:          /* SLO $nnnn */
                SLO(p2, 0, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x10:          /* BPL $nnnn */
                BRANCH(!LOCAL_SIGN(), p1);
                break;

            case 0x11:          /* ORA ($nn),Y */
                ORA(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0x13:          /* SLO ($nn),Y */
                SLO_IND_Y(p1);
                break;

            case 0x14:          /* NOOP $nn,X */
            case 0x34:          /* NOOP $nn,X */
            case 0x54:          /* NOOP $nn,X */
            case 0x74:          /* NOOP $nn,X */
            case 0xd4:          /* NOOP $nn,X */
            case 0xf4:          /* NOOP $nn,X */
                NOOP((NOOP_LOAD_ZERO_X(p1), CLK_NOOP_ZERO_X), 2);
                break;

            case 0x15:          /* ORA $nn,X */
                ORA(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0x16:          /* ASL $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                ASL((p1 + reg_x_read) & 0xff, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x17:          /* SLO $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                SLO((p1 + reg_x_read) & 0xff, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x18:          /* CLC */
                CLC();
                break;

            case 0x19:          /* ORA $nnnn,Y */
                ORA(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0x1a:          /* NOOP */
            case 0x3a:          /* NOOP */
            case 0x5a:          /* NOOP */
            case 0x7a:          /* NOOP */
            case 0xda:          /* NOOP */
            case 0xfa:          /* NOOP */
                NOOP_IMM(1);
                break;

            case 0x1b:          /* SLO $nnnn,Y */
                SLO(p2, 0, 3, LOAD_ABS_Y_RMW, STORE_ABS_Y_RMW, DUMMY_STORE_ABS_Y_RMW);
                break;

            case 0x1c:          /* NOOP $nnnn,X */
            case 0x3c:          /* NOOP $nnnn,X */
            case 0x5c:          /* NOOP $nnnn,X */
            case 0x7c:          /* NOOP $nnnn,X */
            case 0xdc:          /* NOOP $nnnn,X */
            case 0xfc:          /* NOOP $nnnn,X */
                NOOP_ABS_X();
                break;

            case 0x1d:          /* ORA $nnnn,X */
                ORA(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0x1e:          /* ASL $nnnn,X */
                ASL(p2, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x1f:          /* SLO $nnnn,X */
                SLO(p2, 0, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x20:          /* JSR $nnnn */
                JSR();
                break;

            case 0x21:          /* AND ($nn,X) */
                AND(LOAD_IND_X(p1), 1, 2);
                break;

            case 0x23:          /* RLA ($nn,X) */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                RLA(LOAD_ZERO_ADDR(p1 + reg_x_read), 2, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x24:          /* BIT $nn */
                BIT(LOAD_ZERO(p1), 2);
                break;

            case 0x25:          /* AND $nn */
                AND(LOAD_ZERO(p1), 1, 2);
                break;

            case 0x26:          /* ROL $nn */
                ROL(p1, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x27:          /* RLA $nn */
                RLA(p1, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x28:          /* PLP */
                PLP();
                break;

            case 0x29:          /* AND #$nn */
                AND(p1, 0, 2);
                break;

            case 0x2a:          /* ROL A */
                ROL_A();
                break;

            case 0x2c:          /* BIT $nnnn */
                BIT(LOAD(p2), 3);
                break;

            case 0x2d:          /* AND $nnnn */
                AND(LOAD(p2), 1, 3);
                break;

            case 0x2e:          /* ROL $nnnn */
                ROL(p2, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x2f:          /* RLA $nnnn */
                RLA(p2, 0, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x30:          /* BMI $nnnn */
                BRANCH(LOCAL_SIGN(), p1);
                break;

            case 0x31:          /* AND ($nn),Y */
                AND(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0x33:          /* RLA ($nn),Y */
                RLA_IND_Y(p1);
                break;

            case 0x35:          /* AND $nn,X */
                AND(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0x36:          /* ROL $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                ROL((p1 + reg_x_read) & 0xff, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x37:          /* RLA $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                RLA((p1 + reg_x_read) & 0xff, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x38:          /* SEC */
                SEC();
                break;

            case 0x39:          /* AND $nnnn,Y */
                AND(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0x3b:          /* RLA $nnnn,Y */
                RLA(p2, 0, 3, LOAD_ABS_Y_RMW, STORE_ABS_Y_RMW, DUMMY_STORE_ABS_Y_RMW);
                break;

            case 0x3d:          /* AND $nnnn,X */
                AND(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0x3e:          /* ROL $nnnn,X */
                ROL(p2, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x3f:          /* RLA $nnnn,X */
                RLA(p2, 0, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x40:          /* RTI */
                RTI();
                break;

            case 0x41:          /* EOR ($nn,X) */
                EOR(LOAD_IND_X(p1), 1, 2);
                break;

            case 0x43:          /* SRE ($nn,X) */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                SRE(LOAD_ZERO_ADDR(p1 + reg_x_read), 2, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x45:          /* EOR $nn */
                EOR(LOAD_ZERO(p1), 1, 2);
                break;

            case 0x46:          /* LSR $nn */
                LSR(p1, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x47:          /* SRE $nn */
                SRE(p1, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x48:          /* PHA */
                PHA();
                break;

            case 0x49:          /* EOR #$nn */
                EOR(p1, 0, 2);
                break;

            case 0x4a:          /* LSR A */
                LSR_A();
                break;

            case 0x4b:          /* ASR #$nn */
                ASR(p1, 2);
                break;

            case 0x4c:          /* JMP $nnnn */
                JMP(p2);
                break;

            case 0x4d:          /* EOR $nnnn */
                EOR(LOAD(p2), 1, 3);
                break;

            case 0x4e:          /* LSR $nnnn */
                LSR(p2, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x4f:          /* SRE $nnnn */
                SRE(p2, 0, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x50:          /* BVC $nnnn */
#ifdef DRIVE_CPU
                CLK_ADD(CLK, -1);
                drivecpu_rotate();
                if (drivecpu_byte_ready()) {
                    drivecpu_byte_ready_egde_clear();
                    LOCAL_SET_OVERFLOW(1);
                }
                CLK_ADD(CLK, 1);
#endif
                BRANCH(!LOCAL_OVERFLOW(), p1);
                break;

            case 0x51:          /* EOR ($nn),Y */
                EOR(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0x53:          /* SRE ($nn),Y */
                SRE_IND_Y(p1);
                break;

            case 0x55:          /* EOR $nn,X */
                EOR(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0x56:          /* LSR $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                LSR((p1 + reg_x_read) & 0xff, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x57:          /* SRE $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                SRE((p1 + reg_x_read) & 0xff, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x58:          /* CLI */
                CLI();
                break;

            case 0x59:          /* EOR $nnnn,Y */
                EOR(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0x5b:          /* SRE $nnnn,Y */
                SRE(p2, 0, 3, LOAD_ABS_Y_RMW, STORE_ABS_Y_RMW, DUMMY_STORE_ABS_Y_RMW);
                break;

            case 0x5d:          /* EOR $nnnn,X */
                EOR(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0x5e:          /* LSR $nnnn,X */
                LSR(p2, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x5f:          /* SRE $nnnn,X */
                SRE(p2, 0, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x60:          /* RTS */
                RTS();
                break;

            case 0x61:          /* ADC ($nn,X) */
                ADC(LOAD_IND_X(p1), 1, 2);
                break;

            case 0x63:          /* RRA ($nn,X) */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                RRA(LOAD_ZERO_ADDR(p1 + reg_x_read), 2, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x65:          /* ADC $nn */
                ADC(LOAD_ZERO(p1), 1, 2);
                break;

            case 0x66:          /* ROR $nn */
                ROR(p1, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x67:          /* RRA $nn */
                RRA(p1, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x68:          /* PLA */
                PLA();
                break;

            case 0x69:          /* ADC #$nn */
                ADC(p1, 0, 2);
                break;

            case 0x6a:          /* ROR A */
                ROR_A();
                break;

            case 0x6b:          /* ARR #$nn */
                ARR(p1, 2);
                break;

            case 0x6c:          /* JMP ($nnnn) */
                JMP_IND();
                break;

            case 0x6d:          /* ADC $nnnn */
                ADC(LOAD(p2), 1, 3);
                break;

            case 0x6e:          /* ROR $nnnn */
                ROR(p2, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x6f:          /* RRA $nnnn */
                RRA(p2, 0, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x70:          /* BVS $nnnn */
#ifdef DRIVE_CPU
                CLK_ADD(CLK, -1);
                drivecpu_rotate();
                if (drivecpu_byte_ready()) {
                    drivecpu_byte_ready_egde_clear();
                    LOCAL_SET_OVERFLOW(1);
                }
                CLK_ADD(CLK, 1);
#endif
                BRANCH(LOCAL_OVERFLOW(), p1);
                break;

            case 0x71:          /* ADC ($nn),Y */
                ADC(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0x73:          /* RRA ($nn),Y */
                RRA_IND_Y(p1);
                break;

            case 0x75:          /* ADC $nn,X */
                ADC(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0x76:          /* ROR $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                ROR((p1 + reg_x_read) & 0xff, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x77:          /* RRA $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                RRA((p1 + reg_x_read) & 0xff, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0x78:          /* SEI */
                SEI();
                break;

            case 0x79:          /* ADC $nnnn,Y */
                ADC(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0x7b:          /* RRA $nnnn,Y */
                RRA(p2, 0, 3, LOAD_ABS_Y_RMW, STORE_ABS_Y_RMW, DUMMY_STORE_ABS_Y_RMW);
                break;

            case 0x7d:          /* ADC $nnnn,X */
                ADC(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0x7e:          /* ROR $nnnn,X */
                ROR(p2, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x7f:          /* RRA $nnnn,X */
                RRA(p2, 0, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0x80:          /* NOOP #$nn */
            case 0x82:          /* NOOP #$nn */
            case 0x89:          /* NOOP #$nn */
            case 0xc2:          /* NOOP #$nn */
            case 0xe2:          /* NOOP #$nn */
                NOOP_IMM(2);
                break;

            case 0x81:          /* STA ($nn,X) */
                STA((LOAD_ZERO_DUMMY(p1), LOAD_ZERO_ADDR(p1 + reg_x_read)), 3, 1, 2, STORE_ABS);
                break;

            case 0x83:          /* SAX ($nn,X) */
                SAX((LOAD_ZERO_DUMMY(p1), LOAD_ZERO_ADDR(p1 + reg_x_read)), 3, 1, 2);
                break;

            case 0x84:          /* STY $nn */
                STY_ZERO(p1, 1, 2);
                break;

            case 0x85:          /* STA $nn */
                STA_ZERO(p1, 1, 2);
                break;

            case 0x86:          /* STX $nn */
                STX_ZERO(p1, 1, 2);
                break;

            case 0x87:          /* SAX $nn */
                SAX_ZERO(p1, 1, 2);
                break;

            case 0x88:          /* DEY */
                DEY();
                break;

            case 0x8a:          /* TXA */
                TXA();
                break;

            case 0x8b:          /* ANE #$nn */
                ANE(p1, 2);
                break;

            case 0x8c:          /* STY $nnnn */
                STY(p2, 1, 3);
                break;

            case 0x8d:          /* STA $nnnn */
                STA(p2, 0, 1, 3, STORE_ABS);
                break;

            case 0x8e:          /* STX $nnnn */
                STX(p2, 1, 3);
                break;

            case 0x8f:          /* SAX $nnnn */
                SAX(p2, 0, 1, 3);
                break;

            case 0x90:          /* BCC $nnnn */
                BRANCH(!LOCAL_CARRY(), p1);
                break;

            case 0x91:          /* STA ($nn),Y */
                STA_IND_Y(p1);
                break;

            case 0x93:          /* SHA ($nn),Y */
                SHA_IND_Y(p1);
                break;

            case 0x94:          /* STY $nn,X */
                STY_ZERO((LOAD_ZERO_DUMMY(p1), p1 + reg_x_read), CLK_ZERO_I_STORE, 2);
                break;

            case 0x95:          /* STA $nn,X */
                STA_ZERO((LOAD_ZERO_DUMMY(p1), p1 + reg_x_read), CLK_ZERO_I_STORE, 2);
                break;

            case 0x96:          /* STX $nn,Y */
                STX_ZERO((LOAD_ZERO_DUMMY(p1), p1 + reg_y_read), CLK_ZERO_I_STORE, 2);
                break;

            case 0x97:          /* SAX $nn,Y */
                SAX((LOAD_ZERO_DUMMY(p1), (p1 + reg_y_read) & 0xff), 0, CLK_ZERO_I_STORE, 2);
                break;

            case 0x98:          /* TYA */
                TYA();
                break;

            case 0x99:          /* STA $nnnn,Y */
                STA(p2, 0, CLK_ABS_I_STORE2, 3, STORE_ABS_Y);
                break;

            case 0x9a:          /* TXS */
                TXS();
                break;

            case 0x9b:          /* SHS $nnnn,Y */
#ifdef C64DTV
                NOOP_ABS_Y();
#else
                SHS_ABS_Y(p2);
#endif
                break;

            case 0x9c:          /* SHY $nnnn,X */
                SHY_ABS_X(p2);
                break;

            case 0x9d:          /* STA $nnnn,X */
                STA(p2, 0, CLK_ABS_I_STORE2, 3, STORE_ABS_X);
                break;

            case 0x9e:          /* SHX $nnnn,Y */
                SHX_ABS_Y(p2);
                break;

            case 0x9f:          /* SHA $nnnn,Y */
                SHA_ABS_Y(p2);
                break;

            case 0xa0:          /* LDY #$nn */
                LDY(p1, 0, 2);
                break;

            case 0xa1:          /* LDA ($nn,X) */
                LDA(LOAD_IND_X(p1), 1, 2);
                break;

            case 0xa2:          /* LDX #$nn */
                LDX(p1, 0, 2);
                break;

            case 0xa3:          /* LAX ($nn,X) */
                LAX(LOAD_IND_X(p1), 1, 2);
                break;

            case 0xa4:          /* LDY $nn */
                LDY(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xa5:          /* LDA $nn */
                LDA(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xa6:          /* LDX $nn */
                LDX(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xa7:          /* LAX $nn */
                LAX(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xa8:          /* TAY */
                TAY();
                break;

            case 0xa9:          /* LDA #$nn */
                LDA(p1, 0, 2);
                break;

            case 0xaa:          /* TAX */
                TAX();
                break;

            case 0xab:          /* LXA #$nn */
                LXA(p1, 2);
                break;

            case 0xac:          /* LDY $nnnn */
                LDY(LOAD(p2), 1, 3);
                break;

            case 0xad:          /* LDA $nnnn */
                LDA(LOAD(p2), 1, 3);
                break;

            case 0xae:          /* LDX $nnnn */
                LDX(LOAD(p2), 1, 3);
                break;

            case 0xaf:          /* LAX $nnnn */
                LAX(LOAD(p2), 1, 3);
                break;

            case 0xb0:          /* BCS $nnnn */
                BRANCH(LOCAL_CARRY(), p1);
                break;

            case 0xb1:          /* LDA ($nn),Y */
                LDA(LOAD_IND_Y_BANK(p1), 1, 2);
                break;

            case 0xb3:          /* LAX ($nn),Y */
                LAX(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0xb4:          /* LDY $nn,X */
                LDY(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0xb5:          /* LDA $nn,X */
                LDA(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0xb6:          /* LDX $nn,Y */
                LDX(LOAD_ZERO_Y(p1), CLK_ZERO_I2, 2);
                break;

            case 0xb7:          /* LAX $nn,Y */
                LAX(LOAD_ZERO_Y(p1), CLK_ZERO_I2, 2);
                break;

            case 0xb8:          /* CLV */
                CLV();
                break;

            case 0xb9:          /* LDA $nnnn,Y */
                LDA(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0xba:          /* TSX */
                TSX();
                break;

            case 0xbb:          /* LAS $nnnn,Y */
                LAS(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0xbc:          /* LDY $nnnn,X */
                LDY(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0xbd:          /* LDA $nnnn,X */
                LDA(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0xbe:          /* LDX $nnnn,Y */
                LDX(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0xbf:          /* LAX $nnnn,Y */
                LAX(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0xc0:          /* CPY #$nn */
                CPY(p1, 0, 2);
                break;

            case 0xc1:          /* CMP ($nn,X) */
                CMP(LOAD_IND_X(p1), 1, 2);
                break;

            case 0xc3:          /* DCP ($nn,X) */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                DCP(LOAD_ZERO_ADDR(p1 + reg_x_read), 2, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xc4:          /* CPY $nn */
                CPY(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xc5:          /* CMP $nn */
                CMP(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xc6:          /* DEC $nn */
                DEC(p1, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xc7:          /* DCP $nn */
                DCP(p1, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xc8:          /* INY */
                INY();
                break;

            case 0xc9:          /* CMP #$nn */
                CMP(p1, 0, 2);
                break;

            case 0xca:          /* DEX */
                DEX();
                break;

            case 0xcb:          /* SBX #$nn */
                SBX(p1, 2);
                break;

            case 0xcc:          /* CPY $nnnn */
                CPY(LOAD(p2), 1, 3);
                break;

            case 0xcd:          /* CMP $nnnn */
                CMP(LOAD(p2), 1, 3);
                break;

            case 0xce:          /* DEC $nnnn */
                DEC(p2, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xcf:          /* DCP $nnnn */
                DCP(p2, 0, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xd0:          /* BNE $nnnn */
                BRANCH(!LOCAL_ZERO(), p1);
                break;

            case 0xd1:          /* CMP ($nn),Y */
                CMP(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0xd3:          /* DCP ($nn),Y */
                DCP_IND_Y(p1);
                break;

            case 0xd5:          /* CMP $nn,X */
                CMP(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0xd6:          /* DEC $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                DEC((p1 + reg_x_read) & 0xff, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xd7:          /* DCP $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                DCP((p1 + reg_x_read) & 0xff, 0, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xd8:          /* CLD */
                CLD();
                break;

            case 0xd9:          /* CMP $nnnn,Y */
                CMP(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0xdb:          /* DCP $nnnn,Y */
                DCP(p2, 0, 3, LOAD_ABS_Y_RMW, STORE_ABS_Y_RMW, DUMMY_STORE_ABS_Y_RMW);
                break;

            case 0xdd:          /* CMP $nnnn,X */
                CMP(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0xde:          /* DEC $nnnn,X */
                DEC(p2, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0xdf:          /* DCP $nnnn,X */
                DCP(p2, 0, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0xe0:          /* CPX #$nn */
                CPX(p1, 0, 2);
                break;

            case 0xe1:          /* SBC ($nn,X) */
                SBC(LOAD_IND_X(p1), 1, 2);
                break;

            case 0xe3:          /* ISB ($nn,X) */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                ISB(LOAD_ZERO_ADDR(p1 + reg_x_read), 2, 2, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xe4:          /* CPX $nn */
                CPX(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xe5:          /* SBC $nn */
                SBC(LOAD_ZERO(p1), 1, 2);
                break;

            case 0xe6:          /* INC $nn */
                INC(p1, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xe7:          /* ISB $nn */
                ISB(p1, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xe8:          /* INX */
                INX();
                break;

            case 0xe9:          /* SBC #$nn */
                SBC(p1, 0, 2);
                break;

            case 0xea:          /* NOP */
                NOP();
                break;

            case 0xeb:          /* USBC #$nn (same as SBC) */
                SBC(p1, 0, 2);
                break;

            case 0xec:          /* CPX $nnnn */
                CPX(LOAD(p2), 1, 3);
                break;

            case 0xed:          /* SBC $nnnn */
                SBC(LOAD(p2), 1, 3);
                break;

            case 0xee:          /* INC $nnnn */
                INC(p2, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xef:          /* ISB $nnnn */
                ISB(p2, 0, 3, LOAD_ABS, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xf0:          /* BEQ $nnnn */
                BRANCH(LOCAL_ZERO(), p1);
                break;

            case 0xf1:          /* SBC ($nn),Y */
                SBC(LOAD_IND_Y(p1), 1, 2);
                break;

            case 0xf3:          /* ISB ($nn),Y */
                ISB_IND_Y(p1);
                break;

            case 0xf5:          /* SBC $nn,X */
                SBC(LOAD_ZERO_X(p1), CLK_ZERO_I2, 2);
                break;

            case 0xf6:          /* INC $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                INC((p1 + reg_x_read) & 0xff, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xf7:          /* ISB $nn,X */
                LOAD_ZERO_DUMMY(p1);
                CLK_ADD_DUMMY(CLK, 1);
                ISB((p1 + reg_x_read) & 0xff, 0, 2, LOAD_ZERO, STORE_ABS, DUMMY_STORE_ABS_RMW);
                break;

            case 0xf8:          /* SED */
                SED();
                break;

            case 0xf9:          /* SBC $nnnn,Y */
                SBC(LOAD_ABS_Y(p2), 1, 3);
                break;

            case 0xfb:          /* ISB $nnnn,Y */
                ISB(p2, 0, 3, LOAD_ABS_Y_RMW, STORE_ABS_Y_RMW, DUMMY_STORE_ABS_Y_RMW);
                break;

            case 0xfd:          /* SBC $nnnn,X */
                SBC(LOAD_ABS_X(p2), 1, 3);
                break;

            case 0xfe:          /* INC $nnnn,X */
                INC(p2, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;

            case 0xff:          /* ISB $nnnn,X */
                ISB(p2, 0, 3, LOAD_ABS_X_RMW, STORE_ABS_X_RMW, DUMMY_STORE_ABS_X_RMW);
                break;
        }
    }
}
