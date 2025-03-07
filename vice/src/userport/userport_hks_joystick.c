/*
 * userport_hks_joystick.c - Userport HIT, Kingsoft and Starbyte joystick adapters emulation.
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

/* C64 HIT userport joystick adapter (C64/C128)

C64/C128 | JOY1 | JOY2 | NOTES
------------------------------
    4    |  -   |  -   | CNT1 -> CNT2
    6    |  -   |  -   | CNT2 <- CNT1
    7    |  -   |  6   | SP2 <- JOY2 FIRE
    C    |  1   |  -   | PB0 <- JOY1 UP
    D    |  2   |  -   | PB1 <- JOY1 DOWN
    E    |  3   |  -   | PB2 <- JOY1 LEFT
    F    |  4   |  -   | PB3 <- JOY1 RIGHT
    H    |  -   |  1   | PB4 <- JOY2 UP
    J    |  -   |  2   | PB5 <- JOY2 DOWN
    K    |  -   |  3   | PB6 <- JOY2 LEFT
    L    |  -   |  4   | PB7 <- JOY2 RIGHT
    M    |  -   |  6   | PA2 <- JOY1 FIRE
*/

/* C64 KingSoft userport joystick adapter (C64/C128)

C64/C128 | JOY1 | JOY2 | NOTES
------------------------------
    4    |  -   |  -   | CNT1 -> CNT2
    6    |  -   |  -   | CNT2 <- CNT1
    7    |  -   |  6   | SP2 <- JOY2 FIRE
    C    |  -   |  4   | PB0 <- JOY2 RIGHT
    D    |  -   |  3   | PB1 <- JOY2 LEFT
    E    |  -   |  2   | PB2 <- JOY2 DOWN
    F    |  -   |  1   | PB3 <- JOY2 UP
    H    |  6   |  -   | PB4 <- JOY1 FIRE
    J    |  4   |  -   | PB5 <- JOY1 RIGHT
    K    |  3   |  -   | PB6 <- JOY1 LEFT
    L    |  2   |  -   | PB7 <- JOY1 DOWN
    M    |  1   |  -   | PA2 <- JOY1 UP
*/

/* C64 StarByte userport joystick adapter (C64/C128)

C64/C128 | JOY1 | JOY2 | NOTES
------------------------------
   4     |  -   |  -   | CNT1 -> CNT2
   6     |  -   |  -   | CNT2 <- CNT1
   7     |  6   |  -   | SP2 <- JOY1 FIRE
   C     |  2   |  -   | PB0 <- JOY1 DOWN
   D     |  4   |  -   | PB1 <- JOY1 RIGHT
   E     |  3   |  -   | PB2 <- JOY1 LEFT
   F     |  1   |  -   | PB3 <- JOY1 UP
   H     |  -   |  6   | PB4 <- JOY2 FIRE
   J     |  -   |  2   | PB5 <- JOY2 DOWN
   K     |  -   |  4   | PB6 <- JOY2 RIGHT
   L     |  -   |  3   | PB7 <- JOY2 LEFT
   M     |  -   |  1   | PA2 <- JOY2 UP
*/

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmdline.h"
#include "resources.h"
#include "joyport.h"
#include "joystick.h"
#include "snapshot.h"
#include "userport.h"
#include "userport_hks_joystick.h"
#include "machine.h"
#include "uiapi.h"

#include "log.h"

static int userport_joy_hit_enable = 0;
static int userport_joy_kingsoft_enable = 0;
static int userport_joy_starbyte_enable = 0;

/* Some prototypes are needed */
static void userport_joystick_hit_store_pbx(uint8_t value, int pulse);
static uint8_t userport_joystick_hit_read_pbx(uint8_t orig);
static uint8_t userport_joystick_hit_read_pa2(uint8_t orig);
static void userport_joystick_hit_store_pa2(uint8_t val);
static void userport_joystick_hit_store_sp1(uint8_t val);
static uint8_t userport_joystick_hit_read_sp2(uint8_t orig);
static int userport_joystick_hit_write_snapshot_module(snapshot_t *s);
static int userport_joystick_hit_read_snapshot_module(snapshot_t *s);
static int userport_joystick_hit_enable(int value);

