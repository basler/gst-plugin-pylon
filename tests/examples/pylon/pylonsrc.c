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
 * Example showing usage of the pylonsrc element
 */

#include <gst/gst.h>
#include <glib-unix.h>

#define PYLONSRC_NAME "src"

static gboolean
sig_handler (GMainLoop * loop)
{
  g_return_val_if_fail (loop, FALSE);

  g_print ("Interrupt caught, exiting...");
  g_main_loop_quit (loop);

  return TRUE;
}

static gboolean
toggle_pattern (GstElement * pylonsrc)
{
  gint pattern = 0;
  GstChildProxy *cp = NULL;

  g_return_val_if_fail (pylonsrc, FALSE);

  cp = GST_CHILD_PROXY (pylonsrc);

  gst_child_proxy_get (cp, "cam::TestImageSelector", &pattern, NULL);

  /* Toggle test image pattern */
  pattern = 1 == pattern ? 2 : 1;

  gst_child_proxy_set (cp, "cam::TestImageSelector", pattern, NULL);

  return TRUE;
}

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
bus_callback (GstBus * bus, GstMessage * msg, GMainLoop * loop)
{
  GError *err = NULL;
  gchar *dbg_info = NULL;

  g_return_val_if_fail (loop, FALSE);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &err, &dbg_info);
      print_error (msg, err, dbg_info, "ERROR");
      /* Treat errors as fatal and tear down the app */
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (msg, &err, &dbg_info);
      print_error (msg, err, dbg_info, "WARNING");
      break;
    default:
      break;
  }

  return TRUE;
}

int
main (int argc, char **argv)
{
  GMainLoop *loop = NULL;
  GstElement *pipe = NULL;
  GstElement *pylonsrc = NULL;
  GstBus *bus = NULL;
  GError *error = NULL;
  const gchar *desc =
      "pylonsrc device-serial-number=0815-0000 name=" PYLONSRC_NAME
      " ! queue ! videoconvert ! autovideosink";
  gint ret = EXIT_FAILURE;

  /* Make sure we have an emulator running */
  g_setenv ("PYLON_CAMEMU", "1", TRUE);

  gst_init (&argc, &argv);

  pipe =
      gst_parse_launch_full (desc, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
  if (!pipe) {
    g_printerr ("Unable to create pipeline: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  loop = g_main_loop_new (NULL, FALSE);
  g_unix_signal_add (SIGINT, G_SOURCE_FUNC (sig_handler), loop);

  /* Add a bus listener to receive errors, warnings and other notifications
   * from the pipeline
   */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  gst_bus_add_watch (bus, (GstBusFunc) bus_callback, loop);
  gst_object_unref (bus);

  pylonsrc = gst_bin_get_by_name (GST_BIN (pipe), PYLONSRC_NAME);
  if (!pylonsrc) {
    g_printerr ("No pylonsrc element found\n");
    goto free_pipe;
  }

  if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (pipe,
          GST_STATE_PLAYING)) {
    g_printerr ("Unable to play pipeline\n");
    goto free_pylonsrc;
  }

  /* Add a periodic callback to change a camera property */
  g_timeout_add_seconds (1, G_SOURCE_FUNC (toggle_pattern), pylonsrc);

  /* Run until an interrupt is received */
  g_main_loop_run (loop);

  gst_element_set_state (pipe, GST_STATE_NULL);

  g_main_loop_unref (loop);
  ret = EXIT_SUCCESS;

  g_print ("bye!\n");

free_pylonsrc:
  gst_object_unref (pylonsrc);

free_pipe:
  gst_object_unref (pipe);

out:
  return ret;
}
