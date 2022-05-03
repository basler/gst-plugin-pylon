/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Authors: Michael Gruner <michael.gruner@ridgerun.com>
 *          Marisol Zeledon <marisol.zeledon@ridgerun.com>
 */

#include "gstpylon.h"

#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/PylonIncludes.h>

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
        new Pylon::CBaslerUniversalInstantCamera (Pylon::
        CTlFactory::GetInstance ().CreateFirstDevice ());
  }
  catch (const Pylon::GenericException & e)
  {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    g_free (self);
    self = NULL;
  }

  return self;
}

void
gst_pylon_free (GstPylon * self)
{
  g_return_if_fail (self);

  delete self->camera;
  self->camera = NULL;

  g_free (self);
}

gboolean
gst_pylon_start (GstPylon * self, GError ** err)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (err || *err == NULL, FALSE);

  try {
    self->camera->Open ();
    self->camera->StartGrabbing ();
  }
  catch (const Pylon::GenericException & e)
  {
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
  g_return_val_if_fail (err || *err == NULL, FALSE);

  try {
    self->camera->StopGrabbing ();
    self->camera->Close ();
  }
  catch (const Pylon::GenericException & e) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    ret = FALSE;
  }

  return ret;
}

gboolean
gst_pylon_capture (GstPylon * self, GstBuffer ** buf, GError ** err)
{
  Pylon::CBaslerUniversalGrabResultPtr ptr_grab_result;
  size_t buffer_size;
  GstMapInfo info;
  int timeout_ms = 5000;
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (buf, FALSE);
  g_return_val_if_fail (err || *err == NULL, FALSE);

  /* Catch the timeout exception if any */
  try {
    self->camera->RetrieveResult (timeout_ms, ptr_grab_result,
        Pylon::TimeoutHandling_ThrowException);
  }
  catch (const Pylon::GenericException & e)
  {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    return FALSE;
  }

  if (ptr_grab_result->GrabSucceeded () == FALSE) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        ptr_grab_result->GetErrorDescription ().c_str ());
    return FALSE;
  }

  buffer_size = ptr_grab_result->GetBufferSize ();
  *buf = gst_buffer_new_allocate (NULL, buffer_size, NULL);

  if (*buf == NULL) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        "Failed to allocate buffer.");
    return FALSE;
  }

  ret = gst_buffer_map (*buf, &info, GST_MAP_WRITE);

  if (ret == FALSE) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        "Failed tu map buffer.");
    gst_buffer_unref (*buf);
    return FALSE;
  }

  memcpy (info.data, ptr_grab_result->GetBuffer (), buffer_size);
  gst_buffer_unmap (*buf, &info);

  return TRUE;
}
