/*
 * menu_plus4cart.c - Implementation of the plus4 cartridge settings menu for the SDL UI.
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
#include "lib.h"
#include "menu_common.h"
#include "menu_plus4cart.h"
#include "plus4cart.h"
#include "resources.h"
#include "ui.h"
#include "uifilereq.h"
#include "uimenu.h"

static UI_MENU_CALLBACK(attach_cart_callback)
{
    char *name = NULL;
    int type = vice_ptr_to_int(param);

    if (activated) {
        name = sdl_ui_file_selection_dialog("Select cartridge image", FILEREQ_MODE_CHOOSE_FILE);
        if (name != NULL) {
            if (cartridge_attach_image(type, name) < 0) {
                ui_error("Cannot load cartridge image.");
            }
            lib_free(name);
        }
    }
    return NULL;
}

static UI_MENU_CALLBACK(detach_cart_callback)
{
    if (activated) {
        cartridge_detach_image(-1);
    }
    return NULL;
}

UI_MENU_DEFINE_RADIO(IOCollisionHandling)

static const ui_menu_entry_t iocollision_menu[] = {
    { "Detach all",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_IOCollisionHandling_callback,
      (ui_callback_data_t)IO_COLLISION_METHOD_DETACH_ALL },
    { "Detach last",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_IOCollisionHandling_callback,
      (ui_callback_data_t)IO_COLLISION_METHOD_DETACH_LAST },
    { "AND values",
      MENU_ENTRY_RESOURCE_RADIO,
      radio_IOCollisionHandling_callback,
      (ui_callback_data_t)IO_COLLISION_METHOD_AND_WIRES },
    SDL_MENU_LIST_END
};

static UI_MENU_CALLBACK(iocollision_show_type_callback)
{
    int type;

    resources_get_int("IOCollisionHandling", &type);
    switch (type) {
        case IO_COLLISION_METHOD_DETACH_ALL:
            return MENU_SUBMENU_STRING " detach all";
            break;
        case IO_COLLISION_METHOD_DETACH_LAST:
            return MENU_SUBMENU_STRING " detach last";
            break;
        case IO_COLLISION_METHOD_AND_WIRES:
            return MENU_SUBMENU_STRING " AND values";
            break;
    }
    return "n/a";
}

UI_MENU_DEFINE_TOGGLE(CartridgeReset)

const ui_menu_entry_t plus4cart_menu[] = {
    { "Attach CRT image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_CRT },
    SDL_MENU_ITEM_SEPARATOR,
    { "Attach raw " CARTRIDGE_PLUS4_NAME_MAGIC " image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_MAGIC },
    { "Attach raw " CARTRIDGE_PLUS4_NAME_MULTI " image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_MULTI },
    { "Attach raw " CARTRIDGE_PLUS4_NAME_JACINT1MB " image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_JACINT1MB },
    SDL_MENU_ITEM_SEPARATOR,
    { "Attach full C1 image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_GENERIC_C1 },
    { "Attach C1 low image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_GENERIC_C1LO },
    { "Attach C1 high image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_GENERIC_C1HI },
    { "Attach full C2 image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_GENERIC_C2 },
    { "Attach C2 low image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_GENERIC_C2LO },
    { "Attach C2 high image",
      MENU_ENTRY_DIALOG,
      attach_cart_callback,
      (ui_callback_data_t)CARTRIDGE_PLUS4_GENERIC_C2HI },
    SDL_MENU_ITEM_SEPARATOR,
    { "Detach cartridge image",
      MENU_ENTRY_OTHER,
      detach_cart_callback,
      NULL },
    SDL_MENU_ITEM_SEPARATOR,
    { "I/O collision handling ($FD00-$FEFF)",
      MENU_ENTRY_SUBMENU,
      iocollision_show_type_callback,
      (ui_callback_data_t)iocollision_menu },
    { "Reset on cartridge change",
      MENU_ENTRY_RESOURCE_TOGGLE,
      toggle_CartridgeReset_callback,
      NULL },
    SDL_MENU_LIST_END
};
