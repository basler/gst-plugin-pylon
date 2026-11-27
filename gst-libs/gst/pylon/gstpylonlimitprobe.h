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

#ifndef _GST_PYLON_LIMIT_PROBE_H_
#define _GST_PYLON_LIMIT_PROBE_H_

#include <glib.h>
#include <gst/pylon/gstpylonfeatureprior.h>
#include <gst/pylon/gstpylonincludes.h>

#include <string>

gboolean gst_pylon_try_prior_limits(GstPylonDeviceProfile device_profile,
                                    GenApi::INode* node,
                                    gint64& minimum_under_all_settings,
                                    gint64& maximum_under_all_settings);
gboolean gst_pylon_try_prior_limits(GstPylonDeviceProfile device_profile,
                                    GenApi::INode* node,
                                    gdouble& minimum_under_all_settings,
                                    gdouble& maximum_under_all_settings);

gint64 gst_pylon_query_integer_limit(GenApi::INode* node,
                                     const std::string& limit);
gdouble gst_pylon_query_double_limit(GenApi::INode* node,
                                     const std::string& limit);

void gst_pylon_probe_integer_limits(GenApi::INode* node,
                                    gint64& minimum_under_all_settings,
                                    gint64& maximum_under_all_settings,
                                    GstPylonDeviceProfile device_profile);
void gst_pylon_probe_double_limits(GenApi::INode* node,
                                   gdouble& minimum_under_all_settings,
                                   gdouble& maximum_under_all_settings,
                                   GstPylonDeviceProfile device_profile);

#endif
