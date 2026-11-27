/* Copyright (C) 2026 Basler AG
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

#pragma once

#include <gst/gst.h>
#include <gst/pylon/gstpylonincludes.h>

#include <string>

/* Camera device profile used to select prior introspection knowledge. */
enum class GstPylonDeviceProfile {
  kGeneric,    /* Generic / legacy camera, CamEmu, or StreamGrabber */
  kBaslerGen2, /* Basler 2nd-gen cameras (ace 2, dart 2, boost, racer 2 / 2S) */
};

/* Prior knowledge about how TLParamsLocked=1 affects feature writability. */
enum class GstPylonTlLockBehavior {
  kUnknown,         /* Probe TLParamsLocked live */
  kUnlockedPlaying, /* Never locked by TLParamsLocked -> MUTABLE_PLAYING */
  kLockedPlaying,   /* Always locked by TLParamsLocked -> MUTABLE_READY */
};

/* Strategy for resolving global min/max limits without Cartesian probing. */
enum class GstPylonLimitStrategy {
  kDynamicSearch,          /* Fallback: run invalidator Cartesian search */
  kQueryCurrent,           /* Single GetMin()/GetMax() read; no permutations */
  kFixedRange,             /* Constant device-wide [min, max] bounds */
  kOffsetXFromSensor,      /* [0, SensorWidth - Width.GetInc()] */
  kOffsetYFromSensor,      /* [0, SensorHeight - Height.GetInc()] */
  kSizeFromSensorWidth,    /* [GetMin(), SensorWidth] */
  kSizeFromSensorHeight,   /* [GetMin(), SensorHeight] */
  kDerivedFromSiblingMin,  /* [min(cur, sibling.min), max(cur, sibling.max)] */
  kDerivedFromSiblingMax,  /* [0, max(cur.max, sibling.max)] */
  kDerivedFromSiblingBoth, /* [min(cur.min, sib.min), max(cur.max, sib.max)] */
};

struct GstPylonIntLimitRule {
  GstPylonLimitStrategy strategy = GstPylonLimitStrategy::kDynamicSearch;
  gint64 fixed_min = 0;
  gint64 fixed_max = 0;
  const char *sibling_node = nullptr;
};

struct GstPylonDoubleLimitRule {
  GstPylonLimitStrategy strategy = GstPylonLimitStrategy::kDynamicSearch;
  gdouble fixed_min = 0.0;
  gdouble fixed_max = 0.0;
  const char *sibling_node = nullptr;
};

/* Pure string-based detection (also used by camera-free unit tests). */
GstPylonDeviceProfile gst_pylon_detect_device_profile(
    const std::string &device_family_name,
    const std::string &device_firmware_version);

/* Detect device profile from a camera/stream-grabber GenApi nodemap. */
GstPylonDeviceProfile gst_pylon_detect_device_profile(
    GenApi::INodeMap &nodemap);

/* Query prior TLParamsLocked behavior for a feature. */
GstPylonTlLockBehavior gst_pylon_prior_tl_lock_behavior(
    GstPylonDeviceProfile profile, const std::string &feature_name);

/* Lookup declarative limit rules for integer / float features. */
GstPylonIntLimitRule gst_pylon_prior_int_limit_rule(
    GstPylonDeviceProfile profile, const std::string &feature_name);

GstPylonDoubleLimitRule gst_pylon_prior_double_limit_rule(
    GstPylonDeviceProfile profile, const std::string &feature_name);

/* Try to resolve integer / double global limits using prior knowledge.
 * Returns TRUE if resolved, FALSE if caller should fall back to dynamic
 * Cartesian search. */
gboolean gst_pylon_prior_resolve_int_limits(GstPylonDeviceProfile profile,
                                            GenApi::INode *node,
                                            gint64 &min_out, gint64 &max_out);

gboolean gst_pylon_prior_resolve_double_limits(GstPylonDeviceProfile profile,
                                               GenApi::INode *node,
                                               gdouble &min_out,
                                               gdouble &max_out);
