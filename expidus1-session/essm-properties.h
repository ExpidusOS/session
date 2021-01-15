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

#ifndef __ESSM_PROPERTIES_H__
#define __ESSM_PROPERTIES_H__

#include <X11/SM/SMlib.h>

#include <libexpidus1util/libexpidus1util.h>

/* GNOME compatibility */
#define GsmPriority     "_GSM_Priority"
#define GsmDesktopFile  "_GSM_DesktopFile"

#define MAX_RESTART_ATTEMPTS 5

typedef struct _EssmProperties EssmProperties;

struct _EssmProperties
{
  guint   restart_attempts;
  guint   restart_attempts_reset_id;

  guint   startup_timeout_id;

  GPid    pid;
  guint   child_watch_id;

  gchar  *client_id;
  gchar  *hostname;
  gchar  *service_name;

  GTree  *sm_properties;
};


#define ESSM_PROPERTIES(p) ((EssmProperties *) (p))


EssmProperties *essm_properties_new (const gchar *client_id,
                                     const gchar *hostname) G_GNUC_PURE;
void            essm_properties_free    (EssmProperties *properties);

void            essm_properties_extract (EssmProperties *properties,
                                         gint           *num_props,
                                         SmProp       ***props);
void            essm_properties_store   (EssmProperties *properties,
                                         ExpidusRc         *rc,
                                         const gchar    *prefix);

EssmProperties* essm_properties_load (ExpidusRc *rc, const gchar *prefix);

gboolean essm_properties_check (const EssmProperties *properties) G_GNUC_CONST;

const gchar *essm_properties_get_string (EssmProperties *properties,
                                                  const gchar *property_name);
gchar **essm_properties_get_strv (EssmProperties *properties,
                                  const gchar *property_name);
guchar essm_properties_get_uchar (EssmProperties *properties,
                                  const gchar *property_name,
                                  guchar default_value);

const GValue *essm_properties_get (EssmProperties *properties,
                                   const gchar *property_name);

void essm_properties_set_string (EssmProperties *properties,
                                 const gchar *property_name,
                                 const gchar *property_value);
void essm_properties_set_strv (EssmProperties *properties,
                               const gchar *property_name,
                               gchar **property_value);
void essm_properties_set_uchar (EssmProperties *properties,
                                const gchar *property_name,
                                guchar property_value);

gboolean essm_properties_set (EssmProperties *properties,
                              const gchar *property_name,
                              const GValue *property_value);
gboolean essm_properties_set_from_smprop (EssmProperties *properties,
                                          const SmProp *sm_prop);

gboolean essm_properties_remove (EssmProperties *properties,
                                 const gchar *property_name);

void essm_properties_set_default_child_watch (EssmProperties *properties);

gint essm_properties_compare (const EssmProperties *a,
                              const EssmProperties *b) G_GNUC_CONST;

gint essm_properties_compare_id (const EssmProperties *properties,
                                 const gchar *client_id);

#endif /* !__ESSM_PROPERTIES_H__ */
