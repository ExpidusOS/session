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

#ifndef __ESSM_SHUTDOWN_H__
#define __ESSM_SHUTDOWN_H__

typedef struct _EssmShutdownClass EssmShutdownClass;
typedef struct _EssmShutdown      EssmShutdown;

#define ESSM_TYPE_SHUTDOWN            (essm_shutdown_get_type ())
#define ESSM_SHUTDOWN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESSM_TYPE_SHUTDOWN, EssmShutdown))
#define ESSM_SHUTDOWN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ESSM_TYPE_SHUTDOWN, EssmShutdownClass))
#define ESSM_IS_SHUTDOWN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESSM_TYPE_SHUTDOWN))
#define ESSM_IS_SHUTDOWN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ESSM_TYPE_SHUTDOWN))
#define ESSM_SHUTDOWN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ESSM_TYPE_SHUTDOWN, EssmShutdownClass))

typedef enum
{
  ESSM_SHUTDOWN_ASK = 0,
  ESSM_SHUTDOWN_LOGOUT,
  ESSM_SHUTDOWN_SHUTDOWN,
  ESSM_SHUTDOWN_RESTART,
  ESSM_SHUTDOWN_SUSPEND,
  ESSM_SHUTDOWN_HIBERNATE,
  ESSM_SHUTDOWN_HYBRID_SLEEP,
  ESSM_SHUTDOWN_SWITCH_USER,
}
EssmShutdownType;

typedef enum
{
  PASSWORD_RETRY,
  PASSWORD_SUCCEED,
  PASSWORD_FAILED
}
EssmPassState;

GType         essm_shutdown_get_type         (void) G_GNUC_CONST;

EssmShutdown *essm_shutdown_get              (void);

gboolean      essm_shutdown_password_require (EssmShutdown      *shutdown,
                                              EssmShutdownType   type);

EssmPassState essm_shutdown_password_send    (EssmShutdown      *shutdown,
                                              EssmShutdownType   type,
                                              const gchar       *password);

gboolean      essm_shutdown_try_type         (EssmShutdown      *shutdown,
                                              EssmShutdownType   type,
                                              GError           **error);

gboolean      essm_shutdown_try_restart      (EssmShutdown      *shutdown,
                                              GError           **error);

gboolean      essm_shutdown_try_shutdown     (EssmShutdown      *shutdown,
                                              GError           **error);

gboolean      essm_shutdown_try_suspend      (EssmShutdown      *shutdown,
                                              GError           **error);

gboolean      essm_shutdown_try_hibernate    (EssmShutdown      *shutdown,
                                              GError           **error);

gboolean      essm_shutdown_try_hybrid_sleep (EssmShutdown      *shutdown,
                                              GError           **error);

gboolean      essm_shutdown_try_switch_user  (EssmShutdown      *shutdown,
                                              GError           **error);

gboolean      essm_shutdown_can_restart      (EssmShutdown      *shutdown,
                                              gboolean          *can_restart,
                                              GError           **error);

gboolean      essm_shutdown_can_shutdown     (EssmShutdown      *shutdown,
                                              gboolean          *can_shutdown,
                                              GError           **error);

gboolean      essm_shutdown_can_suspend      (EssmShutdown      *shutdown,
                                              gboolean          *can_suspend,
                                              gboolean          *auth_suspend,
                                              GError           **error);

gboolean      essm_shutdown_can_hibernate    (EssmShutdown      *shutdown,
                                              gboolean          *can_hibernate,
                                              gboolean          *auth_hibernate,
                                              GError           **error);

gboolean      essm_shutdown_can_hybrid_sleep (EssmShutdown      *shutdown,
                                              gboolean          *can_hybrid_sleep,
                                              gboolean          *auth_hybrid_sleep,
                                              GError           **error);

gboolean      essm_shutdown_can_switch_user  (EssmShutdown      *shutdown,
                                              gboolean          *can_switch_user,
                                              GError           **error);


gboolean      essm_shutdown_can_save_session (EssmShutdown      *shutdown);

#endif	/* !__ESSM_SHUTDOWN_H__ */
