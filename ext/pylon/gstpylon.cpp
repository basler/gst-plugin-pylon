/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marisol Zeledon <marisol.zeledon@ridgerun.com>
 */

#include "gstpylon.h"

#include <gst/gst.h>
#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

struct _GstPylon
{
  Pylon::CBaslerUniversalInstantCamera * camera;
};

void
gst_pylon_initialize ()
{
  Pylon::PylonInitialize ();
}

GstPylon *
gst_pylon_new (GError ** err)
{
  GstPylon *self = (GstPylon *) g_malloc (sizeof (GstPylon));

  g_return_val_if_fail (self, NULL);

  try {
    self->camera =
        new Pylon::CBaslerUniversalInstantCamera (Pylon::CTlFactory::
        GetInstance ().CreateFirstDevice ());
  }
  catch (const Pylon::GenericException & e)
  {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
  }

  return self;
}

gboolean
gst_pylon_free (GstPylon * self)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);

  delete self->camera;
  self->camera = nullptr;

  g_free (self);

  return ret;
}

gboolean
gst_pylon_start (GstPylon * self, GError ** err)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

  try {
    self->camera->Open ();
  }
  catch (const Pylon::GenericException & e) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    ret = FALSE;
  }

  return ret;
}

gboolean
gst_pylon_stop (GstPylon * self, GError ** err)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

  try {
    self->camera->Close ();
  }
  catch (const Pylon::GenericException & e) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    ret = FALSE;
  }

  return ret;
}
