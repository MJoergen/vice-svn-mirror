/*
 * output-file.c - Output file interface.
 *
 * Written by
 *  Andreas Dehmel <dehmel@forwiss.tu-muenchen.de>
 *  Andreas Boose <viceteam@t-online.de>
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

#include "archdep.h"
#include "cmdline.h"
#include "coproc.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "output-select.h"
#include "output.h"
#include "resources.h"
#include "types.h"
#include "util.h"

#include "output-text.h"

/* #define DEBUG_PRINTER */

#ifdef DEBUG_PRINTER
#define DBG(x)  log_debug x
#else
#define DBG(x)
#endif

static char *PrinterDev[NUM_OUTPUT_SELECT] = { NULL, NULL, NULL };
static int printer_device[NUM_OUTPUT_SELECT];
static FILE *output_fd[NUM_OUTPUT_SELECT] = { NULL, NULL, NULL };

static int set_printer_device_name(const char *val, void *param)
{
    util_string_set(&PrinterDev[vice_ptr_to_int(param)], val);
    return 0;
}

static int set_printer_device(int prn_dev, void *param)
{
    if (prn_dev < 0 || prn_dev > 2) {
        return -1;
    }

    printer_device[vice_ptr_to_int(param)] = (unsigned int)prn_dev;
    return 0;
}

static const resource_string_t resources_string[] = {
    { "PrinterTextDevice1", ARCHDEP_PRINTER_DEFAULT_DEV1,
      RES_EVENT_NO, NULL,
      &PrinterDev[0], set_printer_device_name, (void *)0 },
    { "PrinterTextDevice2", ARCHDEP_PRINTER_DEFAULT_DEV2,
      RES_EVENT_NO, NULL,
      &PrinterDev[1], set_printer_device_name, (void *)1 },
    { "PrinterTextDevice3", ARCHDEP_PRINTER_DEFAULT_DEV3,
      RES_EVENT_NO, NULL,
      &PrinterDev[2], set_printer_device_name, (void *)2 },
    RESOURCE_STRING_LIST_END
};

static const resource_int_t resources_int[] = {
    { "Printer4TextDevice", 0, RES_EVENT_NO, NULL,
      &printer_device[0], set_printer_device, (void *)0 },
    { "Printer5TextDevice", 0, RES_EVENT_NO, NULL,
      &printer_device[1], set_printer_device, (void *)1 },
    { "Printer6TextDevice", 0, RES_EVENT_NO, NULL,
      &printer_device[2], set_printer_device, (void *)2 },
    RESOURCE_INT_LIST_END
};

static const resource_int_t resources_int_userport[] = {
    { "PrinterUserportTextDevice", 0, RES_EVENT_NO, NULL,
      &printer_device[3], set_printer_device, (void *)3 },
    RESOURCE_INT_LIST_END
};

static const cmdline_option_t cmdline_options[] =
{
    { "-prtxtdev1", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "PrinterTextDevice1", NULL,
      "<Name>", "Specify name of printer text device or dump file" },
    { "-prtxtdev2", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "PrinterTextDevice2", NULL,
      "<Name>", "Specify name of printer text device or dump file" },
    { "-prtxtdev3", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "PrinterTextDevice3", NULL,
      "<Name>", "Specify name of printer text device or dump file" },
    { "-pr4txtdev", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "Printer4TextDevice", NULL,
      "<0-2>", "Specify printer text output device for printer #4" },
    { "-pr5txtdev", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "Printer5TextDevice", NULL,
      "<0-2>", "Specify printer text output device for printer #5" },
    { "-pr6txtdev", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "Printer6TextDevice", NULL,
      "<0-2>", "Specify printer text output device for printer #6" },
    CMDLINE_LIST_END
};

static const cmdline_option_t cmdline_options_userport[] =
{
    { "-prusertxtdev", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "PrinterUserportTextDevice", (resource_value_t)0,
      "<0-2>", "Specify printer text output device for userport printer" },
    CMDLINE_LIST_END
};

