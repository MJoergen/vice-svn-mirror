/*
 * menu_midi.c - MIDI menu for SDL UI.
 *
 * Written by
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

#include "vice.h"

#ifdef HAVE_MIDI

#include <stdio.h>

#include "types.h"

#include "menu_common.h"
#include "menu_midi.h"
#include "uimenu.h"

#include "cartridge.h"
#include "mididrv.h"
#include "c64-midi.h"

/* *nix MIDI settings */
#if defined(UNIX_COMPILE) && !defined(MACOS_COMPILE)

void sdl_menu_midi_in_free(void)
{
}

void sdl_menu_midi_out_free(void)
{
}

#if defined(USE_OSS)
UI_MENU_DEFINE_STRING(MIDIInDev)
UI_MENU_DEFINE_STRING(MIDIOutDev)
#endif

#if defined(USE_ALSA)
UI_MENU_DEFINE_STRING(MIDIName)
#endif

/* only show driver/device items when a midi driver exists */
#if defined(USE_OSS) && defined (USE_ALSA)
UI_MENU_DEFINE_RADIO(MIDIDriver)

static const ui_menu_entry_t midi_driver_menu[] = {
    { "OSS",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIDriver_callback,
      (ui_callback_data_t)MIDI_DRIVER_OSS },
    { "ALSA",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIDriver_callback,
      (ui_callback_data_t)MIDI_DRIVER_ALSA },
    SDL_MENU_LIST_END
};

#define VICE_SDL_MIDI_ARCHDEP_ITEMS            \
    SDL_MENU_ITEM_SEPARATOR,                   \
    { "Driver",                                \
      MENU_ENTRY_SUBMENU,                      \
      submenu_radio_callback,                  \
      (ui_callback_data_t)midi_driver_menu },  \
    SDL_MENU_ITEM_SEPARATOR,                   \
    SDL_MENU_ITEM_TITLE("ALSA client"),        \
    { "Name",                                  \
      MENU_ENTRY_RESOURCE_STRING,              \
      string_MIDIName_callback,                \
      (ui_callback_data_t)"ALSA client name" },\
    SDL_MENU_ITEM_SEPARATOR,                   \
    SDL_MENU_ITEM_TITLE("OSS driver devices"), \
    { "MIDI-In",                               \
      MENU_ENTRY_RESOURCE_STRING,              \
      string_MIDIInDev_callback,               \
      (ui_callback_data_t)"MIDI-In device" },  \
    { "MIDI-Out",                              \
      MENU_ENTRY_RESOURCE_STRING,              \
      string_MIDIOutDev_callback,              \
      (ui_callback_data_t)"MIDI-Out device" },

#elif defined (USE_ALSA)

#define VICE_SDL_MIDI_ARCHDEP_ITEMS            \
    SDL_MENU_ITEM_SEPARATOR,                   \
    SDL_MENU_ITEM_TITLE("ALSA client"),        \
    { "Name",                                  \
      MENU_ENTRY_RESOURCE_STRING,              \
      string_MIDIName_callback,                \
      (ui_callback_data_t)"ALSA client name" },

#elif defined(USE_OSS)

#define VICE_SDL_MIDI_ARCHDEP_ITEMS            \
    SDL_MENU_ITEM_SEPARATOR,                   \
    SDL_MENU_ITEM_TITLE("OSS driver devices"), \
    { "MIDI-In",                               \
      MENU_ENTRY_RESOURCE_STRING,              \
      string_MIDIInDev_callback,               \
      (ui_callback_data_t)"MIDI-In device" },  \
    { "MIDI-Out",                              \
      MENU_ENTRY_RESOURCE_STRING,              \
      string_MIDIOutDev_callback,              \
      (ui_callback_data_t)"MIDI-Out device" },

#else
#define VICE_SDL_MIDI_ARCHDEP_ITEMS
#endif

#endif /* defined(UNIX_COMPILE) && !defined(MACOS_COMPILE) */

