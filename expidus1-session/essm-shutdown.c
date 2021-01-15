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
 *
 * Parts of this file where taken from gnome-session/logout.c, which
 * was written by Owen Taylor <otaylor@redhat.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <gio/gio.h>
#include <libexpidus1util/libexpidus1util.h>
#include <gtk/gtk.h>
#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif

#include <libessm/essm-util.h>

#include <expidus1-session/essm-shutdown.h>
#include <expidus1-session/essm-compat-gnome.h>
#include <expidus1-session/essm-compat-kde.h>
#include <expidus1-session/essm-consolekit.h>
#include <expidus1-session/essm-fadeout.h>
#include <expidus1-session/essm-global.h>
#include <expidus1-session/essm-legacy.h>
#include <expidus1-session/essm-systemd.h>
#include <expidus1-session/essm-shutdown-fallback.h>



static void essm_shutdown_finalize  (GObject      *object);



struct _EssmShutdownClass
{
  GObjectClass __parent__;
};


struct _EssmShutdown
{
  GObject __parent__;

  EssmSystemd    *systemd;
  EssmConsolekit *consolekit;

  /* kiosk settings */
  gboolean        kiosk_can_shutdown;
  gboolean        kiosk_can_save_session;
};



G_DEFINE_TYPE (EssmShutdown, essm_shutdown, G_TYPE_OBJECT)



static void
essm_shutdown_class_init (EssmShutdownClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = essm_shutdown_finalize;
}



static void
essm_shutdown_init (EssmShutdown *shutdown)
{
  ExpidusKiosk *kiosk;

  shutdown->consolekit = NULL;
  shutdown->systemd = NULL;

  if (LOGIND_RUNNING())
    shutdown->systemd = essm_systemd_get ();
  else
    shutdown->consolekit = essm_consolekit_get ();

  /* check kiosk */
  kiosk = expidus_kiosk_new ("expidus1-session");
  shutdown->kiosk_can_shutdown = expidus_kiosk_query (kiosk, "Shutdown");
  shutdown->kiosk_can_save_session = expidus_kiosk_query (kiosk, "SaveSession");
  expidus_kiosk_free (kiosk);
}



static void
essm_shutdown_finalize (GObject *object)
{
  EssmShutdown *shutdown = ESSM_SHUTDOWN (object);

  if (shutdown->systemd != NULL)
    g_object_unref (G_OBJECT (shutdown->systemd));

  if (shutdown->consolekit != NULL)
    g_object_unref (G_OBJECT (shutdown->consolekit));

  (*G_OBJECT_CLASS (essm_shutdown_parent_class)->finalize) (object);
}



static gboolean
essm_shutdown_kiosk_can_shutdown (EssmShutdown  *shutdown,
                                  GError       **error)
{
  if (!shutdown->kiosk_can_shutdown)
    {
      g_set_error_literal (error, 1, 0, _("Shutdown is blocked by the kiosk settings"));
      return FALSE;
    }

  return TRUE;
}



EssmShutdown *
essm_shutdown_get (void)
{
  static EssmShutdown *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (ESSM_TYPE_SHUTDOWN, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
essm_shutdown_try_type (EssmShutdown      *shutdown,
                        EssmShutdownType   type,
                        GError           **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  switch (type)
    {
    case ESSM_SHUTDOWN_SHUTDOWN:
      return essm_shutdown_try_shutdown (shutdown, error);

    case ESSM_SHUTDOWN_RESTART:
      return essm_shutdown_try_restart (shutdown, error);

    case ESSM_SHUTDOWN_SUSPEND:
      return essm_shutdown_try_suspend (shutdown, error);

    case ESSM_SHUTDOWN_HIBERNATE:
      return essm_shutdown_try_hibernate (shutdown, error);

    case ESSM_SHUTDOWN_HYBRID_SLEEP:
      return essm_shutdown_try_hybrid_sleep (shutdown, error);

    case ESSM_SHUTDOWN_SWITCH_USER:
      return essm_shutdown_try_switch_user (shutdown, error);

    default:
      g_set_error (error, 1, 0, _("Unknown shutdown method %d"), type);
      break;
    }

  return FALSE;
}




gboolean
essm_shutdown_try_restart (EssmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_try_restart (shutdown->systemd, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_try_restart (shutdown->consolekit, error))
        {
          return TRUE;
        }
    }

  return essm_shutdown_fallback_try_action (ESSM_SHUTDOWN_RESTART, error);
}



gboolean
essm_shutdown_try_shutdown (EssmShutdown  *shutdown,
                            GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_try_shutdown (shutdown->systemd, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_try_shutdown (shutdown->consolekit, NULL))
        {
          return TRUE;
        }
    }

  return essm_shutdown_fallback_try_action (ESSM_SHUTDOWN_SHUTDOWN, error);
}