static uint8_t userport_joystick_kingsoft_read_pbx(uint8_t orig);
static void userport_joystick_kingsoft_store_pbx(uint8_t value, int pulse);
static uint8_t userport_joystick_kingsoft_read_pa2(uint8_t orig);
static void userport_joystick_kingsoft_store_pa2(uint8_t val);
static void userport_joystick_kingsoft_store_sp1(uint8_t val);
static uint8_t userport_joystick_kingsoft_read_sp2(uint8_t orig);
static int userport_joystick_kingsoft_write_snapshot_module(snapshot_t *s);
static int userport_joystick_kingsoft_read_snapshot_module(snapshot_t *s);
static int userport_joystick_kingsoft_enable(int value);

static uint8_t userport_joystick_starbyte_read_pbx(uint8_t orig);
static void userport_joystick_starbyte_store_pbx(uint8_t value, int pulse);
static uint8_t userport_joystick_starbyte_read_pa2(uint8_t orig);
static void userport_joystick_starbyte_store_pa2(uint8_t val);
static void userport_joystick_starbyte_store_sp1(uint8_t val);
static uint8_t userport_joystick_starbyte_read_sp2(uint8_t orig);
static int userport_joystick_starbyte_write_snapshot_module(snapshot_t *s);
static int userport_joystick_starbyte_read_snapshot_module(snapshot_t *s);
static int userport_joystick_starbyte_enable(int value);

static userport_device_t hit_device = {
    "HIT userport joy adapter",                  /* device name */
    JOYSTICK_ADAPTER_ID_GENERIC_USERPORT,        /* this is a joystick adapter */
    USERPORT_DEVICE_TYPE_JOYSTICK_ADAPTER,       /* device is a joystick adapter */
    userport_joystick_hit_enable,                /* enable function */
    userport_joystick_hit_read_pbx,              /* read pb0-pb7 function */
    userport_joystick_hit_store_pbx,             /* store pb0-pb7 function */
    userport_joystick_hit_read_pa2,              /* read pa2 pin function */
    userport_joystick_hit_store_pa2,             /* store pa2 pin function */
    NULL,                                        /* NO read pa3 pin function */
    NULL,                                        /* NO store pa3 pin function */
    0,                                           /* pc pin is NOT needed */
    userport_joystick_hit_store_sp1,             /* store sp1 pin function */
    NULL,                                        /* NO read sp1 pin function */
    NULL,                                        /* NO store sp2 pin function */
    userport_joystick_hit_read_sp2,              /* read sp2 pin function */
    NULL,                                        /* NO reset function */
    NULL,                                        /* NO powerup function */
    userport_joystick_hit_write_snapshot_module, /* snapshot write function */
    userport_joystick_hit_read_snapshot_module   /* snapshot read function */
};

static userport_device_t kingsoft_device = {
    "KingSoft userport joy adapter",                  /* device name */
    JOYSTICK_ADAPTER_ID_GENERIC_USERPORT,             /* this is a joystick adapter */
    USERPORT_DEVICE_TYPE_JOYSTICK_ADAPTER,            /* device is a joystick adapter */
    userport_joystick_kingsoft_enable,                /* enable function */
    userport_joystick_kingsoft_read_pbx,              /* read pb0-pb7 function */
    userport_joystick_kingsoft_store_pbx,             /* store pb0-pb7 function */
    userport_joystick_kingsoft_read_pa2,              /* read pa2 pin function */
    userport_joystick_kingsoft_store_pa2,             /* store pa2 pin function */
    NULL,                                             /* NO read pa3 pin function */
    NULL,                                             /* NO store pa3 pin function */
    0,                                                /* pc pin is NOT needed */
    userport_joystick_kingsoft_store_sp1,             /* store sp1 pin function */
    NULL,                                             /* NO read sp1 pin function */
    NULL,                                             /* NO store sp2 pin function */
    userport_joystick_kingsoft_read_sp2,              /* read sp2 pin function */
    NULL,                                             /* NO reset function */
    NULL,                                             /* NO powerup function */
    userport_joystick_kingsoft_write_snapshot_module, /* snapshot write function */
    userport_joystick_kingsoft_read_snapshot_module   /* snapshot read function */
};

