/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@expidus.org>
 * Copyright (c) 2011      Nick Schermer <nick@expidus.org>
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

#ifndef __ESSM_LOGOUT_DIALOG_H__
#define __ESSM_LOGOUT_DIALOG_H__

#include <expidus1-session/essm-shutdown.h>

typedef struct _EssmLogoutDialogClass EssmLogoutDialogClass;
typedef struct _EssmLogoutDialog      EssmLogoutDialog;

#define ESSM_TYPE_LOGOUT_DIALOG            (essm_logout_dialog_get_type ())
#define ESSM_LOGOUT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESSM_TYPE_LOGOUT_DIALOG, EssmLogoutDialog))
#define ESSM_LOGOUT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ESSM_TYPE_LOGOUT_DIALOG, EssmLogoutDialogClass))
#define ESSM_IS_LOGOUT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESSM_TYPE_LOGOUT_DIALOG))
#define ESSM_IS_LOGOUT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ESSM_TYPE_LOGOUT_DIALOG))
#define ESSM_LOGOUT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ESSM_TYPE_LOGOUT_DIALOG, EssmLogoutDialogClass))

GType      essm_logout_dialog_get_type (void) G_GNUC_CONST;

gboolean   essm_logout_dialog          (const gchar      *session_name,
                                        EssmShutdownType *return_type,
                                        gboolean         *return_save_session,
                                        gboolean          accessibility);

#endif
