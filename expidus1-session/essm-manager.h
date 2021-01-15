/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@expidus.org>
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

#ifndef __ESSM_MANAGER_H__
#define __ESSM_MANAGER_H__

#include <glib-object.h>

#include <esconf/esconf.h>
#include <libexpidus1util/libexpidus1util.h>

#include <expidus1-session/essm-client.h>
#include <expidus1-session/essm-global.h>
#include <expidus1-session/essm-shutdown.h>

G_BEGIN_DECLS

#define ESSM_TYPE_MANAGER     (essm_manager_get_type())
#define ESSM_MANAGER(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), ESSM_TYPE_MANAGER, EssmManager))
#define ESSM_IS_MANAGER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ESSM_TYPE_MANAGER))

#define DIE_TIMEOUT            (     7 * 1000)
#define SAVE_TIMEOUT           (    60 * 1000)
#define STARTUP_TIMEOUT        (     8 * 1000)
#define RESTART_RESET_TIMEOUT  (5 * 60 * 1000)
#define ESSM_CHOOSE_LOGOUT  0
#define ESSM_CHOOSE_LOAD    1
#define ESSM_CHOOSE_NEW     2

typedef enum
{
  ESSM_MANAGER_STARTUP,
  ESSM_MANAGER_IDLE,
  ESSM_MANAGER_CHECKPOINT,
  ESSM_MANAGER_SHUTDOWN,
  ESSM_MANAGER_SHUTDOWNPHASE2,
} EssmManagerState;

typedef enum
{
    ESSM_MANAGER_QUEUE_PENDING_PROPS = 0,
    ESSM_MANAGER_QUEUE_STARTING_PROPS,
    ESSM_MANAGER_QUEUE_RESTART_PROPS,
    ESSM_MANAGER_QUEUE_RUNNING_CLIENTS,
} EssmManagerQueueType;

typedef enum
{
    ESSM_MANAGER_COMPAT_GNOME = 0,
    ESSM_MANAGER_COMPAT_KDE,
} EssmManagerCompatType;

typedef struct _EssmManager  EssmManager;

GType essm_manager_get_type (void) G_GNUC_CONST;

EssmManager *essm_manager_new (GDBusConnection *connection);

void essm_manager_load (EssmManager   *manager,
                        EsconfChannel *channel);

gboolean essm_manager_restart (EssmManager *manager);

/* call when startup is finished */
void essm_manager_signal_startup_done (EssmManager *manager);

/* call for each client that fails */
gboolean essm_manager_handle_failed_properties (EssmManager    *manager,
                                                EssmProperties *properties);

EssmClient* essm_manager_new_client (EssmManager *manager,
                                     SmsConn      sms_conn,
                                     gchar      **error);

gboolean essm_manager_register_client (EssmManager *manager,
                                       EssmClient  *client,
                                       const gchar *dbus_client_id,
                                       const gchar *previous_id);

void essm_manager_start_interact (EssmManager *manager,
                                  EssmClient  *client);

void essm_manager_interact (EssmManager *manager,
                            EssmClient  *client,
                            gint         dialog_type);

void essm_manager_interact_done (EssmManager *manager,
                                 EssmClient  *client,
                                 gboolean     cancel_shutdown);

void essm_manager_save_yourself (EssmManager *manager,
                                 EssmClient  *client,
                                 gint         save_type,
                                 gboolean     shutdown,
                                 gint         interact_style,
                                 gboolean     fast,
                                 gboolean     global);

void essm_manager_save_yourself_phase2 (EssmManager *manager,
                                        EssmClient  *client);

void essm_manager_save_yourself_done (EssmManager *manager,
                                      EssmClient  *client,
                                      gboolean     success);

void essm_manager_close_connection (EssmManager *manager,
                                    EssmClient  *client,
                                    gboolean     cleanup);

void essm_manager_close_connection_by_ice_conn (EssmManager *manager,
                                                IceConn ice_conn);

gboolean essm_manager_check_clients_saving (EssmManager *manager);

gboolean essm_manager_maybe_enter_phase2 (EssmManager *manager);

gboolean essm_manager_terminate_client (EssmManager *manager,
                                        EssmClient  *client,
                                        GError     **error);

void essm_manager_perform_shutdown (EssmManager *manager);

gboolean essm_manager_run_command (EssmManager          *manager,
                                   const EssmProperties *properties,
                                   const gchar          *command);

void essm_manager_store_session (EssmManager *manager);

void essm_manager_complete_saveyourself (EssmManager *manager);

EssmShutdownType essm_manager_get_shutdown_type (EssmManager *manager);

EssmManagerState essm_manager_get_state (EssmManager *manager);

GQueue *essm_manager_get_queue (EssmManager         *manager,
                                EssmManagerQueueType q_type);

gboolean essm_manager_get_use_failsafe_mode (EssmManager *manager);

void essm_manager_increase_failsafe_pending_clients (EssmManager *manager);

gboolean essm_manager_get_compat_startup (EssmManager          *manager,
                                          EssmManagerCompatType type);

gboolean essm_manager_get_start_at (EssmManager *manager);

#endif /* !__ESSM_MANAGER_H__ */
