/*
 * menu_plus4hw.c - PLUS4 HW menu for SDL UI.
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

#include "types.h"

#include "machine.h"
#include "menu_common.h"
#include "menu_joyport.h"
#include "menu_joystick.h"
#include "menu_mouse.h"
#include "menu_ram.h"
#include "menu_rom.h"
#include "menu_settings.h"
#include "menu_userport.h"
#include "pet.h"
#include "petmodel.h"
#include "pet-resources.h"

#if defined(HAVE_RS232DEV) || defined(HAVE_RS232NET)
#include "menu_rs232.h"
#endif

#include "menu_sid.h"
#include "menu_tape.h"
#include "pets.h"
#include "uimenu.h"

/* PET VIDEO SETTINGS */

/* PET MEMORY SETTINGS */

UI_MENU_DEFINE_RADIO(RamSize)
UI_MENU_DEFINE_RADIO(IOSize)
UI_MENU_DEFINE_TOGGLE(SuperPET)
UI_MENU_DEFINE_TOGGLE(Ram9)
UI_MENU_DEFINE_TOGGLE(RamA)

static const ui_menu_entry_t pet_memory_menu[] = {
    SDL_MENU_ITEM_TITLE("Memory size"),
    { "4KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_RamSize_callback,
      (ui_callback_data_t)4 },
    { "8KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_RamSize_callback,
      (ui_callback_data_t)8 },
    { "16KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_RamSize_callback,
      (ui_callback_data_t)16 },
    { "32KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_RamSize_callback,
      (ui_callback_data_t)32 },
    { "96KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_RamSize_callback,
      (ui_callback_data_t)96 },
    { "128KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_RamSize_callback,
      (ui_callback_data_t)128 },
    SDL_MENU_ITEM_SEPARATOR,
    SDL_MENU_ITEM_TITLE("I/O size"),
    { "256 bytes",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_IOSize_callback,
      (ui_callback_data_t)256 },
    { "2KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_IOSize_callback,
      (ui_callback_data_t)2048 },
    SDL_MENU_ITEM_SEPARATOR,
    SDL_MENU_ITEM_TITLE("8296 memory blocks"),
    { "$9xxx as RAM",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_Ram9_callback,
      NULL },
    { "$Axxx as RAM",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_RamA_callback,
      NULL },
    SDL_MENU_ITEM_SEPARATOR,
    SDL_MENU_ITEM_TITLE("SUPERPET I/O"),
    { "Enable SUPERPET I/O (disables 8x96)",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_SuperPET_callback,
      NULL },
    SDL_MENU_LIST_END
};

/* PETREU */

UI_MENU_DEFINE_TOGGLE(PETREU)
UI_MENU_DEFINE_RADIO(PETREUsize)
UI_MENU_DEFINE_FILE_STRING(PETREUfilename)

static const ui_menu_entry_t petreu_menu[] = {
    { "Enable PET REU",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_PETREU_callback,
      NULL },
    SDL_MENU_ITEM_SEPARATOR,
    SDL_MENU_ITEM_TITLE("Memory size"),
    { "128KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETREUsize_callback,
      (ui_callback_data_t)128 },
    { "512KiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETREUsize_callback,
      (ui_callback_data_t)512 },
    { "1MiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETREUsize_callback,
      (ui_callback_data_t)1024 },
    { "2MiB",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETREUsize_callback,
      (ui_callback_data_t)2048 },
    SDL_MENU_ITEM_SEPARATOR,
    SDL_MENU_ITEM_TITLE("RAM image"),
    { "PET REU image file",
      MENU_ENTRY_DIALOG,
      file_string_PETREUfilename_callback,
      (ui_callback_data_t)"Select PET REU image" },
    SDL_MENU_LIST_END
};

/* PETDWW */

UI_MENU_DEFINE_TOGGLE(PETDWW)
UI_MENU_DEFINE_FILE_STRING(PETDWWfilename)

static const ui_menu_entry_t petdww_menu[] = {
    { "Enable PET DWW",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_PETDWW_callback,
      NULL },
    SDL_MENU_ITEM_SEPARATOR,
    SDL_MENU_ITEM_TITLE("RAM image"),
    { "PET DWW image file",
      MENU_ENTRY_DIALOG,
      file_string_PETDWWfilename_callback,
      (ui_callback_data_t)"Select PET DWW image" },
    SDL_MENU_LIST_END
};

/* PETCOLOUR */

UI_MENU_DEFINE_RADIO(PETColour)
UI_MENU_DEFINE_SLIDER(PETColourBG, 0, 255)

static const ui_menu_entry_t petcolour_menu[] = {
    SDL_MENU_ITEM_TITLE("PET Colour type"),
    { "Off",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETColour_callback,
      (ui_callback_data_t)PET_COLOUR_TYPE_OFF },
    { "RGBI",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETColour_callback,
      (ui_callback_data_t)PET_COLOUR_TYPE_RGBI },
    { "Analog",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_PETColour_callback,
      (ui_callback_data_t)PET_COLOUR_TYPE_ANALOG },
    SDL_MENU_ITEM_SEPARATOR,
    { "PET Colour background",
      MENU_ENTRY_RESOURCE_INT,
      slider_PETColourBG_callback,
      (ui_callback_data_t)"Set PET Colour background (0-255)" },
    SDL_MENU_LIST_END
};

