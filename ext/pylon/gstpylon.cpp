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
  Pylon::CBaslerUniversalInstantCamera *camera;
};

GstPylon *gst_pylon_new () {
  GstPylon *self = (GstPylon*)g_malloc (sizeof (GstPylon));
  *self = {.camera = new Pylon::CBaslerUniversalInstantCamera (Pylon::CTlFactory::GetInstance().CreateFirstDevice())}; 
  return self;
}

void gst_pylon_free (GstPylon *self) {
  delete self->camera;
  self->camera = nullptr;

  g_free (self);
}