typedef gboolean (*SleepFunc) (gpointer object, GError **);

static gboolean
try_sleep_method (gpointer  object,
                  SleepFunc func)
{
  if (object == NULL)
    return FALSE;

  return func (object, NULL);
}



gboolean
essm_shutdown_try_suspend (EssmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  /* Try each way to suspend - it will handle NULL.
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)essm_systemd_try_suspend))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)essm_consolekit_try_suspend))
    return TRUE;

  return essm_shutdown_fallback_try_action (ESSM_SHUTDOWN_SUSPEND, error);
}



gboolean
essm_shutdown_try_hibernate (EssmShutdown  *shutdown,
                             GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  /* Try each way to hibernate - it will handle NULL.
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)essm_systemd_try_hibernate))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)essm_consolekit_try_hibernate))
    return TRUE;

  return essm_shutdown_fallback_try_action (ESSM_SHUTDOWN_HIBERNATE, error);
}



gboolean
essm_shutdown_try_hybrid_sleep (EssmShutdown  *shutdown,
                                GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  /* Try each way to hybrid-sleep - it will handle NULL.
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)essm_systemd_try_hybrid_sleep))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)essm_consolekit_try_hybrid_sleep))
    return TRUE;

  return essm_shutdown_fallback_try_action (ESSM_SHUTDOWN_HYBRID_SLEEP, error);
}

gboolean
essm_shutdown_try_switch_user (EssmShutdown  *shutdown,
                               GError       **error)
{
  GDBusProxy  *display_proxy;
  GVariant    *unused = NULL;
  const gchar *DBUS_NAME = "org.freedesktop.DisplayManager";
  const gchar *DBUS_INTERFACE = "org.freedesktop.DisplayManager.Seat";
  const gchar *DBUS_OBJECT_PATH = g_getenv ("XDG_SEAT_PATH");

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  display_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                 NULL,
                                                 DBUS_NAME,
                                                 DBUS_OBJECT_PATH,
                                                 DBUS_INTERFACE,
                                                 NULL,
                                                 error);

  if (display_proxy == NULL || *error != NULL)
    {
      essm_verbose ("display proxy == NULL or an error was set\n");
      return FALSE;
    }

  essm_verbose ("calling SwitchToGreeter\n");
  unused = g_dbus_proxy_call_sync (display_proxy,
                                  "SwitchToGreeter",
                                  g_variant_new ("()"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  3000,
                                  NULL,
                                  error);

  if (unused != NULL)
    {
      g_variant_unref (unused);
    }

  g_object_unref (display_proxy);

  return (*error == NULL);
}


gboolean
essm_shutdown_can_restart (EssmShutdown  *shutdown,
                           gboolean      *can_restart,
                           GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_restart = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_can_restart (shutdown->systemd, can_restart, error))
        return TRUE;
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_can_restart (shutdown->consolekit, can_restart, error))
        return TRUE;
    }

  *can_restart = essm_shutdown_fallback_auth_restart ();
  return TRUE;
}



gboolean
essm_shutdown_can_shutdown (EssmShutdown  *shutdown,
                            gboolean      *can_shutdown,
                            GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_shutdown = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_can_shutdown (shutdown->systemd, can_shutdown, error))
        return TRUE;
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_can_shutdown (shutdown->consolekit, can_shutdown, error))
        return TRUE;
    }

  *can_shutdown = essm_shutdown_fallback_auth_shutdown ();
  return TRUE;
}



gboolean
essm_shutdown_can_suspend (EssmShutdown  *shutdown,
                           gboolean      *can_suspend,
                           gboolean      *auth_suspend,
                           GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_suspend = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_can_suspend (shutdown->systemd, can_suspend, auth_suspend, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_can_suspend (shutdown->consolekit, can_suspend, auth_suspend, NULL))
        {
          return TRUE;
        }
    }

  *can_suspend = essm_shutdown_fallback_can_suspend ();
  *auth_suspend = essm_shutdown_fallback_auth_suspend ();
  return TRUE;
}



gboolean
essm_shutdown_can_hibernate (EssmShutdown  *shutdown,
                             gboolean      *can_hibernate,
                             gboolean      *auth_hibernate,
                             GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_hibernate = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_can_hibernate (shutdown->systemd, can_hibernate, auth_hibernate, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_can_hibernate (shutdown->consolekit, can_hibernate, auth_hibernate, NULL))
        {
          return TRUE;
        }
    }

  *can_hibernate = essm_shutdown_fallback_can_hibernate ();
  *auth_hibernate = essm_shutdown_fallback_auth_hibernate ();
  return TRUE;
}



gboolean
essm_shutdown_can_hybrid_sleep (EssmShutdown  *shutdown,
                                gboolean      *can_hybrid_sleep,
                                gboolean      *auth_hybrid_sleep,
                                GError       **error)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!essm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_hybrid_sleep = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (essm_systemd_can_hybrid_sleep (shutdown->systemd, can_hybrid_sleep, auth_hybrid_sleep, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (essm_consolekit_can_hybrid_sleep (shutdown->consolekit, can_hybrid_sleep, auth_hybrid_sleep, NULL))
        {
          return TRUE;
        }
    }

  *can_hybrid_sleep = essm_shutdown_fallback_can_hybrid_sleep ();
  *auth_hybrid_sleep = essm_shutdown_fallback_auth_hybrid_sleep ();
  return TRUE;
}



gboolean
essm_shutdown_can_switch_user (EssmShutdown  *shutdown,
                               gboolean      *can_switch_user,
                               GError       **error)
{
  GDBusProxy  *display_proxy;
  gchar       *owner = NULL;
  const gchar *DBUS_NAME = "org.freedesktop.DisplayManager";
  const gchar *DBUS_INTERFACE = "org.freedesktop.DisplayManager.Seat";
  const gchar *DBUS_OBJECT_PATH = g_getenv ("XDG_SEAT_PATH");

  *can_switch_user = FALSE;

  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);

  if (DBUS_OBJECT_PATH == NULL)
    {
      return TRUE;
    }

  display_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                 NULL,
                                                 DBUS_NAME,
                                                 DBUS_OBJECT_PATH,
                                                 DBUS_INTERFACE,
                                                 NULL,
                                                 error);

  if (display_proxy == NULL)
    {
      essm_verbose ("display proxy returned NULL\n");
      return FALSE;
    }

  /* is there anyone actually providing a service? */
  owner = g_dbus_proxy_get_name_owner (display_proxy);
  if (owner != NULL)
  {
    g_object_unref (display_proxy);
    g_free (owner);
    *can_switch_user = TRUE;
    return TRUE;
  }

  essm_verbose ("no owner NULL\n");
  return TRUE;
}



gboolean
essm_shutdown_can_save_session (EssmShutdown *shutdown)
{
  g_return_val_if_fail (ESSM_IS_SHUTDOWN (shutdown), FALSE);
  return shutdown->kiosk_can_save_session;
}
