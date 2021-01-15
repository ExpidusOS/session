/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@expidus.org>
 * Copyright (c) 2008 Jannis Pohlmann <jannis@expidus.org>
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

#ifndef __ESAE_WINDOW_H__
#define __ESAE_WINDOW_H__

#include "esae-model.h"

G_BEGIN_DECLS;

typedef struct _EsaeWindowClass EsaeWindowClass;
typedef struct _EsaeWindow      EsaeWindow;

#define ESAE_TYPE_WINDOW            (esae_window_get_type ())
#define ESAE_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESAE_TYPE_WINDOW, EsaeWindow))
#define ESAE_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ESAE_TYPE_WINDOW, EsaeWindow))
#define ESAE_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESAE_TYPE_WINDOW))
#define ESAE_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ESAE_TYPE_WINDOW))
#define ESAE_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ESAE_TYPE_WINDOW, EsaeWindowClass))

GType      esae_window_get_type          (void) G_GNUC_CONST;

GtkWidget *esae_window_new               (void) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__ESAE_WINDOW_H__ */
