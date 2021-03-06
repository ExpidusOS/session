/*
 * * Copyright (C) 2016 Eric Koegel <eric@expidus.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __EXPIDUS_SCREENSAVER_H
#define __EXPIDUS_SCREENSAVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EXPIDUS_TYPE_SCREENSAVER  (expidus_screensaver_get_type () )
#define EXPIDUS_SCREENSAVER(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), EXPIDUS_TYPE_SCREENSAVER, ExpidusScreenSaver))
#define EXPIDUS_IS_POWER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EXPIDUS_TYPE_SCREENSAVER))

typedef struct ExpidusScreenSaverPrivate ExpidusScreenSaverPrivate;

typedef struct
{
    GObject                 parent;
    ExpidusScreenSaverPrivate *priv;
} ExpidusScreenSaver;

typedef struct
{
    GObjectClass parent_class;
} ExpidusScreenSaverClass;

GType            expidus_screensaver_get_type      (void) G_GNUC_CONST;

ExpidusScreenSaver *expidus_screensaver_new           (void);

void             expidus_screensaver_inhibit       (ExpidusScreenSaver *saver,
                                                 gboolean suspend);

gboolean         expidus_screensaver_lock          (ExpidusScreenSaver *saver);



G_END_DECLS

#endif /* __EXPIDUS_SCREENSAVER_H */
