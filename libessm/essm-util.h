/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@expidus.org>
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

#ifndef __ESSM_UTIL_H__
#define __ESSM_UTIL_H__

#include <esconf/esconf.h>

#include <gtk/gtk.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

G_BEGIN_DECLS;

#define DEFAULT_SESSION_NAME "Default"

typedef struct _EssmSessionInfo  EssmSessionInfo;

struct _EssmSessionInfo
{
  gchar     *name;      /* name of the session */
  time_t     atime;     /* last access time */
  GdkPixbuf *preview;   /* preview icon (52x42) */
};

enum
{
  PREVIEW_COLUMN,
  NAME_COLUMN,
  TITLE_COLUMN,
  ACCESSED_COLUMN,
  ATIME_COLUMN,
  N_COLUMNS,
};


gboolean       essm_start_application                (gchar      **command,
                                                      gchar      **environment,
                                                      GdkScreen   *screen,
                                                      const gchar *current_directory,
                                                      const gchar *client_machine,
                                                      const gchar *user_id);

void           essm_place_trash_window               (GtkWindow *window,
                                                      GdkScreen *screen,
                                                      gint       monitor);

/* XXX - move to libexpidus1util? */
gboolean       essm_strv_equal                       (gchar **a,
                                                      gchar **b);

EsconfChannel *essm_open_config                      (void);

gchar         *essm_gdk_display_get_fullname         (GdkDisplay *display);

GdkPixbuf     *essm_load_session_preview             (const gchar *name);

ExpidusRc        *settings_list_sessions_open_rc        (void);

GList         *settings_list_sessions                (ExpidusRc *rc);

void           settings_list_sessions_treeview_init  (GtkTreeView *treeview);

void           settings_list_sessions_populate       (GtkTreeModel *model,
                                                      GList       *sessions);

void           settings_list_sessions_delete_session (GtkButton *button,
                                                      GtkTreeView *treeview);

G_END_DECLS;

#endif /* !__ESSM_UTIL_H__ */
