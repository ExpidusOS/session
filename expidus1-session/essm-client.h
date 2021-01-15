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

#ifndef __ESSM_CLIENT_H__
#define __ESSM_CLIENT_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <expidus1-session/essm-properties.h>

G_BEGIN_DECLS

#define ESSM_TYPE_CLIENT     (essm_client_get_type ())
#define ESSM_CLIENT(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), ESSM_TYPE_CLIENT, EssmClient))
#define ESSM_IS_CLIENT(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ESSM_TYPE_CLIENT))

/* fwd decl */
struct _EssmManager;

typedef struct _EssmClient EssmClient;

typedef enum
{
  ESSM_CLIENT_IDLE = 0,
  ESSM_CLIENT_INTERACTING,
  ESSM_CLIENT_SAVEDONE,
  ESSM_CLIENT_SAVING,
  ESSM_CLIENT_SAVINGLOCAL,
  ESSM_CLIENT_WAITFORINTERACT,
  ESSM_CLIENT_WAITFORPHASE2,
  ESSM_CLIENT_DISCONNECTED,
  ESSM_CLIENT_STATE_COUNT
} EssmClientState;

GType essm_client_get_type (void) G_GNUC_CONST;

EssmClient *essm_client_new (struct _EssmManager *manager,
                             SmsConn              sms_conn,
                             GDBusConnection     *connection);

void essm_client_set_initial_properties (EssmClient     *client,
                                         EssmProperties *properties);

EssmClientState essm_client_get_state (EssmClient *client);
void essm_client_set_state (EssmClient     *client,
                            EssmClientState state);

const gchar *essm_client_get_id (EssmClient *client);
const gchar *essm_client_get_app_id (EssmClient *client);

SmsConn essm_client_get_sms_connection (EssmClient *client);

EssmProperties *essm_client_get_properties (EssmClient *client);
EssmProperties *essm_client_steal_properties (EssmClient *client);

void essm_client_merge_properties (EssmClient *client,
                                   SmProp    **props,
                                   gint        num_props);
void essm_client_delete_properties (EssmClient *client,
                                    gchar     **prop_names,
                                    gint        num_props);

const gchar *essm_client_get_object_path (EssmClient *client);

void essm_client_set_pid (EssmClient *client,
                          pid_t       pid);

void essm_client_set_app_id (EssmClient  *client,
                             const gchar *app_id);

void         essm_client_set_service_name (EssmClient *client,
                                           const gchar *service_name);
const gchar *essm_client_get_service_name (EssmClient *client);

void essm_client_terminate (EssmClient *client);

void essm_client_end_session (EssmClient *client);

void essm_client_cancel_shutdown (EssmClient *client);

G_END_DECLS

#endif /* !__ESSM_CLIENT_H__ */
