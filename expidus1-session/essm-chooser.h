/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@expidus.org>
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

#ifndef __ESSM_CHOOSER_H__
#define __ESSM_CHOOSER_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS;

#define ESSM_TYPE_CHOOSER essm_chooser_get_type()
#define ESSM_CHOOSER(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, ESSM_TYPE_CHOOSER, EssmChooser)
#define ESSM_CHOOSER_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, ESSM_TYPE_CHOOSER, EssmChooserClass)
#define ESSM_IS_CHOOSER(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, ESSM_TYPE_CHOOSER)

#define ESSM_RESPONSE_LOAD  1
#define ESSM_RESPONSE_NEW   2

typedef struct _EssmChooser EssmChooser;
typedef struct _EssmChooserClass EssmChooserClass;

struct _EssmChooserClass
{
  GtkDialogClass parent_class;
};

struct _EssmChooser
{
  GtkDialog dialog;

  GtkWidget *tree;
};

GType essm_chooser_get_type (void) G_GNUC_CONST;

void essm_chooser_set_sessions (EssmChooser *chooser,
                                GList       *sessions,
                                const gchar *default_session);

gchar *essm_chooser_get_session (const EssmChooser *chooser);

G_END_DECLS;


#endif /* !__ESSM_CHOOSER_H__ */
