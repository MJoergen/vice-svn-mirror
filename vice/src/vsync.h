/*
 * vsyn.h - Common vsync API.
 *
 * Written by
 *  Andreas Boose <boose@linux.rz.fh-hannover.de>
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

#ifndef _VSYNC_H
#define _VSYNC_H

typedef void (*void_hook_t)(void);

extern void suspend_speed_eval(void);
extern int vsync_init_resources(void);
extern int vsync_init_cmdline_options(void);
extern void vsync_init(void (*hook)(void));
extern void vsync_set_machine_parameter(double refresh_rate, long cycles);
extern int do_vsync(int been_skipped);
extern int vsync_disable_timer(void);
extern void_hook_t vsync_set_event_dispatcher(void_hook_t hook);

#endif

