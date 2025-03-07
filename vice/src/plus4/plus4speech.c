/*
 * plus4speech.c - v364 speech support
 *
 * Written by
 *  groepaz <groepaz@gmx.net>
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
#include <string.h>

#include "alarm.h"
#include "archdep.h"
#include "cartio.h"
#include "cmdline.h"
#include "interrupt.h"
#include "lib.h"
#include "log.h"
#include "plus4.h"
#include "plus4cart.h"
#include "plus4mem.h"
#include "plus4speech.h"
#include "maincpu.h"
#include "mon_util.h"
#include "resources.h"
#include "sound.h"
#include "t6721.h"
#include "types.h"
#include "util.h"

/*
   v364 speech support

   - additional 16k ROM
   - MOS8706 mapped at $fd20
   - Toshiba t6721

   xplus4 -speechrom spk3cc4.bin

    say 0 .. N         say a word
    say "word"         say a word
    rate 0 .. 15?      set speed
    voc <addr>         set vocabulary

    http://www.softwolves.com/arkiv/cbm-hackers/4/4482.html

    The V364 speech hardware uses the same Toshiba PARCOR speech synth chip
    (T6721A) as the Magic Voice for the C64, but a more integrated arrangement
    of glue hardware. (MOS 8706 - mapped to $FD2x)

    The speech ROM is in a socket to the left of the BASIC ROM and it occupies
    slot 3 LOW in the ROM banking scheme. The empty socket to the left of that
    is for slot 3 HIGH.

    There is a single custom ASIC for the glue logic to the left of that, so
    the complete listing of V364 28 pin chips from left to right in the line
    goes 8706R0 (custom ASIC), empty (slot 3 HIGH), speech ROM (slot 3 LOW),
    BASIC ROM (slot 0 LOW), KERNAL ROM (slot 0 HIGH), 3 PLUS 1 ROM (slot 1 LOW),
    3 PLUS 1 ROM (slot 1 HIGH), PLA.

    This ASIC replaces the 6525 TIA, 40105 fifo and gate array inside the
    Magic Voice. It has four registers and banks in at $FD2x, which is the
    unused PLA decoded address range (function 0) on all other 264 series
    machines. There are probably four mirrors of the ASIC registers; we did
    not check for this functionality at the time. I took the opportunity to
    obtain this ASIC's pinout, as a first step towards reverse-engineering the
    hardware and maybe one day making a speech attachment for a plus/4 out of
    a modified Magic Voice. Here are the results of the investigation:

    U27 8706R0 (1284) Custom speech glue logic ASIC

    1       RESET#  (cpu)           28      +5V
    2       IRQ#    (cpu)           27      D0      (cpu)
    3       R/W#    (cpu)           26      D0      (t6721a)
    4       phi 0   (cpu)           25      D1      (cpu)
    5       $FD2x#  (cpu)           24      D1      (t6721a)
    6       A0      (cpu)           23      D2      (cpu)
    7       A1      (cpu)           22      D2      (t6721a)
    8       ?       ?               21      D3      (cpu)
    9       EOS#    (t6721a)        20      D3      (t6721a)
    10      APD     (t6721a)        19      D4      (cpu)
    11      phi 2   (t6721a)        18      D5      (cpu)
    12      DI      (t6721a)        17      D6      (cpu)
    13      DTRD    (t6721a)        16      D7      (cpu)
    14      GND                     15      WR#     (t6721)
*/

/* #define SPEECHDEBUG */
/* #define DEBUGIRQ */

#ifdef SPEECHDEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

#ifdef DEBUGIRQ
#define DBGIRQ(x) printf x
#else
#define DBGIRQ(x)
#endif

static t6721_state *t6721; /* context for the t6721 chip */

/* MOS 8706 context */
static uint8_t regs[4];

#define IRQNUM_DTRD     0
#define IRQNUM_EOS      1

/*
  IRQ latch

  irq 0 - ($fd21 bit6) DTRD
  irq 1 - ($fd21 bit7) EOS
*/

