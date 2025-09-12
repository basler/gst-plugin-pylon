/* Copyright (C) 2023 Basler AG
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

#ifndef GSTPYLONFASTPARAMFACTORY_H
#define GSTPYLONFASTPARAMFACTORY_H

#include <gst/gst.h>
#include <gst/pylon/gstpylonincludes.h>

/**
 * GstPylonFastParamFactory:
 *
 * A fast parameter factory that creates GParamSpecs without expensive
 * heuristics. This provides direct mapping with full ranges for numeric
 * types and basic access checking.
 *
 * Trade-offs:
 * - Integer properties use full G_MININT64/G_MAXINT64 range instead of GenICam
 * limits
 * - Float properties use large range (-1e15 to 1e15) instead of GenICam limits
 * - No complex feature interdependency analysis
 * - No caching of computed ranges
 * - Significantly faster property installation
 * - Default values for selector properties use safe defaults (0, FALSE, "",
 * etc.)
 *
 * IMPORTANT: While property creation uses simplified ranges and default values,
 * actual property reading (via gst_pylon_object_get_property) always fetches
 * real values from the camera with proper selector handling. Fast mode only
 * affects the initial property setup, not runtime value retrieval.
 */
class GstPylonFastParamFactory {
 public:
  GstPylonFastParamFactory(GenApi::INodeMap &nodemap,
                           const std::string &device_fullname)
      : nodemap(nodemap), device_fullname(device_fullname){};

  GParamSpec *make_param(GenApi::INode *node, GenApi::INode *selector,
                         guint64 selector_value);

 private:
  GParamSpec *gst_pylon_make_fast_spec_int64(GenApi::INode *node);
  GParamSpec *gst_pylon_make_fast_spec_selector_int64(GenApi::INode *node,
                                                      GenApi::INode *selector,
                                                      guint64 selector_value);
  GParamSpec *gst_pylon_make_fast_spec_bool(GenApi::INode *node);
  GParamSpec *gst_pylon_make_fast_spec_selector_bool(GenApi::INode *node,
                                                     GenApi::INode *selector,
                                                     guint64 selector_value);
  GParamSpec *gst_pylon_make_fast_spec_double(GenApi::INode *node);
  GParamSpec *gst_pylon_make_fast_spec_selector_double(GenApi::INode *node,
                                                       GenApi::INode *selector,
                                                       guint64 selector_value);
  GParamSpec *gst_pylon_make_fast_spec_str(GenApi::INode *node);
  GParamSpec *gst_pylon_make_fast_spec_selector_str(GenApi::INode *node,
                                                    GenApi::INode *selector,
                                                    guint64 selector_value);
  GType gst_pylon_make_fast_enum_type(GenApi::INode *node);
  GParamSpec *gst_pylon_make_fast_spec_enum(GenApi::INode *node);
  GParamSpec *gst_pylon_make_fast_spec_selector_enum(GenApi::INode *node,
                                                     GenApi::INode *selector,
                                                     guint64 selector_value);

  GParamFlags gst_pylon_query_fast_access(GenApi::INode *node);

 private:
  GenApi::INodeMap &nodemap;
  const std::string &device_fullname;
};

#endif  // GSTPYLONFASTPARAMFACTORY_H