static userport_device_t starbyte_device = {
    "StarByte userport joy adapter",                  /* device name */
    JOYSTICK_ADAPTER_ID_GENERIC_USERPORT,             /* this is a joystick adapter */
    USERPORT_DEVICE_TYPE_JOYSTICK_ADAPTER,            /* device is a joystick adapter */
    userport_joystick_starbyte_enable,                /* enable function */
    userport_joystick_starbyte_read_pbx,              /* read pb0-pb7 function */
    userport_joystick_starbyte_store_pbx,             /* store pb0-pb7 function */
    userport_joystick_starbyte_read_pa2,              /* read pa2 pin function */
    userport_joystick_starbyte_store_pa2,             /* store pa2 pin function */
    NULL,                                             /* NO read pa3 pin function */
    NULL,                                             /* NO store pa3 pin function */
    0,                                                /* pc pin is NOT needed */
    userport_joystick_starbyte_store_sp1,             /* store sp1 pin function */
    NULL,                                             /* NO read sp1 pin function */
    NULL,                                             /* NO store sp2 pin function */
    userport_joystick_starbyte_read_sp2,              /* read sp2 pin function */
    NULL,                                             /* NO reset function */
    NULL,                                             /* NO powerup function */
    userport_joystick_starbyte_write_snapshot_module, /* snapshot write function */
    userport_joystick_starbyte_read_snapshot_module   /* snapshot read function */
};

/* ------------------------------------------------------------------------- */

static int userport_joystick_hit_kingsoft_output_check(int port, uint8_t output_bits)
{
    if (port == JOYPORT_4) {
        if (output_bits & 0x10) {   /* no output on 'fire' pin for userport joystick 2 */
            return 0;
        }
    }
    return 1;
}

static int userport_joystick_starbyte_output_check(int port, uint8_t output_bits)
{
    if (port == JOYPORT_3) {
        if (output_bits & 0x10) {   /* no output on 'fire' pin for userport joystick 1 */
            return 0;
        }
    }
    return 1;
}

static int userport_joystick_hit_enable(int value)
{
    int val = value ? 1 : 0;

    if (userport_joy_hit_enable == val) {
        return 0;
    }

    if (val) {
        /* check if a different joystick adapter is already active */
        if (joystick_adapter_get_id()) {
            ui_error("Joystick adapter %s is already active", joystick_adapter_get_name());
            return -1;
        }
        joystick_adapter_activate(JOYSTICK_ADAPTER_ID_GENERIC_USERPORT, "Userport HIT joystick adapter");
        joystick_adapter_set_output_check_function(userport_joystick_hit_kingsoft_output_check);
        joystick_adapter_set_ports(2);
    } else {
        joystick_adapter_deactivate();
    }

    userport_joy_hit_enable = val;

    return 0;
}

static int userport_joystick_kingsoft_enable(int value)
{
    int val = value ? 1 : 0;

    if (userport_joy_kingsoft_enable == val) {
        return 0;
    }

    if (val) {
        /* check if a different joystick adapter is already active */
        if (joystick_adapter_get_id()) {
            ui_error("Joystick adapter %s is already active", joystick_adapter_get_name());
            return -1;
        }
        joystick_adapter_activate(JOYSTICK_ADAPTER_ID_GENERIC_USERPORT, "Userport Kingsoft joystick adapter");
        joystick_adapter_set_output_check_function(userport_joystick_hit_kingsoft_output_check);
        joystick_adapter_set_ports(2);
    } else {
        joystick_adapter_deactivate();
    }

    userport_joy_kingsoft_enable = val;

    return 0;
}

