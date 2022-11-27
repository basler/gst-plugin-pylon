/* Copyright (C) 2022 Basler AG
 *
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

/*
 * Example showing how to read the metadata
 */

#include <gst/gst.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <ext/pylon/gstpylonmeta.h>

#define PYLONSRC_NAME "src"
#define SINK_NAME "sink"

typedef struct _Context Context;
struct _Context
{
  GMainLoop *loop;
  GstElement *pylonsrc;
  GstElement *fakesink;
};

#ifdef G_OS_UNIX
static gboolean
sig_handler (Context * ctx)
{
  g_return_val_if_fail (ctx, FALSE);
  g_return_val_if_fail (ctx->loop, FALSE);

  g_print ("Interrupt caught, exiting...");
  g_main_loop_quit (ctx->loop);

  return TRUE;
}
#endif

static void
print_error (GstMessage * msg, GError * error, gchar * dbg, const gchar * tag)
{
  g_return_if_fail (msg);
  g_return_if_fail (error);
  g_return_if_fail (tag);

  g_printerr ("%s from element %s: %s\n", tag, GST_OBJECT_NAME (msg->src),
      error->message);
  g_printerr ("Debugging info: %s\n", (dbg) ? dbg : "none");
  g_error_free (error);
  g_free (dbg);
}

static gboolean
bus_callback (GstBus * bus, GstMessage * msg, Context * ctx)
{
  GError *err = NULL;
  gchar *dbg_info = NULL;

  g_return_val_if_fail (ctx, FALSE);
  g_return_val_if_fail (ctx->loop, FALSE);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &err, &dbg_info);
      print_error (msg, err, dbg_info, "ERROR");
      /* Treat errors as fatal and tear down the app */
      g_main_loop_quit (ctx->loop);
      break;
    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (msg, &err, &dbg_info);
      print_error (msg, err, dbg_info, "WARNING");
      break;
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      printf ("end of stream\n");
      g_main_loop_quit (ctx->loop);
      break;
    default:
      break;
  }

  return TRUE;
}

static void
try_enable_all_chunks (Context * ctx)
{
  GParamSpec **property_specs = NULL;
  GObject *cam = NULL;
  guint num_properties = 0;
  GstChildProxy *cp = NULL;
  gboolean has_chunks = FALSE;

  g_return_if_fail (ctx);
  g_return_if_fail (ctx->pylonsrc);

  cp = GST_CHILD_PROXY (ctx->pylonsrc);
  cam = gst_child_proxy_get_child_by_name (cp, "cam");
  property_specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (cam),
      &num_properties);

  /* FIXME: how to check if a property is available?? */
  for (size_t i = 0; i < num_properties; i++) {
    const gchar *prop_name = g_param_spec_get_name (property_specs[i]);
    if (g_str_has_prefix (prop_name, "ChunkModeActive")) {
      printf ("try enable chunk mode\n");
      g_object_set (cam, prop_name, TRUE, NULL);
      has_chunks = TRUE;
      break;
    }
  }

  if (has_chunks) {
    for (size_t i = 0; i < num_properties; i++) {
      const gchar *prop_name = g_param_spec_get_name (property_specs[i]);
      if (g_str_has_prefix (prop_name, "ChunkEnable")) {
        printf ("try enable %s\n", prop_name);
        g_object_set (cam, prop_name, TRUE, NULL);
      }
    }
  }

}

static GstPylonMeta *
gst_buffer_get_pylon_meta (GstBuffer * buffer)
{
  GstPylonMeta *meta =
      (GstPylonMeta *) gst_buffer_get_meta (buffer, GST_PYLON_META_API_TYPE);
  return meta;
}


static GstPadProbeReturn
cb_have_data (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buffer;
  GstPylonMeta *meta = NULL;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);
  meta = (GstPylonMeta *) gst_buffer_get_pylon_meta (buffer);
  printf ("-----------------\n");
  printf ("block_id         %lu\n", meta->block_id);
  printf ("offsetx          %lu\n", meta->offset.offset_x);
  printf ("offsety          %lu\n", meta->offset.offset_y);
  printf ("camera_timestamp %lu\n", meta->timestamp);
  printf ("stride           %lu\n", meta->stride);

  /* show chunks embedded in the stream */
  for (int idx = 0; idx < gst_structure_n_fields (meta->chunks); idx++) {
    printf ("chunks_available %s\n", gst_structure_nth_field_name (meta->chunks,
            idx));

  }
  return GST_PAD_PROBE_OK;
}

int
main (int argc, char **argv)
{
  Context ctx = { 0 };
  GstElement *pipe = NULL;
  GstBus *bus = NULL;
  guint bus_watch = 0;
  GError *error = NULL;
  gint ret = EXIT_FAILURE;
  GstPad *pad;
  const gchar *desc =
      "pylonsrc capture-error=skip num-buffers=10 name=" PYLONSRC_NAME
      " ! fakesink name=" SINK_NAME;

  gst_init (&argc, &argv);

  pipe =
      gst_parse_launch_full (desc, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
  if (!pipe) {
    g_printerr ("Unable to create pipeline: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  ctx.pylonsrc = gst_bin_get_by_name (GST_BIN (pipe), PYLONSRC_NAME);
  if (!ctx.pylonsrc) {
    g_printerr ("No pylonsrc element found\n");
    goto free_pipe;
  }

  ctx.fakesink = gst_bin_get_by_name (GST_BIN (pipe), SINK_NAME);
  if (!ctx.fakesink) {
    g_printerr ("No fakesink element found\n");
    goto free_pylonsrc;
  }

  ctx.loop = g_main_loop_new (NULL, FALSE);
#ifdef G_OS_UNIX
  g_unix_signal_add (SIGINT, (GSourceFunc) sig_handler, &ctx);
#endif

  /* Add a bus listener to receive errors, warnings and other notifications
   * from the pipeline
   */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  bus_watch = gst_bus_add_watch (bus, (GstBusFunc) bus_callback, &ctx);
  gst_object_unref (bus);

  if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (pipe,
          GST_STATE_PLAYING)) {
    g_printerr ("Unable to play pipeline\n");
    goto free_overlay;
  }

  /* attach a probe to the output of pylonsrc */
  pad = gst_element_get_static_pad (ctx.fakesink, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) cb_have_data, NULL, NULL);
  gst_object_unref (pad);

  /* try to enable all chunks the camera supports */
  try_enable_all_chunks (&ctx);


  /* Run until an interrupt is received */
  g_main_loop_run (ctx.loop);

  gst_element_set_state (pipe, GST_STATE_NULL);

  g_main_loop_unref (ctx.loop);
  ret = EXIT_SUCCESS;

  g_print ("bye!\n");

free_overlay:
  gst_object_unref (ctx.fakesink);

free_pylonsrc:
  gst_object_unref (ctx.pylonsrc);

free_pipe:
  g_source_remove (bus_watch);
  gst_object_unref (pipe);

out:
  gst_deinit ();

  return ret;
}