int irq_enable = 0; /* FIXME: guessed */
int irq_latch = 0; /* FIXME: guessed */

static int last = 0;

static void latch_trigger(void)
{
    int this = (irq_latch & irq_enable) ? 1 : 0;

    if (last != this) {
        if (this) {
            DBGIRQ(("SPEECH: irq assert latch cause: "));
            DBGIRQ(("%s", (((irq_latch & irq_enable) >> IRQNUM_EOS)) & 1 ? "eos " : ""));
            DBGIRQ(("%s", (((irq_latch & irq_enable) >> IRQNUM_DTRD)) & 1 ? "dtrd " : ""));
            DBGIRQ(("\n"));
            maincpu_set_irq(0, 1);
        } else {
            DBGIRQ(("SPEECH: irq deassert latch\n"));
            maincpu_set_irq(0, 0);
        }
    }
    last = this;
}

/*
    writing to the irq latch mask
    - clears latched irqs
    - sets enable mask
*/
static void latch_set_mask(int mask)
{
    DBG(("SPEECH: latch clear/set mask %x\n", mask));
    irq_enable = mask & 3;
    irq_latch = 0;
    latch_trigger();
}

static void latch_set_irq(int num, int bit)
{
    irq_latch &= ~(1 << num);
    irq_latch |= ((bit & 1) << num);
    latch_trigger();
}

static int latch_load_and_clear(void)
{
    int val = irq_latch;

/*    irq_latch = 0; */
    latch_trigger();

    return val;
}

/*
 the faked FIFO
*/
#define FIFO_LEN        (16)

int readptr, writeptr;
int DTRD = 0;
int datainfifo = 0;
int fifo_reset = 0;
unsigned int fifo_buffer = 0;

static void update_dtrd(int d)
{
#if 0
    if (d) {
        DTRD = 1;
    } else {
        DTRD = 0;

        if (t6721->apd == 0) {
            if (t6721->eos == 0) {
                if (t6721->playing == 1) {
                    if (datainfifo < 4) {
                        DTRD = 1;
                    } else {
                        DTRD = 0;
                    }
                }
            }
        }
    }
    latch_set_irq(IRQNUM_DTRD, DTRD);
#endif
}

/* hooked to callback of t6721 chip */
static uint8_t read_bit_from_fifo(t6721_state *state, unsigned int *bit)
{
    *bit = 0;

    if (datainfifo < 1) {
        update_dtrd(1);
        return 0;
    }

    datainfifo--;
    if (fifo_buffer & (1 << readptr)) {
        *bit = 1;
    }
    readptr++;

    if (readptr == FIFO_LEN) {
        readptr = 0;
    }

    if (datainfifo < 4) {
        update_dtrd(1);
    } else {
        update_dtrd(0);
    }

    return 1;
}

/* writes one bit to the FIFO */
static uint8_t write_bit_to_fifo(uint8_t bit)
{
    if (fifo_reset) {
        /* DBG(("SPEECH: wr first bit: %d\n", bit)); */
        datainfifo = 0;
    }

    /* if dtrd==0, then run 1 tick, which makes dtrd==1 */
    if (t6721->dtrd) {
        t6721_update_ticks(t6721, 1);
    }

    if (datainfifo >= FIFO_LEN) {
        update_dtrd(0);
        return 0;
    }

    if (bit) {
        bit = 1;
    }

    fifo_buffer &= ~(1 << writeptr);
    fifo_buffer |= (bit << writeptr);
    writeptr++;

    datainfifo++;
    fifo_reset = 0; /* unset FIFO reset condition on first written byte */

    if (writeptr == FIFO_LEN) {
        writeptr = 0;
    }

    t6721_update_ticks(t6721, 2); /* run 2 ticks, which gives the chip time to read 1 bit */

    update_dtrd(0);
    return 1;
}

