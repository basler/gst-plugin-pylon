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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

static GList *elements = NULL;

static void
setup (void)
{
  GList *features, *f;
  GList *plugins, *p;
  gchar **ignorelist = NULL;
  const gchar *STATE_IGNORE_ELEMENTS = NULL;
  GstRegistry *def;

  GST_DEBUG ("getting elements for package %s", PACKAGE);
  STATE_IGNORE_ELEMENTS = g_getenv ("GST_STATE_IGNORE_ELEMENTS");
  if (!g_getenv ("GST_NO_STATE_IGNORE_ELEMENTS") && STATE_IGNORE_ELEMENTS) {
    GST_DEBUG ("Will ignore element factories: '%s'", STATE_IGNORE_ELEMENTS);
    ignorelist = g_strsplit (STATE_IGNORE_ELEMENTS, " ", 0);
  }

  def = gst_registry_get ();

  plugins = gst_registry_get_plugin_list (def);

  for (p = plugins; p; p = p->next) {
    GstPlugin *plugin = p->data;

    if (strcmp (gst_plugin_get_source (plugin), PACKAGE) != 0)
      continue;

    features =
        gst_registry_get_feature_list_by_plugin (def,
        gst_plugin_get_name (plugin));

    for (f = features; f; f = f->next) {
      GstPluginFeature *feature = f->data;
      const gchar *name = gst_plugin_feature_get_name (feature);
      gboolean ignore = FALSE;

      if (!GST_IS_ELEMENT_FACTORY (feature))
        continue;

      if (ignorelist) {
        gchar **s;

        for (s = ignorelist; s && *s; ++s) {
          if (g_str_has_prefix (name, *s)) {
            GST_DEBUG ("ignoring element %s", name);
            ignore = TRUE;
          }
        }
        if (ignore)
          continue;
      }

      GST_DEBUG ("adding element %s", name);
      elements = g_list_prepend (elements, (gpointer) g_strdup (name));
    }
    gst_plugin_feature_list_free (features);
  }
  gst_plugin_list_free (plugins);
  g_strfreev (ignorelist);
}

static void
teardown (void)
{
  GList *e;

  for (e = elements; e; e = e->next) {
    g_free (e->data);
  }
  g_list_free (elements);
  elements = NULL;
}


GST_START_TEST (test_state_changes_up_and_down_seq)
{
  GstElement *element;
  GList *e;

  for (e = elements; e; e = e->next) {
    const gchar *name = e->data;

    GST_INFO ("testing element %s", name);
    element = gst_element_factory_make (name, name);
    fail_if (element == NULL, "Could not make element from factory %s", name);

    if (GST_IS_PIPELINE (element)) {
      GST_DEBUG ("element %s is a pipeline", name);
    }

    gst_element_set_state (element, GST_STATE_READY);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_PLAYING);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_READY);
    gst_element_set_state (element, GST_STATE_NULL);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_READY);
    gst_element_set_state (element, GST_STATE_PLAYING);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (element));
  }
}

GST_END_TEST;

GST_START_TEST (test_state_changes_up_seq)
{
  GstElement *element;
  GList *e;

  for (e = elements; e; e = e->next) {
    const gchar *name = e->data;

    GST_INFO ("testing element %s", name);
    element = gst_element_factory_make (name, name);
    fail_if (element == NULL, "Could not make element from factory %s", name);

    if (GST_IS_PIPELINE (element)) {
      GST_DEBUG ("element %s is a pipeline", name);
    }

    gst_element_set_state (element, GST_STATE_READY);

    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_READY);

    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_PLAYING);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_READY);

    gst_element_set_state (element, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (element));
  }
}

GST_END_TEST;

GST_START_TEST (test_state_changes_down_seq)
{
  GstElement *element;
  GList *e;

  for (e = elements; e; e = e->next) {
    const gchar *name = e->data;

    GST_INFO ("testing element %s", name);
    element = gst_element_factory_make (name, name);
    fail_if (element == NULL, "Could not make element from factory %s", name);

    if (GST_IS_PIPELINE (element)) {
      GST_DEBUG ("element %s is a pipeline", name);
    }

    gst_element_set_state (element, GST_STATE_READY);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_PLAYING);

    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_PLAYING);

    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_READY);
    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_PLAYING);

    gst_element_set_state (element, GST_STATE_PAUSED);
    gst_element_set_state (element, GST_STATE_READY);
    gst_element_set_state (element, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (element));
  }
}

GST_END_TEST;


static Suite *
states_suite (void)
{
  Suite *s = suite_create ("states_good");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_state_changes_up_and_down_seq);
  tcase_add_test (tc_chain, test_state_changes_up_seq);
  tcase_add_test (tc_chain, test_state_changes_down_seq);

  return s;
}

GST_CHECK_MAIN (states);
