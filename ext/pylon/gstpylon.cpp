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

#include <map>

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

/* prototypes */
static void free_ptr_grab_result (gpointer data);
/* *INDENT-OFF* */
static std::string
gst_pylon_pfnc_to_gst (GenICam_3_1_Basler_pylon::gcstring genapi_format);

static std::vector <std::string>
gst_pylon_pfnc_list_to_gst (GenApi::StringList_t genapi_formats);
/* *INDENT-ON* */

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
  catch (const Pylon::GenericException & e)
  {
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

static
    std::string
gst_pylon_pfnc_to_gst (GenICam_3_1_Basler_pylon::gcstring genapi_format)
{
  /* *INDENT-OFF* */
  static const std::map <const GenICam_3_1_Basler_pylon::gcstring, const std::string> formats_map = {
        {"Mono8", "GRAY8"},
        {"Mono10", "GRAY16_LE"},
	{"Mono12", "GRAY16_LE"},
	{"RGB8Packed", "RGB"},
	{"BGR8Packed", "BGR"}
  };
  /* *INDENT-ON* */

  std::string gst_format;
  if (formats_map.find (genapi_format) != formats_map.end ()) {
    gst_format = formats_map.at (genapi_format);
  }

  return gst_format;
}

/* *INDENT-OFF* */
static std::vector <std::string>
/* *INDENT-ON* */

gst_pylon_pfnc_list_to_gst (GenApi::StringList_t genapi_formats)
{
  std::vector < std::string > formats_list;

for (const auto & fmt:genapi_formats) {
    std::string gst_fmt = gst_pylon_pfnc_to_gst (fmt);

    if (!gst_fmt.empty ()) {
      formats_list.push_back (gst_fmt);
    }
  }

  return formats_list;
}

GstCaps *
gst_pylon_query_configuration (GstPylon * self, GError ** err)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (err || *err == NULL, NULL);

  /* Query camera available parameters */
  GenApi::INodeMap & nodemap = self->camera.GetNodeMap ();
  Pylon::CEnumParameter pixelFormat (nodemap, "PixelFormat");
  GenApi::StringList_t genapi_formats;
  pixelFormat.GetSettableValues (genapi_formats);

  /* Convert GenApi formats to Gst formats */
  std::vector < std::string > gst_formats =
      gst_pylon_pfnc_list_to_gst (genapi_formats);

  if (gst_formats.empty ()) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        "Failed to find any supported pixel formats available");
    return NULL;
  }

  try {
    self->camera.OffsetX.TrySetToMinimum ();
    self->camera.OffsetY.TrySetToMinimum ();
  }
  catch (const Pylon::GenericException & e)
  {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
        e.GetDescription ());
    return NULL;
  }

  gint min_w = self->camera.Width.GetMin ();
  gint max_w = self->camera.Width.GetMax ();
  gint min_h = self->camera.Height.GetMin ();
  gint max_h = self->camera.Height.GetMax ();

  /* Build gst caps */
  GstCaps *caps = gst_caps_new_empty ();
  GstStructure *gst_structure = gst_structure_new_empty ("video/x-raw");
  GValue value = G_VALUE_INIT;

  /* Fill format field */
  GValue value_list = G_VALUE_INIT;
  g_value_init (&value_list, GST_TYPE_LIST);

for (const auto & fmt:gst_formats) {
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, fmt.c_str ());
    gst_value_list_append_value (&value_list, &value);
    g_value_unset (&value);
  }

  gst_structure_set_value (gst_structure, "format", &value_list);
  g_value_unset (&value_list);

  /* Fill width field */
  g_value_init (&value, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&value, min_w, max_w);
  gst_structure_set_value (gst_structure, "width", &value);
  g_value_unset (&value);

  /* Fill height field */
  g_value_init (&value, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&value, min_h, max_h);
  gst_structure_set_value (gst_structure, "height", &value);
  g_value_unset (&value);

  /* Fill framerate field */
  Pylon::CFloatParameter framerate (nodemap, "AcquisitionFrameRateAbs");
  gdouble min_fps = framerate.GetMin ();
  gdouble max_fps = framerate.GetMax ();

  gint min_fps_num = 0;
  gint min_fps_den = 0;
  gst_util_double_to_fraction (min_fps, &min_fps_num, &min_fps_den);

  gint max_fps_num = 0;
  gint max_fps_den = 0;
  gst_util_double_to_fraction (max_fps, &max_fps_num, &max_fps_den);

  g_value_init (&value, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&value, min_fps_num, min_fps_den,
      max_fps_num, max_fps_den);
  gst_structure_set_value (gst_structure, "framerate", &value);
  g_value_unset (&value);

  gst_caps_append_structure (caps, gst_structure);

  return caps;
}