/*
   writes one nibble to the FIFO
*/
static void write_data_nibble(uint8_t nibble)
{
    int i;
    uint8_t mask;
/*    DBG(("%x ", nibble)); */
/*    DBG(("SPEECH: wr byte %04x\n", nibble)); */
    for (i = 0, mask = 1; i < 4; ++i, mask <<= 1) {
        if (write_bit_to_fifo((uint8_t)(nibble & mask)) == 0) {
            return;
        }
    }
}

/* hooked to callback of t6721 chip */
static void set_dtrd(t6721_state *state)
{
    static int old;
    if (old != state->dtrd) {
        DTRD = state->dtrd;
        old = state->dtrd;
        /* DBG(("SPEECH: irq assert dtrd:%x masked:%x\n", DTRD, DTRD & irq_enable)); */
        latch_set_irq(IRQNUM_DTRD, DTRD);
    }
}

/* hooked to callback of t6721 chip */
static void set_apd(t6721_state *state)
{
    if (state->apd) {
        fifo_reset = 1; /* set FIFO reset condition */

        /* reset FIFO */
        writeptr = 0;
        readptr = 0;
        datainfifo = 0;

        update_dtrd(0);
    }
}

/* hooked to callback of t6721 chip */
static void set_eos(t6721_state *state)
{
    static int lst;

    if (lst != state->eos) {
        DBG(("SPEECH: set EOS: %d\n", state->eos));
        latch_set_irq(IRQNUM_EOS, state->eos);
    }
    lst = state->eos;
}

/*
MOS8706 Registers

$FD20 : command register

    read:
    - the v364 software doesn't read this register

    store:

    bit0..3 - t6721 D0..D3 (command code)
    bit4 - unused
    bit5 - unused
    bit6 - unused
    bit7 - t6721 WR line (latch command on 0->1)

$FD21 : status and IRQ latch register

    read:

    bit0 - irq ?
    bit1 - irq ?
    bit2 - unused
    bit3 - unused
    bit4 - unused
    bit5 - unused
    bit6 : EOS' (end of speech) from the T6721 (stop speech if =0)
    bit7 : DTRD (while =1 in loop that writes data to fd22)

    store: (only values 0 and 3 used)

    bit0 - irq enable ?
    bit1 - irq enable ?
    bit2 - unused
    bit3 - unused
    bit4 - unused
    bit5 - unused
    bit6 - unused
    bit7 - unused

$FD22 : speech data register

    read:
    - the v364 software doesn't read this register

    store: speech data
*/

static uint8_t speech_read(uint16_t addr)
{
    uint8_t value = 0;
    /* DBG(("SPEECH: rd %04x\n", addr)); */
    switch (addr & 3) {
        case 0:
            /* the v364 software does not read this register */
            DBG(("SPEECH: <FIXME> rd cmd\n"));
            /* value = t6721_read(t6721); */
            t6721_update_ticks(t6721, 1);
            break;
        case 1:
            /* DBG(("SPEECH: rd status %04x\n", addr)); */
            value |= latch_load_and_clear();
            value |= ((t6721->eos ^ 1) << 6); /* EOS */
            value |= ((DTRD) << 7); /* DTRD */
            t6721_update_ticks(t6721, 1);
            break;
        case 2:
            /* the v364 software does not read this register */
            DBG(("SPEECH: <FIXME> rd data\n"));
            t6721_update_ticks(t6721, 1);
            break;
    }
    return value;
}

static void speech_store(uint16_t addr, uint8_t value)
{
    /* DBG(("SPEECH: wr %04x %02x\n", addr, value)); */
    switch (addr & 3) {
        case 0: /* Command register */
                /* DBG(("SPEECH: wr cmd %02x\n", value & 0x0f)); */
            t6721->wr = (value >> 7) & 1;     /* wr line */
            t6721_store(t6721, (uint8_t)(value & 0x0f));
            t6721_update_ticks(t6721, 1);
            regs[0] = value;
            break;
        case 1: /* IRQ latch register ? */
                /* v364 code uses only values 0 and 3
                   0 - irq acknowledge ?
                   3 - ?
                */
                /* DBG(("SPEECH: wr status %02x\n", value)); */
            latch_set_mask(value & 3);
            t6721_update_ticks(t6721, 1);
            regs[1] = value;
            break;
        case 2: /* sample data register */
            write_data_nibble((uint8_t)((value >> 0) & 0x0f));
            write_data_nibble((uint8_t)((value >> 4) & 0x0f));
            regs[2] = value;
            break;
    }
}

