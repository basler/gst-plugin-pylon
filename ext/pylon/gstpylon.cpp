/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marsol Zeledon <marisol.zeledon@ridgerun.com>
 */

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

#include "gstpylon.h"

struct _GstPylon {
  Pylon::CBaslerUniversalInstantCamera *camera;
};

void gst_pylon_initialize () {
  Pylon::PylonInitialize ();
}

GstPylon *gst_pylon_new () {
  GstPylon *self = (GstPylon*)g_malloc (sizeof (GstPylon));

  try {
    self->camera = new Pylon::CBaslerUniversalInstantCamera (Pylon::CTlFactory::GetInstance().CreateFirstDevice());
  }
  catch (const Pylon::GenericException& e) {
    std::cerr << "The camera object could not be created." << std::endl << e.GetDescription() << std::endl;
  }

  return self;
}

void gst_pylon_free (GstPylon *self) {
  delete self->camera;
  self->camera = nullptr;

  g_free (self);
}

void gst_pylon_start (GstPylon *self) {
  try {
    self->camera->Open();
  }
  catch (const Pylon::GenericException& e) {
    std::cerr << "The camera could not be opened." << std::endl << e.GetDescription() << std::endl;
  }
}

void gst_pylon_stop (GstPylon *self) {
  try {
    self->camera->Close();
  }
  catch (const Pylon::GenericException& e) {
    std::cerr << "The camera could not be closed." << std::endl << e.GetDescription() << std::endl;
  }
}
