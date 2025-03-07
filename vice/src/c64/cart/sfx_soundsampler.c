/*
 * sfx_soundsampler.c - SFX Sound Sampler cartridge emulation.
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

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cartio.h"
#include "cartridge.h"
#include "cmdline.h"
#include "export.h"
#include "lib.h"
#include "log.h"
#include "machine-video.h"
#include "machine.h"
#include "maincpu.h"
#include "resources.h"
#include "sampler.h"
#include "sfx_soundsampler.h"
#include "sid.h"
#include "snapshot.h"
#include "sound.h"
#include "uiapi.h"

/* ------------------------------------------------------------------------- */

static uint8_t current_sample = 0x80;

/* ------------------------------------------------------------------------- */

/* some prototypes are needed */
static void sfx_soundsampler_sound_store(uint16_t addr, uint8_t value);
static uint8_t sfx_soundsampler_sample_read(uint16_t addr);
static void sfx_soundsampler_latch_sample(uint16_t addr, uint8_t value);

static io_source_t sfx_soundsampler_io1_device = {
    CARTRIDGE_NAME_SFX_SOUND_SAMPLER, /* name of the device */
    IO_DETACH_RESOURCE,               /* use resource to detach the device when involved in a read-collision */
    "SFXSoundSampler",                /* resource to set to '0' */
    0xde00, 0xdeff, 0x00,             /* range for the device, reg:$de00, mirrors:$de01-$deff, range is different on vic20 */
    0,                                /* read is never valid, reg is write only */
    sfx_soundsampler_latch_sample,    /* store function */
    NULL,                             /* NO poke function */
    NULL,                             /* NO read function */
    NULL,                             /* TODO: peek function */
    NULL,                             /* nothing to dump */
    CARTRIDGE_SFX_SOUND_SAMPLER,      /* cartridge ID */
    IO_PRIO_NORMAL,                   /* normal priority, device read needs to be checked for collisions */
    0                                 /* insertion order, gets filled in by the registration function */
};

static io_source_t sfx_soundsampler_io2_device = {
    CARTRIDGE_NAME_SFX_SOUND_SAMPLER, /* name of the device */
    IO_DETACH_RESOURCE,               /* use resource to detach the device when involved in a read-collision */
    "SFXSoundSampler",                /* resource to set to '0' */
    0xdf00, 0xdfff, 0x00,             /* range for the device, reg:$df00, mirrors:$df01-$dfff, range is different on vic20 */
    1,                                /* read is always valid */
    sfx_soundsampler_sound_store,     /* store function */
    NULL,                             /* NO poke function */
    sfx_soundsampler_sample_read,     /* read function */
    NULL,                             /* TODO: peek function */
    NULL,                             /* nothing to dump */
    CARTRIDGE_SFX_SOUND_SAMPLER,      /* cartridge ID */
    IO_PRIO_NORMAL,                   /* normal priority, device read needs to be checked for collisions */
    0                                 /* insertion order, gets filled in by the registration function */
};

static io_source_list_t *sfx_soundsampler_io1_list_item = NULL;
static io_source_list_t *sfx_soundsampler_io2_list_item = NULL;

static const export_resource_t export_res = {
    CARTRIDGE_NAME_SFX_SOUND_SAMPLER, 0, 0, &sfx_soundsampler_io1_device, &sfx_soundsampler_io2_device, CARTRIDGE_SFX_SOUND_SAMPLER
};

/* ------------------------------------------------------------------------- */

/* Some prototypes are needed */
static int sfx_soundsampler_sound_machine_init(sound_t *psid, int speed, int cycles_per_sec);
static void sfx_soundsampler_sound_machine_store(sound_t *psid, uint16_t addr, uint8_t val);
static uint8_t sfx_soundsampler_sound_machine_read(sound_t *psid, uint16_t addr);
static void sfx_soundsampler_sound_reset(sound_t *psid, CLOCK cpu_clk);

