/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marisol Zeledon <marisol.zeledon@ridgerun.com>
 */

#ifndef GSTPYLON_H_
#define GSTPYLON_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GstPylon GstPylon;

void gst_pylon_initialize ();

GstPylon *gst_pylon_new ();
void gst_pylon_free (GstPylon *self);

void gst_pylon_start (GstPylon *self);
void gst_pylon_stop (GstPylon *self);

G_END_DECLS

#endif  // GSTPYLON_H_