/* OSX MIDI settings */
#ifdef MACOS_COMPILE
UI_MENU_DEFINE_STRING(MIDIName)
UI_MENU_DEFINE_STRING(MIDIInName)
UI_MENU_DEFINE_STRING(MIDIOutName)

void sdl_menu_midi_in_free(void)
{
}

void sdl_menu_midi_out_free(void)
{
}

#define VICE_SDL_MIDI_ARCHDEP_ITEMS                 \
    SDL_MENU_ITEM_SEPARATOR,                        \
    { "Client name",                                \
      MENU_ENTRY_RESOURCE_STRING,                   \
      string_MIDIName_callback,                     \
      (ui_callback_data_t)"Name of MIDI client" },  \
    { "MIDI-In",                                    \
      MENU_ENTRY_RESOURCE_STRING,                   \
      string_MIDIInName_callback,                   \
      (ui_callback_data_t)"Name of MIDI-In Port" }, \
    { "MIDI-Out",                                   \
      MENU_ENTRY_RESOURCE_STRING,                   \
      string_MIDIOutName_callback,                  \
      (ui_callback_data_t)"Name of MIDI-Out Port" },

#endif /* defined(MACOS_COMPILE) */

/* win32 MIDI settings */
#ifdef WINDOWS_COMPILE

#include <windows.h>
#include <mmsystem.h>

#include "lib.h"

UI_MENU_DEFINE_RADIO(MIDIInDev)
UI_MENU_DEFINE_RADIO(MIDIOutDev)

static ui_menu_entry_t midi_in_dyn_menu[21];
static ui_menu_entry_t midi_out_dyn_menu[21];

static int midi_in_dyn_menu_init = 0;
static int midi_out_dyn_menu_init = 0;

void sdl_menu_midi_in_free(void)
{
    int i;

    for (i = 0; midi_in_dyn_menu[i].string != NULL; i++) {
        lib_free(midi_in_dyn_menu[i].string);
    }
}

void sdl_menu_midi_out_free(void)
{
    int i;

    for (i = 0; midi_out_dyn_menu[i].string != NULL; i++) {
        lib_free(midi_out_dyn_menu[i].string);
    }
}

UI_MENU_CALLBACK(MIDIInDev_dynmenu_callback)
{
    MMRESULT ret;
    MIDIINCAPS mic;
    int i = 0;
    int j;
    int num_in = midiInGetNumDevs();

    /* rebuild menu if it already exists. */
    if (midi_in_dyn_menu_init != 0) {
        sdl_menu_midi_in_free();
    } else {
        midi_in_dyn_menu_init = 1;
    }

    if (num_in == 0) {
        midi_in_dyn_menu[i].string = (char *)lib_strdup("No Devices Present");
        midi_in_dyn_menu[i].type = MENU_ENTRY_TEXT;
        midi_in_dyn_menu[i].callback = seperator_callback;
        midi_in_dyn_menu[i].data = NULL;
        i++;
    } else {
        for (j = 0; (j < num_in) && (i < 20); j++) {
            ret = midiInGetDevCaps(j, &mic, sizeof(MIDIINCAPS));
            if (ret == MMSYSERR_NOERROR) {
                midi_in_dyn_menu[i].string = (char *)lib_strdup(mic.szPname);
                midi_in_dyn_menu[i].type = MENU_ENTRY_RESOURCE_RADIO;
                midi_in_dyn_menu[i].callback = radio_MIDIInDev_callback;
                midi_in_dyn_menu[i].data = (ui_callback_data_t)(int_to_void_ptr(j));
                i++;
            }
        }
    }

    midi_in_dyn_menu[i].string = NULL;
    midi_in_dyn_menu[i].type = 0;
    midi_in_dyn_menu[i].callback = NULL;
    midi_in_dyn_menu[i].data = NULL;

    return MENU_SUBMENU_STRING;
}

