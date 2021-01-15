/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@expidus.org>
 * Copyright (c) 2010      Ali Abdallah    <aliov@expidus.org>
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

#ifndef __ESSM_CONSOLEKIT_HELPER_H__
#define __ESSM_CONSOLEKIT_HELPER_H__

typedef struct _EssmConsolekitClass EssmConsolekitClass;
typedef struct _EssmConsolekit      EssmConsolekit;

#define ESSM_TYPE_CONSOLEKIT            (essm_consolekit_get_type ())
#define ESSM_CONSOLEKIT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESSM_TYPE_CONSOLEKIT, EssmConsolekit))
#define ESSM_CONSOLEKIT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ESSM_TYPE_CONSOLEKIT, EssmConsolekitClass))
#define ESSM_IS_CONSOLEKIT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESSM_TYPE_CONSOLEKIT))
#define ESSM_IS_CONSOLEKIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ESSM_TYPE_CONSOLEKIT))
#define ESSM_CONSOLEKIT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ESSM_TYPE_CONSOLEKIT, EssmConsolekitClass))

GType           essm_consolekit_get_type     (void) G_GNUC_CONST;

EssmConsolekit *essm_consolekit_get          (void);

gboolean        essm_consolekit_try_restart  (EssmConsolekit  *consolekit,
                                              GError         **error);

gboolean        essm_consolekit_try_shutdown (EssmConsolekit  *consolekit,
                                              GError         **error);

gboolean        essm_consolekit_can_restart  (EssmConsolekit  *consolekit,
                                              gboolean        *can_restart,
                                              GError         **error);

gboolean        essm_consolekit_can_shutdown (EssmConsolekit  *consolekit,
                                              gboolean        *can_shutdown,
                                              GError         **error);

gboolean        essm_consolekit_try_suspend  (EssmConsolekit  *consolekit,
                                              GError         **error);

gboolean        essm_consolekit_try_hibernate (EssmConsolekit  *consolekit,
                                               GError         **error);

gboolean        essm_consolekit_try_hybrid_sleep (EssmConsolekit  *consolekit,
                                                  GError         **error);

gboolean        essm_consolekit_can_suspend  (EssmConsolekit  *consolekit,
                                              gboolean        *can_suspend,
                                              gboolean        *auth_suspend,
                                              GError         **error);

gboolean        essm_consolekit_can_hibernate (EssmConsolekit  *consolekit,
                                               gboolean        *can_hibernate,
                                               gboolean        *auth_hibernate,
                                               GError         **error);

gboolean        essm_consolekit_can_hybrid_sleep (EssmConsolekit  *consolekit,
                                                  gboolean        *can_hybrid_sleep,
                                                  gboolean        *auth_hybrid_sleep,
                                                  GError         **error);


#endif /* !__ESSM_CONSOLEKIT_HELPER_H__ */