#ifdef SOUND_SYSTEM_FLOAT
static int sfx_soundsampler_sound_machine_calculate_samples(sound_t **psid, float *pbuf, int nr, int sound_chip_channels, CLOCK *delta_t);
#else
static int sfx_soundsampler_sound_machine_calculate_samples(sound_t **psid, int16_t *pbuf, int nr, int sound_output_channels, int sound_chip_channels, CLOCK *delta_t);
#endif

static int sfx_soundsampler_sound_machine_cycle_based(void)
{
    return 0;
}

static int sfx_soundsampler_sound_machine_channels(void)
{
    return 1;
}

#ifdef SOUND_SYSTEM_FLOAT
/* stereo mixing placement of the SFX Sound Sampler sound */
static sound_chip_mixing_spec_t sfx_soundsampler_sound_mixing_spec[SOUND_CHIP_CHANNELS_MAX] = {
    {
        100, /* left channel volume % in case of stereo output, default output to both */
        100  /* right channel volume % in case of stereo output, default output to both */
    }
};
#endif

/* SFX Sound Sampler cartridge sound chip */
static sound_chip_t sfx_soundsampler_sound_chip = {
    NULL,                                             /* NO sound chip open function */
    sfx_soundsampler_sound_machine_init,              /* sound chip init function */
    NULL,                                             /* NO sound chip close function */
    sfx_soundsampler_sound_machine_calculate_samples, /* sound chip calculate samples function */
    sfx_soundsampler_sound_machine_store,             /* sound chip store function */
    sfx_soundsampler_sound_machine_read,              /* sound chip read function */
    sfx_soundsampler_sound_reset,                     /* sound chip reset function */
    sfx_soundsampler_sound_machine_cycle_based,       /* sound chip 'is_cycle_based()' function, sound chip is NOT cycle based */
    sfx_soundsampler_sound_machine_channels,          /* sound chip 'get_amount_of_channels()' function, sound chip has 1 channel */
#ifdef SOUND_SYSTEM_FLOAT
    sfx_soundsampler_sound_mixing_spec,               /* stereo mixing placement specs */
#endif
    0                                                 /* chip enabled, toggled when sound chip is (de-)activated */
};

static uint16_t sfx_soundsampler_sound_chip_offset = 0;
static sound_dac_t sfx_soundsampler_dac;

void sfx_soundsampler_sound_chip_init(void)
{
    sfx_soundsampler_sound_chip_offset = sound_chip_register(&sfx_soundsampler_sound_chip);
}

/* ------------------------------------------------------------------------- */

static int sfx_soundsampler_io_swap = 0;

int sfx_soundsampler_cart_enabled(void)
{
    return sfx_soundsampler_sound_chip.chip_enabled;
}

static int set_sfx_soundsampler_enabled(int value, void *param)
{
    int val = value ? 1 : 0;

    if (sfx_soundsampler_sound_chip.chip_enabled != val) {
        if (val) {
            if (export_add(&export_res) < 0) {
                return -1;
            }
            if (machine_class == VICE_MACHINE_VIC20) {
                if (sfx_soundsampler_io_swap) {
                    sfx_soundsampler_io1_device.start_address = 0x9800;
                    sfx_soundsampler_io1_device.end_address = 0x9bff;
                    sfx_soundsampler_io2_device.start_address = 0x9c00;
                    sfx_soundsampler_io2_device.end_address = 0x9fff;
                } else {
                    sfx_soundsampler_io1_device.start_address = 0x9c00;
                    sfx_soundsampler_io1_device.end_address = 0x9fff;
                    sfx_soundsampler_io2_device.start_address = 0x9800;
                    sfx_soundsampler_io2_device.end_address = 0x9bff;
                }
            }
            sfx_soundsampler_io1_list_item = io_source_register(&sfx_soundsampler_io1_device);
            sfx_soundsampler_io2_list_item = io_source_register(&sfx_soundsampler_io2_device);
            sfx_soundsampler_sound_chip.chip_enabled = 1;
            sampler_start(SAMPLER_OPEN_MONO, "SFX Sound Sampler");
        } else {
            export_remove(&export_res);
            io_source_unregister(sfx_soundsampler_io1_list_item);
            io_source_unregister(sfx_soundsampler_io2_list_item);
            sfx_soundsampler_io1_list_item = NULL;
            sfx_soundsampler_io2_list_item = NULL;
            sfx_soundsampler_sound_chip.chip_enabled = 0;
            sampler_stop();
        }
    }
    return 0;
}

