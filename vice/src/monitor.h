/*
 * monitor.h - The VICE built-in monitor, external interface.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Daniel Sladic <sladic@eecg.toronto.edu>
 *  Ettore Perazzoli <ettore@comm2000.it>
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

#ifndef VICE_MONITOR_H
#define VICE_MONITOR_H

#include "types.h"
#include "monitor/asm.h"

/** Generic interface.  **/
#define NUM_MEMSPACES e_invalid_space

enum mon_int {
    MI_NONE = 0,
    MI_BREAK = 1 << 0,
    MI_WATCH = 1 << 1,
    MI_STEP = 1 << 2
};

enum t_memspace {
    e_default_space = 0,
    e_comp_space,
    e_disk8_space,
    e_disk9_space,
    e_disk10_space,
    e_disk11_space,
    e_invalid_space
};
typedef enum t_memspace MEMSPACE;

#define FIRST_SPACE e_comp_space
#define LAST_SPACE e_disk11_space

enum CPU_TYPE_s {
    CPU_6502,
    CPU_WDC65C02,
    CPU_R65C02,
    CPU_65SC02,
    CPU_65816,
    CPU_Z80,
    CPU_6502DTV,
    CPU_6809
};
typedef enum CPU_TYPE_s CPU_TYPE_t;

struct interrupt_cpu_status_s;

struct monitor_cpu_type_s {
    CPU_TYPE_t cpu_type;
    unsigned int (*asm_addr_mode_get_size)(unsigned int mode, unsigned int p0,
                                           unsigned int p1, unsigned int p2, unsigned int p3);
    const struct asm_opcode_info_s *(*asm_opcode_info_get)(unsigned int p0, unsigned int p1,
                                                           unsigned int p2, unsigned int p3);
    int (*mon_assemble_instr)(const char *opcode_name, asm_mode_addr_info_t operand);
    unsigned int (*mon_register_get_val)(int mem, int reg_id);
    void (*mon_register_set_val)(int mem, int reg_id, uint16_t val);
    void (*mon_register_print)(int mem);
    const char* (*mon_register_print_ex)(int mem);
    struct mon_reg_list_s *(*mon_register_list_get)(int mem);
};
typedef struct monitor_cpu_type_s monitor_cpu_type_t;


struct supported_cpu_type_list_s {
    monitor_cpu_type_t *monitor_cpu_type_p;
    struct supported_cpu_type_list_s *next;
};
typedef struct supported_cpu_type_list_s supported_cpu_type_list_t;

/* A linked list of supported monitor_cpu_types for each memspace */
extern supported_cpu_type_list_t *monitor_cpu_type_supported[NUM_MEMSPACES];

/* This is the standard interface through which the monitor accesses a
   certain CPU.  */
struct monitor_interface_s {
    /* Pointer to the registers of the 6502 CPU.  */
    struct mos6510_regs_s *cpu_regs;

    /* Pointer to the registers of the R65C02 CPU. */
    struct R65C02_regs_s *cpu_R65C02_regs;

    /* Pointer to the registers of the 65816/65802 CPU. */
    struct WDC65816_regs_s *cpu_65816_regs;

    /* Pointer to the registers of the Z80 CPU.  */
    struct z80_regs_s *z80_cpu_regs;

    /* Pointer to the registers of the DTV CPU.  */
    struct mos6510dtv_regs_s *dtv_cpu_regs;

    /* Pointer to the registers of the DTV CPU.  */
    struct h6809_regs_s *h6809_cpu_regs;

    /* Pointer to the alarm/interrupt status.  */
    struct interrupt_cpu_status_s *int_status;

    /* Pointer to the machine's clock counter.  */
    CLOCK *clk;

    int current_bank;
    int current_bank_index;

    /* Returns the string bank identifiers in a NULL terminated list. */
    const char **(*mem_bank_list)(void);

    /* Returns the numeric bank identifiers in a -1 terminated list.
     * These correspond to the string identifiers so there could be
     * repeats.
     */
    const int *(*mem_bank_list_nos)(void);

    int (*mem_bank_from_name)(const char *name);
    int (*mem_bank_index_from_bank)(int bank);
    int (*mem_bank_flags_from_bank)(int bank);

    uint8_t (*mem_bank_read)(int bank, uint16_t addr, void *context);
    uint8_t (*mem_bank_peek)(int bank, uint16_t addr, void *context);
    void (*mem_bank_write)(int bank, uint16_t addr, uint8_t byte, void *context);
    void (*mem_bank_poke)(int bank, uint16_t addr, uint8_t byte, void *context);

    struct mem_ioreg_list_s *(*mem_ioreg_list_get)(void *context);

    /* Pointer to a function to disable/enable watchpoint checking.  */
    void (*toggle_watchpoints_func)(int value, void *context);

