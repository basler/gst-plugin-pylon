/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marisol Zeledon <marisol.zeledon@ridgerun.com>
 */

#ifndef _GST_PYLON_H_
#define _GST_PYLON_H_

#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstPylon GstPylon;

void gst_pylon_initialize ();

GstPylon *gst_pylon_new (GError ** err);
void gst_pylon_free (GstPylon * self);

gboolean gst_pylon_start (GstPylon * self, GError ** err);
gboolean gst_pylon_stop (GstPylon * self, GError ** err);
gboolean gst_pylon_capture (GstPylon * self, GstBuffer ** buf, GError ** err);

G_END_DECLS

#endif