int output_text_init_cmdline_options(void)
{
    if (machine_class != VICE_MACHINE_C64DTV
        && machine_class != VICE_MACHINE_PLUS4) {
        if (cmdline_register_options(cmdline_options_userport) < 0) {
            return -1;
        }
    }

    return cmdline_register_options(cmdline_options);
}

/* ------------------------------------------------------------------------- */

/*
 * TODO: only do this on systems which support it.
 *
 * 2022-04-03:  On systems where this isn't supported fork_coproc() logs an error
 *              and returns -1. --compyx
 */
static FILE *fopen_or_pipe(char *name)
{
    if (name[0] == '|') {
        int fd_rd, fd_wr;
        if (fork_coproc(&fd_wr, &fd_rd, name + 1) < 0) {
            log_error(LOG_DEFAULT, "fopen_or_pipe(): Cannot fork process '%s'.", name + 1);
            return NULL;
        }
        archdep_close(fd_rd);   /* We only want to write to the process */
        return archdep_fdopen(fd_wr, MODE_WRITE);
    } else {
        return fopen(name, MODE_APPEND);
    }
}

/* ------------------------------------------------------------------------- */

static int output_text_open(unsigned int prnr,
                            output_parameter_t *output_parameter)
{
    switch (printer_device[prnr]) {
        case 0:
        case 1:
        case 2:
            if (PrinterDev[printer_device[prnr]] == NULL) {
                return -1;
            }

            if (output_fd[printer_device[prnr]] == NULL) {
                FILE *fd;
                fd = fopen_or_pipe(PrinterDev[printer_device[prnr]]);
                if (fd == NULL) {
                    return -1;
                }
                output_fd[printer_device[prnr]] = fd;
            }
            return 0;
        default:
            return -1;
    }
}

static void output_text_close(unsigned int prnr)
{
    if (output_fd[printer_device[prnr]] != NULL) {
        fclose(output_fd[printer_device[prnr]]);
    }
    output_fd[printer_device[prnr]] = NULL;
}

static int output_text_putc(unsigned int prnr, uint8_t b)
{
    if (output_fd[printer_device[prnr]] == NULL) {
        return -1;
    }
    fputc(b, output_fd[printer_device[prnr]]);

    return 0;
}

static int output_text_getc(unsigned int prnr, uint8_t *b)
{
    if (output_fd[printer_device[prnr]] == NULL) {
        return -1;
    }
    *b = (uint8_t)fgetc(output_fd[printer_device[prnr]]);
    return 0;
}

static int output_text_flush(unsigned int prnr)
{
    DBG(("output_text_flush:%u", prnr));
    if (output_fd[printer_device[prnr]] == NULL) {
        return -1;
    }

    fflush(output_fd[printer_device[prnr]]);
    return 0;
}

static int output_text_formfeed(unsigned int prnr)
{
    DBG(("output_text_formfeed:%u", prnr));
    return output_text_flush(prnr);
}

/* ------------------------------------------------------------------------- */

int output_text_init_resources(void)
{
    output_select_t output_select;

    output_select.output_name = "text";
    output_select.output_open = output_text_open;
    output_select.output_close = output_text_close;
    output_select.output_putc = output_text_putc;
    output_select.output_getc = output_text_getc;
    output_select.output_flush = output_text_flush;
    output_select.output_formfeed = output_text_formfeed;

    output_select_register(&output_select);

    if (resources_register_string(resources_string) < 0) {
        return -1;
    }

    if (machine_class != VICE_MACHINE_C64DTV
        && machine_class != VICE_MACHINE_PLUS4) {
        if (resources_register_int(resources_int_userport) < 0) {
            return -1;
        }
    }

    return resources_register_int(resources_int);
}

void output_text_shutdown_resources(void)
{
    lib_free(PrinterDev[0]);
    lib_free(PrinterDev[1]);
    lib_free(PrinterDev[2]);
}