static int userport_joystick_starbyte_enable(int value)
{
    int val = value ? 1 : 0;

    if (userport_joy_starbyte_enable == val) {
        return 0;
    }

    if (val) {
        /* check if a different joystick adapter is already active */
        if (joystick_adapter_get_id()) {
            ui_error("Joystick adapter %s is already active", joystick_adapter_get_name());
            return -1;
        }
        joystick_adapter_activate(JOYSTICK_ADAPTER_ID_GENERIC_USERPORT, "Userport Starbyte joystick adapter");
        joystick_adapter_set_output_check_function(userport_joystick_starbyte_output_check);
        joystick_adapter_set_ports(2);
    } else {
        joystick_adapter_deactivate();
    }

    userport_joy_starbyte_enable = val;

    return 0;
}

int userport_joystick_hit_resources_init(void)
{
    return userport_device_register(USERPORT_DEVICE_JOYSTICK_HIT, &hit_device);
}

int userport_joystick_kingsoft_resources_init(void)
{
    return userport_device_register(USERPORT_DEVICE_JOYSTICK_KINGSOFT, &kingsoft_device);
}

int userport_joystick_starbyte_resources_init(void)
{
    return userport_device_register(USERPORT_DEVICE_JOYSTICK_STARBYTE, &starbyte_device);
}

/* ---------------------------------------------------------------------*/

static void userport_joystick_hit_store_pbx(uint8_t value, int pulse)
{
    uint8_t j1 = value & 0xf;
    uint8_t j2 = (value & 0xf0) >> 4;

    store_joyport_dig(JOYPORT_3, j1, 0xf);
    store_joyport_dig(JOYPORT_4, j2, 0xf);
}

static uint8_t userport_joystick_hit_read_pbx(uint8_t orig)
{
    uint8_t retval;
    uint8_t jv3 = ~read_joyport_dig(JOYPORT_3);
    uint8_t jv4 = ~read_joyport_dig(JOYPORT_4);

    retval = (uint8_t)~((jv3 & 0xf) | ((jv4 & 0xf) << 4));

    return retval;
}

static uint8_t userport_joystick_hit_read_pa2(uint8_t orig)
{
    uint8_t jv1 = ~read_joyport_dig(JOYPORT_3);

    return (jv1 & 0x10) ? 0 : 1;
}

static void userport_joystick_hit_store_pa2(uint8_t val)
{
    store_joyport_dig(JOYPORT_3, (val & 1) << 4, 0x10);
}

static uint8_t hit_sp2_retval = 0xff;

static void userport_joystick_hit_store_sp1(uint8_t val)
{
    hit_sp2_retval = ((~read_joyport_dig(JOYPORT_4)) & 0x10) ? 0 : 0xff;
}

static uint8_t userport_joystick_hit_read_sp2(uint8_t orig)
{
    return hit_sp2_retval;
}

/* ------------------------------------------------------------------------- */

static uint8_t userport_joystick_kingsoft_read_pbx(uint8_t orig)
{
    uint8_t retval;
    uint8_t jv3 = ~read_joyport_dig(JOYPORT_3);
    uint8_t jv4 = ~read_joyport_dig(JOYPORT_4);

    retval = ((jv4 >> 3) & 1) << 0;
    retval |= ((jv4 >> 2) & 1) << 1;
    retval |= ((jv4 >> 1) & 1) << 2;
    retval |= ((jv4 >> 0) & 1) << 3;
    retval |= ((jv3 >> 4) & 1) << 4;
    retval |= ((jv3 >> 3) & 1) << 5;
    retval |= ((jv3 >> 2) & 1) << 6;
    retval |= ((jv3 >> 1) & 1) << 7;
    retval = (uint8_t)~retval;

    return retval;
}

