/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marsol Zeledon <marisol.zeledon@ridgerun.com>
 */

#ifndef GSTPYLON_HPP
#define GSTPYLON_HPP

#include <glib.h>

typedef struct _GstPylon GstPylon;

GstPylon *gst_pylon_new ();
void gst_pylon_free (GstPylon *self);


#endif  // GSTPYLON_HPP