static uint8_t speech_peek(uint16_t addr)
{
    return regs[addr & 3];
}

static int speech_dump(void)
{
    mon_out("MOS8706:\n");
    mon_out("0 Command:     %02x\n", regs[0]);
    mon_out("1 IRQ Latch:   %02x\n", regs[1]);
    mon_out("2 Speech Data: %02x\n", regs[2]);
    mon_out("T6721:\n");
    t6721_dump(t6721);
    return 0;
}

void speech_setup_context(machine_context_t *machine_ctx)
{
    DBG(("SPEECH: speech_setup_context\n"));
    /* init t6721 chip */
    t6721 = lib_calloc(1, sizeof(t6721_state));
    t6721->read_data = read_bit_from_fifo;
    t6721->set_apd = set_apd;
    t6721->set_eos = set_eos;
    t6721->set_dtrd = set_dtrd;
    t6721_reset(t6721);
}

/* ------------------------------------------------------------------------- */

/* Some prototypes are needed */
static int speech_sound_machine_init(sound_t *psid, int speed, int cycles_per_sec);

#ifdef SOUND_SYSTEM_FLOAT
static int speech_sound_machine_calculate_samples(sound_t **psid, float *pbuf, int nr, int sound_chip_channels, CLOCK *delta_t);
#else
static int speech_sound_machine_calculate_samples(sound_t **psid, int16_t *pbuf, int nr, int sound_output_channels, int sound_chip_channels, CLOCK *delta_t);
#endif

static int speech_sound_machine_cycle_based(void)
{
    return 0;
}

static int speech_sound_machine_channels(void)
{
    return 1;
}

#ifdef SOUND_SYSTEM_FLOAT
/* stereo mixing placement of the V364 speech sound */
static sound_chip_mixing_spec_t speech_sound_mixing_spec[SOUND_CHIP_CHANNELS_MAX] = {
    {
        100, /* left channel volume % in case of stereo output, default output to both */
        100  /* right channel volume % in case of stereo output, default output to both */
    }
};
#endif

/* V364 speech sound chip */
static sound_chip_t speech_sound_chip = {
    NULL,                                   /* NO sound chip open function */
    speech_sound_machine_init,              /* sound chip init function */
    NULL,                                   /* NO sound chip close function */
    speech_sound_machine_calculate_samples, /* sound chip calculate samples function */
    NULL,                                   /* NO sound chip store function */
    NULL,                                   /* NO sound chip read function */
    NULL,                                   /* NO sound chip reset function */
    speech_sound_machine_cycle_based,       /* sound chip 'is_cycle_based()' function, chip is NOT cycle based */
    speech_sound_machine_channels,          /* sound chip 'get_amount_of_channels()' function, sound chip has 1 channel */
#ifdef SOUND_SYSTEM_FLOAT
    speech_sound_mixing_spec,               /* stereo mixing placement specs */
#endif
    0                                       /* sound chip enabled flag, toggled upon device (de-)activation */
};

static uint16_t speech_sound_chip_offset = 0;

void speech_sound_chip_init(void)
{
    speech_sound_chip_offset = sound_chip_register(&speech_sound_chip);
}

int speech_cart_enabled(void)
{
    return speech_sound_chip.chip_enabled;
}

/* ------------------------------------------------------------------------- */

const char *speech_filename = NULL;