static void userport_joystick_kingsoft_store_pbx(uint8_t value, int pulse)
{
    uint8_t j1 = value & 0x10;
    uint8_t j2 = (value & 1) << 3;

    j1 |= (value & 0x20) >> 2;
    j1 |= (value & 0x40) >> 4;
    j1 |= (value & 0x80) >> 6;
    j2 |= (value & 2) << 1;
    j2 |= (value & 4) >> 1;
    j2 |= (value & 8) >> 3;

    store_joyport_dig(JOYPORT_3, j1, 0x1e);
    store_joyport_dig(JOYPORT_4, j2, 0xf);
}

static uint8_t userport_joystick_kingsoft_read_pa2(uint8_t orig)
{
    uint8_t jv1 = ~read_joyport_dig(JOYPORT_3);

    return (jv1 & 1) ? 0 : 1;
}

static void userport_joystick_kingsoft_store_pa2(uint8_t val)
{
    store_joyport_dig(JOYPORT_3, val & 1, 1);
}

static uint8_t kingsoft_sp2_retval = 0xff;

static void userport_joystick_kingsoft_store_sp1(uint8_t val)
{
    kingsoft_sp2_retval = ((~read_joyport_dig(JOYPORT_4)) & 0x10) ? 0 : 0xff;
}

static uint8_t userport_joystick_kingsoft_read_sp2(uint8_t orig)
{
    return kingsoft_sp2_retval;
}

/* ------------------------------------------------------------------------- */

static uint8_t userport_joystick_starbyte_read_pbx(uint8_t orig)
{
    uint8_t retval;
    uint8_t jv3 = ~read_joyport_dig(JOYPORT_3);
    uint8_t jv4 = ~read_joyport_dig(JOYPORT_4);

    retval = ((jv3 >> 1) & 1) << 0;
    retval |= ((jv3 >> 3) & 1) << 1;
    retval |= ((jv3 >> 2) & 1) << 2;
    retval |= ((jv3 >> 0) & 1) << 3;
    retval |= ((jv4 >> 4) & 1) << 4;
    retval |= ((jv4 >> 1) & 1) << 5;
    retval |= ((jv4 >> 3) & 1) << 6;
    retval |= ((jv4 >> 2) & 1) << 7;
    retval = (uint8_t)~retval;

    return retval;
}

static void userport_joystick_starbyte_store_pbx(uint8_t value, int pulse)
{
    uint8_t j1 = (value & 1) << 1;
    uint8_t j2 = value & 0x10;

    j1 |= (value & 2) << 2;
    j1 |= value & 4;
    j1 |= (value & 8) >> 3;

    j2 |= (value & 0x20) >> 4;
    j2 |= (value & 0x40) >> 3;
    j2 |= (value & 0x80) >> 5;

    store_joyport_dig(JOYPORT_3, j1, 0xf);
    store_joyport_dig(JOYPORT_4, j2, 0x1e);
}

static uint8_t userport_joystick_starbyte_read_pa2(uint8_t orig)
{
    uint8_t jv2 = ~read_joyport_dig(JOYPORT_4);

    return (jv2 & 1) ? 0 : 1;
}

static void userport_joystick_starbyte_store_pa2(uint8_t val)
{
    store_joyport_dig(JOYPORT_4, val & 1, 1);
}

static uint8_t starbyte_sp2_retval = 0xff;

static void userport_joystick_starbyte_store_sp1(uint8_t val)
{
    starbyte_sp2_retval = ((~read_joyport_dig(JOYPORT_3)) & 0x10) ? 0 : 0xff;
}

static uint8_t userport_joystick_starbyte_read_sp2(uint8_t orig)
{
    return starbyte_sp2_retval;
}

/* ---------------------------------------------------------------------*/

/* UP_JOY_HIT snapshot module format:

   type  | name   | description
   ----------------------------
   BYTE  | retval | current serial port brigde value
 */

static const char hit_module_name[] = "UPJOYHIT";
#define HIT_VER_MAJOR   0
#define HIT_VER_MINOR   1