static int set_sfx_soundsampler_io_swap(int value, void *param)
{
    int val = value ? 1 : 0;

    if (val == sfx_soundsampler_io_swap) {
        return 0;
    }

    if (sfx_soundsampler_sound_chip.chip_enabled) {
        set_sfx_soundsampler_enabled(0, NULL);
        sfx_soundsampler_io_swap = val;
        set_sfx_soundsampler_enabled(1, NULL);
    } else {
        sfx_soundsampler_io_swap = val;
    }
    return 0;
}

void sfx_soundsampler_reset(void)
{
    /* TODO: do nothing ? */
}

int sfx_soundsampler_enable(void)
{
    return resources_set_int("SFXSoundSampler", 1);
}


int sfx_soundsampler_disable(void)
{
    return resources_set_int("SFXSoundSampler", 0);
}


void sfx_soundsampler_detach(void)
{
    resources_set_int("SFXSoundSampler", 0);
}

/* ------------------------------------------------------------------------- */

static const resource_int_t resources_int[] = {
    { "SFXSoundSampler", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &sfx_soundsampler_sound_chip.chip_enabled, set_sfx_soundsampler_enabled, NULL },
    RESOURCE_INT_LIST_END
};

static const resource_int_t resources_mascuerade_int[] = {
    { "SFXSoundSamplerIOSwap", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &sfx_soundsampler_io_swap, set_sfx_soundsampler_io_swap, NULL },
    RESOURCE_INT_LIST_END
};

int sfx_soundsampler_resources_init(void)
{
    if (machine_class == VICE_MACHINE_VIC20) {
        if (resources_register_int(resources_mascuerade_int) < 0) {
            return -1;
        }
    }
    return resources_register_int(resources_int);
}

void sfx_soundsampler_resources_shutdown(void)
{
}

static const cmdline_option_t cmdline_options[] =
{
    { "-sfxss", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "SFXSoundSampler", (resource_value_t)1,
      NULL, "Enable the SFX Sound Sampler cartridge" },
    { "+sfxss", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "SFXSoundSampler", (resource_value_t)0,
      NULL, "Disable the SFX Sound Sampler cartridge" },
    CMDLINE_LIST_END
};

static const cmdline_option_t cmdline_mascuerade_options[] =
{
    { "-sfxssioswap", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "SFXSoundSamplerIOSwap", (resource_value_t)1,
      NULL, "Swap io mapping (map cart I/O to VIC20 I/O-2)" },
    { "+sfxssioswap", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "SFXSoundSamplerIOSwap", (resource_value_t)0,
      NULL, "Don't swap io mapping (map cart I/O to VIC20 I/O-3)" },
    CMDLINE_LIST_END
};

int sfx_soundsampler_cmdline_options_init(void)
{
    if (machine_class == VICE_MACHINE_VIC20) {
        if (cmdline_register_options(cmdline_mascuerade_options) < 0) {
            return -1;
        }
    }
    return cmdline_register_options(cmdline_options);
}

/* ---------------------------------------------------------------------*/

static void sfx_soundsampler_latch_sample(uint16_t addr, uint8_t value)
{
    current_sample = sampler_get_sample(SAMPLER_CHANNEL_DEFAULT);
}

