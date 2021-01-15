/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@expidus.org>
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

#ifndef __ESAE_DIALOG_H__
#define __ESAE_DIALOG_H__

#include <gtk/gtk.h>
#include "esae-model.h" /* Type EssmRunHook */

G_BEGIN_DECLS;

typedef struct _EsaeDialogClass EsaeDialogClass;
typedef struct _EsaeDialog      EsaeDialog;

#define ESAE_TYPE_DIALOG            (esae_dialog_get_type ())
#define ESAE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESAE_TYPE_DIALOG, EsaeDialog))
#define ESAE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ESAE_TYPE_DIALOG, EsaeDialogClass))
#define ESAE_IS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESAE_TYPE_DIALOG))
#define ESAE_IS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ESAE_TYPE_DIALOG))
#define ESAE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ESAE_TYPE_DIALOG, EsaeDialogClass))

GType      esae_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *esae_dialog_new      (const gchar *name,
                                 const gchar *descr,
                                 const gchar *command,
                                 EssmRunHook  trigger);

void       esae_dialog_get      (EsaeDialog   *dialog,
                                 gchar       **name,
                                 gchar       **descr,
                                 gchar       **command,
                                 EssmRunHook  *trigger);

G_END_DECLS;

#endif /* !__ESAE_DIALOG_H__ */