static int userport_joystick_hit_write_snapshot_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, hit_module_name, HIT_VER_MAJOR, HIT_VER_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (SMW_B(m, hit_sp2_retval) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    if (0
        || joyport_snapshot_write_module(s, JOYPORT_3) < 0
        || joyport_snapshot_write_module(s, JOYPORT_4) < 0) {
        return -1;
    }
    return 0;
}

static int userport_joystick_hit_read_snapshot_module(snapshot_t *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;

    m = snapshot_module_open(s, hit_module_name, &major_version, &minor_version);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (snapshot_version_is_bigger(major_version, minor_version, HIT_VER_MAJOR, HIT_VER_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    if (SMR_B(m, &hit_sp2_retval) < 0) {
        goto fail;
    }
    snapshot_module_close(m);

    if (0
        || joyport_snapshot_read_module(s, JOYPORT_3) < 0
        || joyport_snapshot_read_module(s, JOYPORT_4) < 0) {
        return -1;
    }
    return 0;

fail:
    snapshot_module_close(m);
    return -1;
}

/* ------------------------------------------------------------------------- */

/* UP_JOY_KINGSOFT snapshot module format:

   type  | name   | description
   ----------------------------
   BYTE  | retval | current serial port brigde value
 */

/* FIXME */
static const char kingsoft_module_name[] = "UPJOYKINGSOFT";
#define KINGSOFT_VER_MAJOR   0
#define KINGSOFT_VER_MINOR   1

static int userport_joystick_kingsoft_write_snapshot_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, kingsoft_module_name, KINGSOFT_VER_MAJOR, KINGSOFT_VER_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (SMW_B(m, kingsoft_sp2_retval) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    if (0
        || joyport_snapshot_write_module(s, JOYPORT_3) < 0
        || joyport_snapshot_write_module(s, JOYPORT_4) < 0) {
        return -1;
    }
    return 0;
}

static int userport_joystick_kingsoft_read_snapshot_module(snapshot_t *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;

    m = snapshot_module_open(s, kingsoft_module_name, &major_version, &minor_version);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (snapshot_version_is_bigger(major_version, minor_version, KINGSOFT_VER_MAJOR, KINGSOFT_VER_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    if (SMR_B(m, &kingsoft_sp2_retval) < 0) {
        goto fail;
    }
    snapshot_module_close(m);

    if (0
        || joyport_snapshot_read_module(s, JOYPORT_3) < 0
        || joyport_snapshot_read_module(s, JOYPORT_4) < 0) {
        return -1;
    }
    return 0;

fail:
    snapshot_module_close(m);
    return -1;
}

/* ------------------------------------------------------------------------- */

/* UP_JOY_STARBYTE snapshot module format:

   type  | name   | description
   ----------------------------
   BYTE  | retval | current serial port bridge value
 */

static const char starbyte_module_name[] = "UPJOYSTARBYTE";
#define STARBYTE_VER_MAJOR   0
#define STARBYTE_VER_MINOR   1

static int userport_joystick_starbyte_write_snapshot_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, starbyte_module_name, STARBYTE_VER_MAJOR, STARBYTE_VER_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (SMW_B(m, starbyte_sp2_retval) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    if (0
        || joyport_snapshot_write_module(s, JOYPORT_3) < 0
        || joyport_snapshot_write_module(s, JOYPORT_4) < 0) {
        return -1;
    }
    return 0;
}

static int userport_joystick_starbyte_read_snapshot_module(snapshot_t *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;

    m = snapshot_module_open(s, starbyte_module_name, &major_version, &minor_version);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (snapshot_version_is_bigger(major_version, minor_version, STARBYTE_VER_MAJOR, STARBYTE_VER_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    if (SMR_B(m, &starbyte_sp2_retval) < 0) {
        goto fail;
    }
    snapshot_module_close(m);

    if (0
        || joyport_snapshot_read_module(s, JOYPORT_3) < 0
        || joyport_snapshot_read_module(s, JOYPORT_4) < 0) {
        return -1;
    }
    return 0;

fail:
    snapshot_module_close(m);
    return -1;
}
