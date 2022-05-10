/* Copyright (C) 2022 Basler AG
 *
 * Created by RidgeRun, LLC <support@ridgerun.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gstpylon.h"

#ifdef _MSC_VER                 // MSVC
#pragma warning(push)
#pragma warning(disable : 4265)
#elif __GNUC__                  // GCC, CLANG, MinGW
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/PylonIncludes.h>

#ifdef _MSC_VER                 // MSVC
#pragma warning(pop)
#elif __GNUC__                  // GCC, CLANG, MinWG
#pragma GCC diagnostic pop
#endif

static void free_ptr_grab_result (gpointer data);

struct _GstPylon
{
  Pylon::CBaslerUniversalInstantCamera camera;
};

void
gst_pylon_initialize ()
{
  Pylon::PylonInitialize ();
}

GstPylon *
gst_pylon_new (GError ** err)
{
  GstPylon *self = new GstPylon;

  g_return_val_if_fail (self, NULL);

  try {
    self->camera.Attach (Pylon::CTlFactory::GetInstance ().
        CreateFirstDevice ());
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

  delete self;
}

gboolean
gst_pylon_start (GstPylon * self, GError ** err)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (err || *err == NULL, FALSE);

  try {
    self->camera.Open ();

    /* hard code camera configuration */
    gint width = 1920;
    gint height = 1080;
    gdouble framerate = 30.0;
    self->camera.Width.SetValue (width, Pylon::IntegerValueCorrection_None);
    self->camera.Height.SetValue (height, Pylon::IntegerValueCorrection_None);
    self->camera.AcquisitionFrameRateEnable.SetValue (true);
    self->camera.AcquisitionFrameRateAbs.SetValue (framerate,
        Pylon::FloatValueCorrection_None);
    self->camera.
        PixelFormat.SetValue (Basler_UniversalCameraParams::
        PixelFormat_RGB8Packed);

    self->camera.StartGrabbing ();
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
    self->camera.StopGrabbing ();
    self->camera.Close ();
  }
  catch (const Pylon::GenericException & e) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    ret = FALSE;
  }

  return ret;
}

static void
free_ptr_grab_result (gpointer data)
{
  g_return_if_fail (data);

  Pylon::CBaslerUniversalGrabResultPtr * ptr_grab_result =
      static_cast < Pylon::CBaslerUniversalGrabResultPtr * >(data);
  delete ptr_grab_result;
}

gboolean
gst_pylon_capture (GstPylon * self, GstBuffer ** buf, GError ** err)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (buf, FALSE);
  g_return_val_if_fail (err || *err == NULL, FALSE);

  Pylon::CBaslerUniversalGrabResultPtr ptr_grab_result;
  gint timeout_ms = 5000;
  /* Catch the timeout exception if any */
  try {
    self->camera.RetrieveResult (timeout_ms, ptr_grab_result,
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

  gsize buffer_size = ptr_grab_result->GetBufferSize ();
  Pylon::CBaslerUniversalGrabResultPtr * persistent_ptr_grab_result =
      new Pylon::CBaslerUniversalGrabResultPtr (ptr_grab_result);
  *buf =
      gst_buffer_new_wrapped_full (static_cast < GstMemoryFlags > (0),
      ptr_grab_result->GetBuffer (), buffer_size, 0, buffer_size,
      persistent_ptr_grab_result,
      static_cast < GDestroyNotify > (free_ptr_grab_result));

  return TRUE;
}
