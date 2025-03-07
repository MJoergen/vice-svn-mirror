/*
 * crtctypes.h - A CRTC emulation (under construction)
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andre Fachat <fachat@physik.tu-chemnitz.de>
 *
 * 16/24bpp support added by
 *  Steven Tieu <stieu@physics.ubc.ca>
 *  Teemu Rantanen <tvr@cs.hut.fi>
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

#ifndef VICE_CRTCTYPES_H
#define VICE_CRTCTYPES_H

#include "crtc.h"
#include "raster.h"
#include "types.h"


enum crtc_video_mode_s {
    CRTC_STANDARD_MODE,
    CRTC_REVERSE_MODE,
    CRTC_NUM_VMODES
};
typedef enum crtc_video_mode_s crtc_video_mode_t;

#define CRTC_IDLE_MODE CRTC_STANDARD_MODE

#define CRTC_NUM_COLORS 2

struct alarm_s;
struct video_chip_cap_s;

struct crtc_s {
    /* Flag: Are we initialized?  */
    int initialized;

    /*---------------------------------------------------------------*/

    /* window size computed by crtc_set_screen_options() */
    unsigned int screen_width;
    unsigned int screen_height;

    /* hardware options as given to crtc_set_hw_options() */
    int hw_cursor;
    int hw_cols;        /* 1 or 2, number of chars per cycle */
    int hw_blank;
    int vaddr_mask;
    int vaddr_charswitch;
    int vaddr_charoffset;
    int vaddr_revswitch;

    /* screen and charset memory options (almost) as given to
       crtc_set_chargen_addr() and crtc_set_screen_addr() */
    uint8_t *screen_base;
    uint8_t *chargen_base;
    int chargen_mask;
    int chargen_offset;

    /* those values are derived */
    int chargen_rel;    /* currently used charset rel. to chargen_base */
    int screen_rel;     /* current screen line rel. to screen_base */

    /* internal CRTC state variables */

    int regno;          /* current register selected with store to addr 0 */

    /* The alarm handler is called in the last cycles of a rasterline.
       Some effects need better timing, though */

    /* rasterline variables */
    CLOCK rl_start;     /* clock when the current rasterline starts (left edge of text area) */
    int rl_visible;     /* number of visible chars in this line */
    int rl_sync;        /* character in line when the sync starts */
    int rl_len;         /* length of line in cycles */
    int sync_diff;      /* cycles between two sync pulses */

    /* values of the previous rasterline */
    int prev_rl_visible;
    int prev_rl_sync;
    int prev_rl_len;
    int prev_screen_rel;

    /* internal state */
    int hjitter;        /* horizontal jitter when sync phase is changed */
    int xoffset;        /* pixel-offset of current rasterline */
    int screen_xoffset; /* pixel-offset of current rasterline */
    int screen_hsync;   /* reference position of hsync at top of screen */
    int screen_yoffset; /* rasterline-offset of CRTC to VICE screen */

    int henable;        /* flagged when horizontal enable flipflop has not
                           been reset in line */

    int current_line;   /* current rasterline as of CRTC counting */
    int framelines;     /* expected number of rasterlines for current frame,
                           decreasing till end */
    int venable;        /* flagged when vertical enable flipflop has not
                           been reset in frame */
    bool off_screen;    /* set when the crt ray is considered off-screen */
    int vsync;          /* number of rasterlines till end of vsync */

    int current_charline; /* state of the current character line counter */

    int blank;          /* external blank (only honored if hw_blank set) */

    /* frame */

    CLOCK frame_start;  /* when did the last frame start */
                        /* last frame length */
    long cycles_per_frame;

    /*---------------------------------------------------------------*/

    int crsrmode;       /* 0=no crsr, 1=continous, 2=1/32, 3=1/16 */
    int crsrcnt;        /* framecounter */
    int crsrstate;      /* 1=crsr active */
    int cursor_lines;   /* flagged when rasterline within hw cursor lines */

    /*---------------------------------------------------------------*/

    /* this is the function to be called when the retrace signal
       changes. type&1=0: old PET, type&1=1: CRTC-PET. retrace_type
       Also used by crtc_offscreen() */

    machine_crtc_retrace_signal_t retrace_callback;
    crtc_hires_draw_t hires_draw_callback;

#define CRTC_RETRACE_TYPE_DISCRETE      0
#define CRTC_RETRACE_TYPE_CRTC          1
    int retrace_type;

    /*---------------------------------------------------------------*/

    /* All the CRTC logging goes here.  */
    signed int log;

    /* CRTC raster.  */
    raster_t raster;

    /* CRTC registers.  */
    uint8_t regs[64];

    /* Alarm to update a raster line.  */
    struct alarm_s *raster_draw_alarm;


    /* Video chip capabilities.  */
    struct video_chip_cap_s *video_chip_cap;

#if CRTC_BEAM_RACING
    /*
     * On real 2001s, the retrace interrupt is triggered at the END of the
     * last scan line of the visible text area. The normal raster alarm runs
     * at the start of the next line, which is too late.
     */
    struct alarm_s *adjusted_retrace_alarm;

    uint8_t prefetch[256*2];    /* maximum theoretical size */
#endif

};

/* internal registers */
#define CRTC_REG_HTOTAL         0x00    /* R0 total horizontal characters - 1 */
#define CRTC_REG_HDISP          0x01    /* R1 displayed horizontal characters */
#define CRTC_REG_HSYNC          0x02    /* R2 horizontal sync position */
#define CRTC_REG_SYNCWIDTH      0x03    /* R3 vertical / horizontal sync width */
#define CRTC_REG_VTOTAL         0x04    /* R4 total vertical characters - 1 */
#define CRTC_REG_VTOTALADJ      0x05    /* R5 total vertical lines adjustment */
#define CRTC_REG_VDISP          0x06    /* R6 displayed vertical characters */
#define CRTC_REG_VSYNC          0x07    /* R7 vertical sync position */
#define CRTC_REG_MODECTRL       0x08    /* R8 */
#define CRTC_REG_SCANLINE       0x09    /* R9 scanlines per character row - 1, including spacing */
#define CRTC_REG_CURSORSTART    0x0a    /* R10 */
#define CRTC_REG_CURSOREND      0x0b    /* R11 */
#define CRTC_REG_DISPSTARTH     0x0c    /* R12 */
#define CRTC_REG_DISPSTARTL     0x0d    /* R13 */
#define CRTC_REG_CURSORPOSH     0x0e    /* R14 cursor position */
#define CRTC_REG_CURSORPOSL     0x0f    /* R15 */
#define CRTC_REG_LPENH          0x10    /* R16 lightpen position */
#define CRTC_REG_LPENL          0x11    /* R17 */

typedef struct crtc_s crtc_t;

extern crtc_t crtc;

/* those define the invisible borders, where we never paint anything */
#define CRTC_SCREEN_BORDERWIDTH  8
#define CRTC_SCREEN_BORDERHEIGHT 8

/* These define the extra space around the size given from the machine,
   to allow effects like open borders etc. */
#define CRTC_EXTRA_COLS         6
#define CRTC_EXTRA_RASTERLINES  16

/* Private function calls, used by the other VIC-II modules.  FIXME:
   Prepend names with `_'?  */
int crtc_load_palette(const char *name);
void crtc_resize(void);

#endif
