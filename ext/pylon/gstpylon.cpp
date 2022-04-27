/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marsol Zeledon <marisol.zeledon@ridgerun.com>
 */

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

#include "gstpylon.hpp"

struct _GstPylon {
  Pylon::CBaslerUniversalInstantCamera camera;
};

GstPylon *gst_pylon_new () {
  GstPylon *self = (GstPylon*)g_malloc (sizeof (GstPylon));
  return self;
}

void gst_pylon_free (GstPylon *self) {
  g_free (self);
}