static io_source_t speech_device = {
    "V364SPEECH",         /* name of the device */
    IO_DETACH_RESOURCE,   /* use resource to detach the device when involved in a read-collision */
    "SpeechEnabled",      /* resource to set to '0' */
    0xfd20, 0xfd22, 0x03, /* range for the device, regs:$fd20-$fd22 */
    1,                    /* read is always valid */
    speech_store,         /* store function */
    NULL,                 /* NO poke function */
    speech_read,          /* read function */
    speech_peek,          /* peek function */
    speech_dump,          /* chip state information dump function */
    IO_CART_ID_NONE,      /* not a cartridge */
    IO_PRIO_NORMAL,       /* normal priority, device read needs to be checked for collisions */
    0                     /* insertion order, gets filled in by the registration function */
};

static io_source_list_t *speech_list_item = NULL;


static int set_speech_enabled(int value, void *param)
{
    int val = value ? 1 : 0;

    if (val == speech_sound_chip.chip_enabled) {
        return 0;
    }

    if (val) {
        resources_get_string("c2loname", &speech_filename);
        if (speech_filename) {
            if (*speech_filename) {
                speech_sound_chip.chip_enabled = 1;
                speech_list_item = io_source_register(&speech_device);
            }
        }
    } else {
        speech_sound_chip.chip_enabled = 0;
        io_source_unregister(speech_list_item);
        speech_list_item = NULL;
    }

    DBG(("speech_set_enabled: '%s' %d : %d\n", speech_filename, val, speech_enabled));
    return 0;
}

static const resource_int_t resources_int[] = {
    { "SpeechEnabled", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &speech_sound_chip.chip_enabled, set_speech_enabled, NULL },
    RESOURCE_INT_LIST_END
};

int speech_resources_init(void)
{
    return resources_register_int(resources_int);
}

void speech_resources_shutdown(void)
{
    speech_filename = NULL;
}

void speech_shutdown(void)
{
    if (t6721) {
        lib_free(t6721);
        t6721 = NULL;
    }
}

static const cmdline_option_t cmdline_options[] =
{
    { "-speech", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "SpeechEnabled", (resource_value_t)1,
      NULL, "Enable the v364 speech add-on" },
    { "+speech", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "SpeechEnabled", (resource_value_t)0,
      NULL, "Disable the v364 speech add-on" },
    CMDLINE_LIST_END
};

int speech_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}


/* ---------------------------------------------------------------------*/

/* FIXME: shutdown missing */

/*
    called periodically for every sound fragment that is played
*/
#ifdef SOUND_SYSTEM_FLOAT
/* FIXME */
static int speech_sound_machine_calculate_samples(sound_t **psid, float *pbuf, int nr, int scc, CLOCK *delta_t)
{
    int i;
    float *buffer;

    buffer = lib_malloc(nr * sizeof(float));

    t6721_update_output(t6721, buffer, nr);

    /* mix generated samples to output */
    for (i = 0; i < nr; i++) {
        pbuf[i] = buffer[i];
    }

    lib_free(buffer);

    return nr;
}
#else
static int speech_sound_machine_calculate_samples(sound_t **psid, int16_t *pbuf, int nr, int soc, int scc, CLOCK *delta_t)
{
    int i;
    int16_t *buffer;

    buffer = lib_malloc(nr * 2);

    t6721_update_output(t6721, buffer, nr);

    /* mix generated samples to output */
    for (i = 0; i < nr; i++) {
        pbuf[i * soc] = sound_audio_mix(pbuf[i * soc], buffer[i]);
        if (soc == SOUND_OUTPUT_STEREO) {
            pbuf[(i * soc) + 1] = sound_audio_mix(pbuf[(i * soc) + 1], buffer[i]);
        }
    }

    lib_free(buffer);

    return nr;
}
#endif

static int speech_sound_machine_init(sound_t *psid, int speed, int cycles_per_sec)
{
    DBG(("SPEECH: speech_sound_machine_init: speed %d cycles/sec: %d\n", speed, cycles_per_sec));
    if (cycles_per_sec == PLUS4_PAL_CYCLES_PER_SEC) {
        t6721_sound_machine_init_vbr(t6721, speed, cycles_per_sec, 1800);
    } else {
        t6721_sound_machine_init_vbr(t6721, speed, cycles_per_sec, 1750);
    }

    return 1;
}