UI_MENU_CALLBACK(MIDIOutDev_dynmenu_callback)
{
    MMRESULT ret;
    MIDIOUTCAPS moc;
    int i = 0;
    int j;
    int num_out = midiOutGetNumDevs();

    /* rebuild menu if it already exists. */
    if (midi_out_dyn_menu_init != 0) {
        sdl_menu_midi_out_free();
    } else {
        midi_out_dyn_menu_init = 1;
    }

    if (num_out == 0) {
        midi_out_dyn_menu[i].string = (char *)lib_strdup("No Devices Present");
        midi_out_dyn_menu[i].type = MENU_ENTRY_TEXT;
        midi_out_dyn_menu[i].callback = seperator_callback;
        midi_out_dyn_menu[i].data = NULL;
        i++;
    } else {
        for (j = 0; (j < num_out) && (i < 20); j++) {
            ret = midiOutGetDevCaps(j, &moc, sizeof(MIDIOUTCAPS));
            if (ret == MMSYSERR_NOERROR) {
                midi_out_dyn_menu[i].string = (char *)lib_strdup(moc.szPname);
                midi_out_dyn_menu[i].type = MENU_ENTRY_RESOURCE_RADIO;
                midi_out_dyn_menu[i].callback = radio_MIDIOutDev_callback;
                midi_out_dyn_menu[i].data = (ui_callback_data_t)(int_to_void_ptr(j));
                i++;
            }
        }
    }

    midi_out_dyn_menu[i].string = NULL;
    midi_out_dyn_menu[i].type = 0;
    midi_out_dyn_menu[i].callback = NULL;
    midi_out_dyn_menu[i].data = NULL;

    return MENU_SUBMENU_STRING;
}

#define VICE_SDL_MIDI_ARCHDEP_ITEMS           \
    SDL_MENU_ITEM_SEPARATOR,                  \
    { "MIDI-In",                              \
      MENU_ENTRY_DYNAMIC_SUBMENU,             \
      MIDIInDev_dynmenu_callback,             \
      (ui_callback_data_t)midi_in_dyn_menu }, \
    { "MIDI-Out",                             \
      MENU_ENTRY_DYNAMIC_SUBMENU,             \
      MIDIOutDev_dynmenu_callback,            \
      (ui_callback_data_t)midi_out_dyn_menu },

#endif /* defined(WINDOWS_COMPILE) */

/* Common menus */

UI_MENU_DEFINE_TOGGLE(MIDIEnable)
UI_MENU_DEFINE_RADIO(MIDIMode)

static const ui_menu_entry_t midi_type_menu[] = {
    { CARTRIDGE_NAME_MIDI_SEQUENTIAL,
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIMode_callback,
      (ui_callback_data_t)MIDI_MODE_SEQUENTIAL },
    { CARTRIDGE_NAME_MIDI_PASSPORT,
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIMode_callback,
      (ui_callback_data_t)MIDI_MODE_PASSPORT },
    { CARTRIDGE_NAME_MIDI_DATEL,
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIMode_callback,
      (ui_callback_data_t)MIDI_MODE_DATEL },
    { CARTRIDGE_NAME_MIDI_NAMESOFT,
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIMode_callback,
      (ui_callback_data_t)MIDI_MODE_NAMESOFT },
    { CARTRIDGE_NAME_MIDI_MAPLIN,
      MENU_ENTRY_RESOURCE_RADIO,
      radio_MIDIMode_callback,
      (ui_callback_data_t)MIDI_MODE_MAPLIN },
    SDL_MENU_LIST_END
};

const ui_menu_entry_t midi_c64_menu[] = {
    { "MIDI emulation",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_MIDIEnable_callback,
      NULL },
    { "MIDI cart type",
      MENU_ENTRY_SUBMENU,
      submenu_radio_callback,
      (ui_callback_data_t)midi_type_menu },
    VICE_SDL_MIDI_ARCHDEP_ITEMS
    SDL_MENU_LIST_END
};

const ui_menu_entry_t midi_vic20_menu[] = {
    { "MIDI emulation",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_MIDIEnable_callback,
      NULL },
    VICE_SDL_MIDI_ARCHDEP_ITEMS
    SDL_MENU_LIST_END
};

#endif /* HAVE_MIDI */