static uint8_t sfx_soundsampler_sample_read(uint16_t addr)
{
    return current_sample;
}

/* ---------------------------------------------------------------------*/

static uint8_t sfx_soundsampler_sound_data;

static void sfx_soundsampler_sound_store(uint16_t addr, uint8_t value)
{
    sfx_soundsampler_sound_data = value;
    sound_store(sfx_soundsampler_sound_chip_offset, value, 0);
}

struct sfx_soundsampler_sound_s {
    uint8_t voice0;
};

static struct sfx_soundsampler_sound_s snd;

#ifdef SOUND_SYSTEM_FLOAT
/* FIXME */
static int sfx_soundsampler_sound_machine_calculate_samples(sound_t **psid, float *pbuf, int nr, int scc, CLOCK *delta_t)
{
    return sound_dac_calculate_samples(&sfx_soundsampler_dac, pbuf, (int)snd.voice0 * 128, nr);
}
#else
static int sfx_soundsampler_sound_machine_calculate_samples(sound_t **psid, int16_t *pbuf, int nr, int soc, int scc, CLOCK *delta_t)
{
    return sound_dac_calculate_samples(&sfx_soundsampler_dac, pbuf, (int)snd.voice0 * 128, nr, soc, (soc == SOUND_OUTPUT_STEREO) ? SOUND_CHANNELS_1_AND_2 : SOUND_CHANNEL_1);
}
#endif

static int sfx_soundsampler_sound_machine_init(sound_t *psid, int speed, int cycles_per_sec)
{
    sound_dac_init(&sfx_soundsampler_dac, speed);
    snd.voice0 = 0;

    return 1;
}

static void sfx_soundsampler_sound_machine_store(sound_t *psid, uint16_t addr, uint8_t val)
{
    snd.voice0 = val;
}

static uint8_t sfx_soundsampler_sound_machine_read(sound_t *psid, uint16_t addr)
{
    return sfx_soundsampler_sound_data;
}

static void sfx_soundsampler_sound_reset(sound_t *psid, CLOCK cpu_clk)
{
    snd.voice0 = 0;
    sfx_soundsampler_sound_data = 0;
}

/* ---------------------------------------------------------------------*/
/*    snapshot support functions                                             */

/* CARTSFXSS snapshot module format:

   type  | name       | version | description
   ------------------------------------------
   BYTE  | IO swap    |   0.1   | VIC20 I/O swap flag
   BYTE  | sound data |   0.0+  | sound data
 */

static const char snap_module_name[] = "CARTSFXSS";
#define SNAP_MAJOR   0
#define SNAP_MINOR   1

int sfx_soundsampler_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, SNAP_MAJOR, SNAP_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (0
        || SMW_B(m, (uint8_t)sfx_soundsampler_io_swap) < 0
        || SMW_B(m, (uint8_t)sfx_soundsampler_sound_data) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    return snapshot_module_close(m);
}

int sfx_soundsampler_snapshot_read_module(snapshot_t *s)
{
    uint8_t vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, snap_module_name, &vmajor, &vminor);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (snapshot_version_is_bigger(vmajor, vminor, SNAP_MAJOR, SNAP_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    /* new in 0.1 */
    if (!snapshot_version_is_smaller(vmajor, vminor, 0, 1)) {
        if (SMR_B_INT(m, &sfx_soundsampler_io_swap) < 0) {
            goto fail;
        }
    } else {
        sfx_soundsampler_io_swap = 0;
    }

    if (SMR_B(m, &sfx_soundsampler_sound_data) < 0) {
        goto fail;
    }

    if (!sfx_soundsampler_sound_chip.chip_enabled) {
        set_sfx_soundsampler_enabled(1, NULL);
    }
    sound_store(sfx_soundsampler_sound_chip_offset, sfx_soundsampler_sound_data, 0);

    return snapshot_module_close(m);

fail:
    snapshot_module_close(m);
    return -1;
}
