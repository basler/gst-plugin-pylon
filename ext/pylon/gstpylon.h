/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marsol Zeledon <marisol.zeledon@ridgerun.com>
 */

#ifndef GSTPYLON_H
#define GSTPYLON_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GstPylon GstPylon;

GstPylon *gst_pylon_new ();
void gst_pylon_free (GstPylon *self);

void gst_pylon_start (GstPylon * self);

G_END_DECLS

#endif  // GSTPYLON_H