    /* Update bank base (used for drives).  */
    void (*set_bank_base)(void *context);

    void (*get_line_cycle)(unsigned int *line, unsigned int *cycle, int *half_cycle);

    void *context;
};
typedef struct monitor_interface_s monitor_interface_t;

/* Externals */
extern unsigned monitor_mask[NUM_MEMSPACES];


/* Prototypes */
monitor_cpu_type_t* monitor_find_cpu_type_from_string(const char *cpu_type);

void monitor_init(monitor_interface_t * maincpu_interface,
                  monitor_interface_t * drive_interface_init[],
                  struct monitor_cpu_type_s **asmarray);
void monitor_shutdown(void);
int monitor_cmdline_options_init(void);
int monitor_resources_init(void);
void monitor_resources_shutdown(void);
void monitor_startup(MEMSPACE mem);
void monitor_startup_trap(void);
bool monitor_is_inside_monitor(void);

void monitor_reset_hook(void);
void monitor_vsync_hook(void);

void monitor_abort(void);

int monitor_force_import(MEMSPACE mem);
void monitor_check_icount(uint16_t a);
void monitor_check_icount_interrupt(void);
void monitor_check_watchpoints(unsigned int lastpc, unsigned int pc);

void monitor_cpu_type_set(const char *cpu_type);

void monitor_watch_push_load_addr(uint16_t addr, MEMSPACE mem);
void monitor_watch_push_store_addr(uint16_t addr, MEMSPACE mem);

monitor_interface_t *monitor_interface_new(void);
void monitor_interface_destroy(monitor_interface_t *monitor_interface);

int monitor_diskspace_dnr(int mem);
int monitor_diskspace_mem(int dnr);

int mon_out(const char *format, ...) VICE_ATTR_PRINTF;

/** Breakpoint interface.  */

#define MONITOR_MAX_CHECKPOINTS 9

/* Prototypes */
int monitor_check_breakpoints(MEMSPACE mem, uint16_t addr);

/** Disassemble interace */
/* Prototypes */
const char *mon_disassemble_to_string(MEMSPACE, unsigned int addr, unsigned int x,
                                      unsigned int p1, unsigned int p2, unsigned int p3,
                                      int hex_mode,
                                      const char *cpu_type);

/** Register interface.  */
struct mon_reg_list_s *mon_register_list_get(int mem);
void mon_ioreg_add_list(struct mem_ioreg_list_s **list, const char *name,
                        int start, int end, void *dump, void *context, int mirror_mode);

/* Assembler initialization.  */
void asm6502_init(struct monitor_cpu_type_s *monitor_cpu_type);
void asmR65C02_init(struct monitor_cpu_type_s *monitor_cpu_type);
void asm65816_init(struct monitor_cpu_type_s *monitor_cpu_type);
void asm6502dtv_init(struct monitor_cpu_type_s *monitor_cpu_type);
void asm6809_init(struct monitor_cpu_type_s *monitor_cpu_type);
void asmz80_init(struct monitor_cpu_type_s *monitor_cpu_type);

struct monitor_cartridge_commands_s {
    int (*cartridge_attach_image)(int type, const char *filename);
    void (*cartridge_detach_image)(int type);
    void (*cartridge_trigger_freeze)(void);
    void (*cartridge_trigger_freeze_nmi_only)(void);
    void (*export_dump)(void);
};
typedef struct monitor_cartridge_commands_s monitor_cartridge_commands_t;

extern monitor_cartridge_commands_t mon_cart_cmd;

/* CPU history/memmap prototypes */
void monitor_cpuhistory_store(CLOCK cycle, unsigned int addr, unsigned int op, unsigned int p1, unsigned int p2,
                              uint8_t reg_a, uint8_t reg_x, uint8_t reg_y,
                              uint8_t reg_sp, unsigned int reg_st, uint8_t origin);
void monitor_cpuhistory_fix_p2(unsigned int p2);
void monitor_memmap_store(unsigned int addr, unsigned int type);

/* memmap defines */
#define MEMMAP_I_O_R    (1 << 8)
#define MEMMAP_I_O_W    (1 << 7)
#define MEMMAP_I_O_X    (1 << 6)
#define MEMMAP_ROM_R    (1 << 5)
#define MEMMAP_ROM_W    (1 << 4)
#define MEMMAP_ROM_X    (1 << 3)
#define MEMMAP_RAM_R    (1 << 2)
#define MEMMAP_RAM_W    (1 << 1)
#define MEMMAP_RAM_X    (1 << 0)

/* HACK to enable fetch/load separation */
extern uint8_t memmap_state;
#define MEMMAP_STATE_OPCODE     0x01
#define MEMMAP_STATE_INSTR      0x02
#define MEMMAP_STATE_IGNORE     0x04
#define MEMMAP_STATE_IN_MONITOR 0x08

#endif