/* SUPERPET CPU */

UI_MENU_DEFINE_RADIO(CPUswitch)

static const ui_menu_entry_t superpet_cpu_menu[] = {
    SDL_MENU_ITEM_TITLE("SuperPET CPU switch"),
    { "MOS 6502",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_CPUswitch_callback,
      (ui_callback_data_t)SUPERPET_CPU_6502 },
    { "Motorola 6809",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_CPUswitch_callback,
      (ui_callback_data_t)SUPERPET_CPU_6809 },
    { "Programmable",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_CPUswitch_callback,
      (ui_callback_data_t)SUPERPET_CPU_PROG },
    SDL_MENU_LIST_END
};

/* PET MODEL SELECTION */

static UI_MENU_CALLBACK(custom_PETModel_callback)
{
    int model, selected;

    selected = vice_ptr_to_int(param);

    if (activated) {
        petmodel_set(selected);
    } else {
        model = petmodel_get();

        if (selected == model) {
            return sdl_menu_text_tick;
        }
    }

    return NULL;
}

static const ui_menu_entry_t pet_model_menu[] = {
    { "2001", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_2001 },
    { "3008", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_3008 },
    { "3016", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_3016 },
    { "3032", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_3032 },
    { "3032B", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_3032B },
    { "4016", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_4016 },
    { "4032", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_4032 },
    { "4032B", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_4032B },
    { "8032", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_8032 },
    { "8096", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_8096 },
    { "8296", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_8296 },
    { "Super PET", MENU_ENTRY_RESOURCE_RADIO, custom_PETModel_callback, (ui_callback_data_t)PETMODEL_SUPERPET },
    SDL_MENU_LIST_END
};

/* FIXME */
#if 0
void uikeyboard_update_pet_type_menu(void)
{
    int idx, type, mapping;

    resources_get_int("KeymapIndex", &idx);
    resources_get_int("KeyboardMapping", &mapping);
}
#endif

static UI_MENU_CALLBACK(radio_KeyboardType_callback)
{
    const char *res = sdl_ui_menu_radio_helper(activated, param, "KeyboardType");
    if (activated) {
        uikeyboard_update_index_menu();
        uikeyboard_update_mapping_menu();
    }
    return res;
}

/* FIXME: this should be dynamic/generated */
static const ui_menu_entry_t pet_keyboard_menu[] = {
    { "Business (UK)",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_KeyboardType_callback,
      (ui_callback_data_t)KBD_TYPE_BUSINESS_UK },
    { "Business (US)",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_KeyboardType_callback,
      (ui_callback_data_t)KBD_TYPE_BUSINESS_US },
    { "Business (DE)",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_KeyboardType_callback,
      (ui_callback_data_t)KBD_TYPE_BUSINESS_DE },
    { "Business (JP)",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_KeyboardType_callback,
      (ui_callback_data_t)KBD_TYPE_BUSINESS_JP },
    { "Graphics (US)",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_KeyboardType_callback,
      (ui_callback_data_t)KBD_TYPE_GRAPHICS_US },
    SDL_MENU_LIST_END
};

UI_MENU_DEFINE_TOGGLE(Crtc)
UI_MENU_DEFINE_TOGGLE(PETHRE)

const ui_menu_entry_t pet_hardware_menu[] = {
    { "Select PET model",
      MENU_ENTRY_SUBMENU,
      submenu_radio_callback,
      (ui_callback_data_t)pet_model_menu },
    { "Keyboard",
      MENU_ENTRY_SUBMENU,
      submenu_radio_callback,
      (ui_callback_data_t)pet_keyboard_menu },
    { "SuperPET CPU switch",
      MENU_ENTRY_SUBMENU,
      submenu_radio_callback,
      (ui_callback_data_t)superpet_cpu_menu },
    SDL_MENU_ITEM_SEPARATOR,
    { "Joyport settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)joyport_menu },
    { "Joystick settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)joystick_userport_only_menu },
#ifdef HAVE_MOUSE
    { "Mouse emulation",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)mouse_grab_menu },
#endif
    { "SID cart settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)sid_pet_menu },
    { "RAM pattern settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)ram_menu },
    { "ROM settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)pet_rom_menu },
    { "PET REU settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)petreu_menu },
    { "PET DWW settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)petdww_menu },
    { "PET Colour settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)petcolour_menu },
    { "Enable PET High Res Emulation board",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_PETHRE_callback,
      NULL },
    { "Userport settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)userport_menu },
    { "Tape port devices",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)tapeport_pet_devices_menu },
    { "Memory and I/O settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)pet_memory_menu },
    { "CRTC chip enable",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_Crtc_callback,
      NULL },
#if defined(HAVE_RS232DEV) || defined(HAVE_RS232NET)
    { "RS232 settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)rs232_nouser_menu },
#endif
    SDL_MENU_LIST_END
};
