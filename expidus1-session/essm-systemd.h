/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Christian Hesse
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ESSM_SYSTEMD_H__
#define __ESSM_SYSTEMD_H__

#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

typedef struct _EssmSystemdClass EssmSystemdClass;
typedef struct _EssmSystemd      EssmSystemd;

#define ESSM_TYPE_SYSTEMD            (essm_systemd_get_type ())
#define ESSM_SYSTEMD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESSM_TYPE_SYSTEMD, EssmSystemd))
#define ESSM_SYSTEMD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ESSM_TYPE_SYSTEMD, EssmSystemdClass))
#define ESSM_IS_SYSTEMD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESSM_TYPE_SYSTEMD))
#define ESSM_IS_SYSTEMD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ESSM_TYPE_SYSTEMD))
#define ESSM_SYSTEMD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ESSM_TYPE_SYSTEMD, EssmSystemdClass))

GType           essm_systemd_get_type     (void) G_GNUC_CONST;

EssmSystemd *essm_systemd_get          (void);

gboolean     essm_systemd_try_restart  (EssmSystemd  *systemd,
                                        GError      **error);

gboolean     essm_systemd_try_shutdown (EssmSystemd  *systemd,
                                        GError      **error);

gboolean     essm_systemd_try_suspend  (EssmSystemd  *systemd,
                                        GError      **error);

gboolean     essm_systemd_try_hibernate (EssmSystemd *systemd,
                                         GError      **error);

gboolean     essm_systemd_try_hybrid_sleep (EssmSystemd *systemd,
                                            GError      **error);

gboolean     essm_systemd_can_restart  (EssmSystemd  *systemd,
                                        gboolean     *can_restart,
                                        GError      **error);

gboolean     essm_systemd_can_shutdown (EssmSystemd  *systemd,
                                        gboolean     *can_shutdown,
                                        GError      **error);

gboolean     essm_systemd_can_suspend  (EssmSystemd  *systemd,
                                        gboolean     *can_suspend,
                                        gboolean     *auth_suspend,
                                        GError      **error);

gboolean     essm_systemd_can_hibernate (EssmSystemd *systemd,
                                        gboolean     *can_hibernate,
                                        gboolean     *auth_hibernate,
                                        GError      **error);

gboolean     essm_systemd_can_hybrid_sleep (EssmSystemd *systemd,
                                            gboolean     *can_hybrid_sleep,
                                            gboolean     *auth_hybrid_sleep,
                                            GError      **error);

G_END_DECLS

#endif  /* __ESSM_SYSTEMD_H__ */
