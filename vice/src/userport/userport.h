/*
 * userport.h - userport abstraction system.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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

#ifndef VICE_USERPORT_H
#define VICE_USERPORT_H

#include "snapshot.h"
#include "types.h"

/* #define USERPORT_EXPERIMENTAL_DEVICES */

#define USERPORT_NO_PULSE   0
#define USERPORT_PULSE      1

enum {
    USERPORT_DEVICE_NONE = 0,
    USERPORT_DEVICE_PRINTER,
    USERPORT_DEVICE_RS232_MODEM,
    USERPORT_DEVICE_JOYSTICK_CGA,
    USERPORT_DEVICE_JOYSTICK_PET,
    USERPORT_DEVICE_JOYSTICK_HUMMER,
    USERPORT_DEVICE_JOYSTICK_OEM,
    USERPORT_DEVICE_JOYSTICK_HIT,
    USERPORT_DEVICE_JOYSTICK_KINGSOFT,
    USERPORT_DEVICE_JOYSTICK_STARBYTE,
    USERPORT_DEVICE_JOYSTICK_SYNERGY,
    USERPORT_DEVICE_JOYSTICK_WOJ,
    USERPORT_DEVICE_DAC,
    USERPORT_DEVICE_DIGIMAX,
    USERPORT_DEVICE_4BIT_SAMPLER,
    USERPORT_DEVICE_8BSS,
    USERPORT_DEVICE_RTC_58321A,
    USERPORT_DEVICE_RTC_DS1307,
    USERPORT_DEVICE_PETSCII_SNESPAD,
    USERPORT_DEVICE_SUPERPAD64,
    USERPORT_DEVICE_DIAG_586220_HARNESS,
    USERPORT_DEVICE_DRIVE_PAR_CABLE,
    USERPORT_DEVICE_IO_SIMULATION,
    USERPORT_DEVICE_WIC64,
    USERPORT_DEVICE_SPACEBALLS,
    USERPORT_DEVICE_SPT_JOYSTICK,

    /* This item always needs to be at the end */
    USERPORT_MAX_DEVICES
};

enum {
    USERPORT_DEVICE_TYPE_NONE = 0,
    USERPORT_DEVICE_TYPE_PRINTER,
    USERPORT_DEVICE_TYPE_MODEM,
    USERPORT_DEVICE_TYPE_DRIVE_PAR_CABLE,
    USERPORT_DEVICE_TYPE_JOYSTICK_ADAPTER,
    USERPORT_DEVICE_TYPE_AUDIO_OUTPUT,
    USERPORT_DEVICE_TYPE_SAMPLER,
    USERPORT_DEVICE_TYPE_RTC,
#ifdef USERPORT_EXPERIMENTAL_DEVICES
    USERPORT_DEVICE_TYPE_HARNESS,
    USERPORT_DEVICE_TYPE_WIFI,
#endif
    USERPORT_DEVICE_TYPE_IO_SIMULATION
};

/* this structure is used by userport devices */
typedef struct userport_device_s {
    /* Name of the device */
    char *name;

    /* flag to indicate that the device is a joystick/pad adapter */
    int joystick_adapter_id;

    /* flag to indicate the device type */
    int device_type;

    /* Device enable/disable */
    int (*enable)(int val);

    /* Read pb0-7 pins */
    uint8_t (*read_pbx)(uint8_t orig);

    /* Store pb0-7 pins */
    void (*store_pbx)(uint8_t val, int pulse);

    /* Read pa2 pin */
    uint8_t (*read_pa2)(uint8_t orig);

    /* Store pa2 pin */
    void (*store_pa2)(uint8_t val);

    /* Read pa3 pin */
    uint8_t (*read_pa3)(uint8_t orig);

    /* Store pa3 pin */
    void (*store_pa3)(uint8_t val);

    /* Device needs pc pin */
    int needs_pc;

    /* Store sp1 pin */
    void (*store_sp1)(uint8_t val);

    /* Read sp1 pin */
    uint8_t (*read_sp1)(uint8_t orig);

    /* Store sp2 pin */
    void (*store_sp2)(uint8_t val);

    /* Read sp2 pin */
    uint8_t (*read_sp2)(uint8_t orig);

    /* device reset function, reset line on the userport */
    void (*reset)(void);

    /* device powerup function, gets called when a hard reset is done */
    void (*powerup)(void);

    /* Snapshot write */
    int (*write_snapshot)(struct snapshot_s *s);

    /* Snapshot read */
    int (*read_snapshot)(struct snapshot_s *s);  /* pointer to the device snapshot read function */
} userport_device_t;

/* this structure is used by userport ports */
typedef struct userport_port_props_s {
    int has_pa2;                   /* port has the pa2 line */
    int has_pa3;                   /* port has the pa3 line */
    void (*set_flag)(uint8_t val); /* pointer to set flag function */
    int has_pc;                    /* port has the pc line */
    int has_sp12;                  /* port has the sp1 and sp2 lines */
    int has_reset;                 /* port had the reset line */
} userport_port_props_t;

typedef struct userport_desc_s {
    char *name;
    int id;
    int device_type;
} userport_desc_t;

void userport_port_register(userport_port_props_t *props);
int userport_device_register(int id, userport_device_t *device);

uint8_t read_userport_pbx(uint8_t orig);
void store_userport_pbx(uint8_t val, int pulse);
uint8_t read_userport_pa2(uint8_t orig);
void store_userport_pa2(uint8_t val);
uint8_t read_userport_pa3(uint8_t orig);
void store_userport_pa3(uint8_t val);
uint8_t read_userport_pc(uint8_t orig);
uint8_t read_userport_sp1(uint8_t orig);
void store_userport_sp1(uint8_t val);
uint8_t read_userport_sp2(uint8_t orig);
void store_userport_sp2(uint8_t val);
void userport_reset_start(void);
void userport_reset(void);
void userport_reset_end(void);
void userport_powerup(void);

/* use this function from userport device code to set the userport flag */
void set_userport_flag(uint8_t val);

int userport_resources_init(void);
int userport_cmdline_options_init(void);

userport_desc_t *userport_get_valid_devices(int sort);
const char *userport_get_device_type_desc(int type);

void userport_enable(int val);

int userport_snapshot_write_module(snapshot_t *s);
int userport_snapshot_read_module(snapshot_t *s);

#endif
