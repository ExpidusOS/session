/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@expidus.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifndef __ESSM_STARTUP_H__
#define __ESSM_STARTUP_H__

#include <esconf/esconf.h>
#include <libexpidus1util/libexpidus1util.h>

void essm_startup_init (EsconfChannel *channel);
void essm_startup_shutdown (void);
void essm_startup_foreign (EssmManager *manager);
void essm_startup_begin (EssmManager *manager);
void essm_startup_session_continue (EssmManager *manager);
gboolean essm_startup_start_properties (EssmProperties *properties,
                                        EssmManager    *manager);

#endif /* !__ESSM_STARTUP_H__ */
