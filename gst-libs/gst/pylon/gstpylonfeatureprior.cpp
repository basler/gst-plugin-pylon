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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstpylondebug.h"
#include "gstpylonfeatureprior.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace {

/* Check for the structured DeviceFirmwareVersion format exposed by
 * second-generation Basler cameras: "p=.../s=.../v=...". */
bool is_structured_firmware_version(const std::string &fw_version) {
  if (fw_version.rfind("p=", 0) != 0) {
    return false;
  }
  const auto s_pos = fw_version.find("/s=");
  if (s_pos == std::string::npos) {
    return false;
  }
  const auto v_pos = fw_version.find("/v=", s_pos + 3);
  return v_pos != std::string::npos;
}

/* Camera families (DeviceFamilyName) that belong exclusively to the
 * second-generation platform. */
const std::unordered_set<std::string> &gen2_exclusive_families() {
  static const std::unordered_set<std::string> families = {
      "ace 2",
      "boost",
      "racer 2",
      "racer 2S",
  };
  return families;
}

/* Camera families (DeviceFamilyName) shared across first- and
 * second-generation models (e.g. dart 1st gen vs. dart 2), which therefore
 * require the structured DeviceFirmwareVersion format to disambiguate. */
const std::unordered_set<std::string> &gen2_shared_families() {
  static const std::unordered_set<std::string> families = {
      "dart",
  };
  return families;
}

/* Features that are always locked when TLParamsLocked is set to 1. */
const std::unordered_set<std::string> &gen2_tl_always_locked() {
  static const std::unordered_set<std::string> features = {
      "Width",
      "Height",
      "PixelFormat",
      "BslMultipleROIColumnsEnable",
      "BslMultipleROIColumnSize",
      "BslMultipleROIColumnOffset",
      "BslMultipleROIRowsEnable",
      "BslMultipleROIRowSize",
      "BslMultipleROIRowOffset",
      "BinningHorizontal",
      "BinningHorizontalMode",
      "BinningVertical",
      "BinningVerticalMode",
      "ImageCompressionMode",
      "ImageCompressionRateOption",
      "BslImageCompressionRatio",
      "ChunkModeActive",
      "ChunkEnable",
      "BslFlatFieldCorrectionMode",
      "BslScalingFactor",
      "GenDCStreamingMode",
      "TestPayloadFormatMode",
      "UserSetDefault",
      "UserSetSelector",
  };
  return features;
}

/* Features whose TLParamsLocked dependency depends on runtime/sensor state
 * (e.g. shutter mode or demosaicing/scaling mode) and therefore still require
 * a live TLParamsLocked probe. */
const std::unordered_set<std::string> &gen2_tl_conditionally_locked() {
  static const std::unordered_set<std::string> features = {
      "TriggerMode",
      "TriggerSource",
      "TriggerActivation",
      "BslDemosaicingMethod",
  };
  return features;
}

/* Generic workarounds preserved for first-generation / non-profiled cameras. */
const std::unordered_map<std::string, GstPylonIntLimitRule> &
generic_int_rules() {
  static const std::unordered_map<std::string, GstPylonIntLimitRule> rules = {
      {"OffsetX", {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"OffsetY", {GstPylonLimitStrategy::kOffsetYFromSensor}},
      {"AutoFunctionROIOffsetX", {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"AutoFunctionAOIOffsetX", {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"AutoFunctionROIOffsetY", {GstPylonLimitStrategy::kOffsetYFromSensor}},
      {"AutoFunctionAOIOffsetY", {GstPylonLimitStrategy::kOffsetYFromSensor}},
      {"AutoFunctionROIWidth", {GstPylonLimitStrategy::kSizeFromSensorWidth}},
      {"AutoFunctionAOIWidth", {GstPylonLimitStrategy::kSizeFromSensorWidth}},
      {"AutoFunctionROIHeight", {GstPylonLimitStrategy::kSizeFromSensorHeight}},
      {"AutoFunctionAOIHeight", {GstPylonLimitStrategy::kSizeFromSensorHeight}},
      {"AcquisitionBurstFrameCount",
       {GstPylonLimitStrategy::kFixedRange, 1, 1023}},
      {"ExposureTime", {GstPylonLimitStrategy::kFixedRange, 1, 10000000}},
      {"BlackLevel", {GstPylonLimitStrategy::kFixedRange, 0, 4095}},
      {"GevSCBWR", {GstPylonLimitStrategy::kFixedRange, 0, 100}},
      {"GevSCBWRA", {GstPylonLimitStrategy::kFixedRange, 1, 512}},
      {"GevSCPD", {GstPylonLimitStrategy::kFixedRange, 0, 50000000}},
      {"GevSCFTD", {GstPylonLimitStrategy::kFixedRange, 0, 50000000}},
  };
  return rules;
}

const std::unordered_map<std::string, GstPylonDoubleLimitRule> &
generic_double_rules() {
  static const std::unordered_map<std::string, GstPylonDoubleLimitRule> rules =
      {
          {"ExposureTime", {GstPylonLimitStrategy::kFixedRange, 1.0, 1.0e7}},
          {"BlackLevel", {GstPylonLimitStrategy::kFixedRange, 0.0, 4095.0}},
          {"BslColorAdjustmentHue",
           {GstPylonLimitStrategy::kFixedRange, -1.0, 1.0}},
          {"BslColorAdjustmentSaturation",
           {GstPylonLimitStrategy::kFixedRange, 0.0, 2.0}},
      };
  return rules;
}

/* Integer limit rules for second-generation Basler cameras. */
const std::unordered_map<std::string, GstPylonIntLimitRule> &gen2_int_rules() {
  static const std::unordered_map<std::string, GstPylonIntLimitRule> rules = {
      /* ROI / sensor-derived bounds */
      {"OffsetX", {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"OffsetY", {GstPylonLimitStrategy::kOffsetYFromSensor}},
      {"AutoFunctionROIOffsetX", {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"AutoFunctionAOIOffsetX", {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"AutoFunctionROIOffsetY", {GstPylonLimitStrategy::kOffsetYFromSensor}},
      {"AutoFunctionAOIOffsetY", {GstPylonLimitStrategy::kOffsetYFromSensor}},
      {"AutoFunctionROIWidth", {GstPylonLimitStrategy::kSizeFromSensorWidth}},
      {"AutoFunctionAOIWidth", {GstPylonLimitStrategy::kSizeFromSensorWidth}},
      {"AutoFunctionROIHeight", {GstPylonLimitStrategy::kSizeFromSensorHeight}},
      {"AutoFunctionAOIHeight", {GstPylonLimitStrategy::kSizeFromSensorHeight}},
      {"BslShadingCorrectionROIOffsetX",
       {GstPylonLimitStrategy::kOffsetXFromSensor}},
      {"BslShadingCorrectionROIWidth",
       {GstPylonLimitStrategy::kSizeFromSensorWidth}},

      /* Fixed firmware-wide bounds */
      {"AcquisitionBurstFrameCount",
       {GstPylonLimitStrategy::kFixedRange, 1, 1023}},
      {"GevSCBWR", {GstPylonLimitStrategy::kFixedRange, 0, 100}},
      {"GevSCBWRA", {GstPylonLimitStrategy::kFixedRange, 1, 512}},
      {"GevSCPD", {GstPylonLimitStrategy::kFixedRange, 0, 50000000}},
      {"GevSCFTD", {GstPylonLimitStrategy::kFixedRange, 0, 4194303}},

      /* Combined binning: BinningVertical range may be locked to
       * BinningHorizontal's current value, so union with BinningHorizontal. */
      {"BinningVertical",
       {GstPylonLimitStrategy::kDerivedFromSiblingBoth, 0, 0,
        "BinningHorizontal"}},

      /* Static-on-device limits: a single GetMin()/GetMax() read returns the
       * exact camera-specific bounds with zero permutations. */
      {"ActionDeviceKey", {GstPylonLimitStrategy::kQueryCurrent}},
      {"ActionGroupKey", {GstPylonLimitStrategy::kQueryCurrent}},
      {"ActionGroupMask", {GstPylonLimitStrategy::kQueryCurrent}},
      {"ActionQueueSize", {GstPylonLimitStrategy::kQueryCurrent}},
      {"ActionSelector", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BinningHorizontal", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslFlatFieldCorrectionSetIndex",
       {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslFrequencyConverterMultiplier",
       {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslFrequencyConverterPostDivider",
       {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslFrequencyConverterPreDivider",
       {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslHueRaw", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslPtpPriority1", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslPtpServoLockedThreshold", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslSaturationRaw", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslSerialTransferLength", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslSpatialCorrectionStride", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslTwiTargetAddress", {GstPylonLimitStrategy::kQueryCurrent}},
      {"BslTwiTransferLength", {GstPylonLimitStrategy::kQueryCurrent}},
      {"CounterDuration", {GstPylonLimitStrategy::kQueryCurrent}},
      {"CounterValue", {GstPylonLimitStrategy::kQueryCurrent}},
      {"CxpConnectionTestErrorCount", {GstPylonLimitStrategy::kQueryCurrent}},
      {"CxpConnectionTestPacketCount", {GstPylonLimitStrategy::kQueryCurrent}},
      {"CxpErrorCounterValue", {GstPylonLimitStrategy::kQueryCurrent}},
      {"DeviceLinkThroughputLimit", {GstPylonLimitStrategy::kQueryCurrent}},
      {"DigitalShift", {GstPylonLimitStrategy::kQueryCurrent}},
      {"EncoderDivider", {GstPylonLimitStrategy::kQueryCurrent}},
      {"EncoderValue", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevCurrentDefaultGateway", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevCurrentIPAddress", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevCurrentSubnetMask", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevPersistentDefaultGateway", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevPersistentIPAddress", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevPersistentSubnetMask", {GstPylonLimitStrategy::kQueryCurrent}},
      {"GevSCPSPacketSize", {GstPylonLimitStrategy::kQueryCurrent}},
      {"HeightMax", {GstPylonLimitStrategy::kQueryCurrent}},
      {"LUTIndex", {GstPylonLimitStrategy::kQueryCurrent}},
      {"LUTValue", {GstPylonLimitStrategy::kQueryCurrent}},
      {"PayloadSize", {GstPylonLimitStrategy::kQueryCurrent}},
      {"PixelDynamicRangeMax", {GstPylonLimitStrategy::kQueryCurrent}},
      {"PixelDynamicRangeMin", {GstPylonLimitStrategy::kQueryCurrent}},
      {"SensorHeight", {GstPylonLimitStrategy::kQueryCurrent}},
      {"SensorWidth", {GstPylonLimitStrategy::kQueryCurrent}},
      {"TestPendingAck", {GstPylonLimitStrategy::kQueryCurrent}},
      {"UserDefinedValue", {GstPylonLimitStrategy::kQueryCurrent}},
      {"UserOutputValueAll", {GstPylonLimitStrategy::kQueryCurrent}},
      {"WidthMax", {GstPylonLimitStrategy::kQueryCurrent}},
  };
  return rules;
}

/* Float limit rules for second-generation Basler cameras. */
const std::unordered_map<std::string, GstPylonDoubleLimitRule> &
gen2_double_rules() {
  static const std::unordered_map<std::string, GstPylonDoubleLimitRule> rules =
      {
          /* Fixed firmware-wide bounds */
          {"ExposureTime", {GstPylonLimitStrategy::kFixedRange, 1.0, 1.0e7}},
          {"BlackLevel", {GstPylonLimitStrategy::kFixedRange, 0.0, 4095.0}},
          {"BslColorAdjustmentHue",
           {GstPylonLimitStrategy::kFixedRange, -1.0, 1.0}},
          {"BslColorAdjustmentSaturation",
           {GstPylonLimitStrategy::kFixedRange, 0.0, 2.0}},

          /* Sibling-derived bounds (upper limit depends on lower limit,
           * periodic signal delay depends on period) */
          {"AutoGainUpperLimit",
           {GstPylonLimitStrategy::kDerivedFromSiblingMin, 0.0, 0.0,
            "AutoGainLowerLimit"}},
          {"AutoExposureTimeUpperLimit",
           {GstPylonLimitStrategy::kDerivedFromSiblingMin, 0.0, 0.0,
            "AutoExposureTimeLowerLimit"}},
          {"BslPeriodicSignalDelay",
           {GstPylonLimitStrategy::kDerivedFromSiblingMax, 0.0, 0.0,
            "BslPeriodicSignalPeriod"}},

          /* Static-on-device limits */
          {"AcquisitionLineRate", {GstPylonLimitStrategy::kQueryCurrent}},
          {"AutoExposureTimeLowerLimit",
           {GstPylonLimitStrategy::kQueryCurrent}},
          {"AutoGainLowerLimit", {GstPylonLimitStrategy::kQueryCurrent}},
          {"AutoTargetBrightness", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BalanceRatio", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslBrightness", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslContrast", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslHDRBeyondRatio", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslHue", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslImageCompressionRatio", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslInputFilterTime", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslInputHoldOffTime", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslLineNoiseReduction", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslLineTimeout", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslNoiseReduction", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslPeriodicSignalPeriod", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslPixelCorrectionBeyond", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslSaturation", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslScalingFactor", {GstPylonLimitStrategy::kQueryCurrent}},
          {"BslSharpnessEnhancement", {GstPylonLimitStrategy::kQueryCurrent}},
          {"ColorTransformationValue", {GstPylonLimitStrategy::kQueryCurrent}},
          {"Gain", {GstPylonLimitStrategy::kQueryCurrent}},
          {"Gamma", {GstPylonLimitStrategy::kQueryCurrent}},
          {"TimerDelay", {GstPylonLimitStrategy::kQueryCurrent}},
          {"TimerDuration", {GstPylonLimitStrategy::kQueryCurrent}},
          {"TimerTriggerArmDelay", {GstPylonLimitStrategy::kQueryCurrent}},
          {"TriggerDelay", {GstPylonLimitStrategy::kQueryCurrent}},
      };
  return rules;
}

template <class P, class T>
void widen_to_include_current(P &param, T &min_val, T &max_val) {
  if (param.IsReadable()) {
    const T cur = param.GetValue();
    if (cur < min_val) {
      min_val = cur;
    }
    if (cur > max_val) {
      max_val = cur;
    }
  }
}

}  // namespace

GstPylonDeviceProfile gst_pylon_detect_device_profile(
    const std::string &device_family_name,
    const std::string &device_firmware_version) {
  if (gen2_exclusive_families().count(device_family_name) > 0) {
    return GstPylonDeviceProfile::kBaslerGen2;
  }

  if (gen2_shared_families().count(device_family_name) > 0 &&
      is_structured_firmware_version(device_firmware_version)) {
    return GstPylonDeviceProfile::kBaslerGen2;
  }

  return GstPylonDeviceProfile::kGeneric;
}

GstPylonDeviceProfile gst_pylon_detect_device_profile(
    GenApi::INodeMap &nodemap) {
  std::string family_name;
  std::string fw_version;

  try {
    Pylon::CStringParameter family_param(nodemap, "DeviceFamilyName");
    if (family_param.IsReadable()) {
      family_name = family_param.GetValue().c_str();
    }
  } catch (const Pylon::GenericException &) {
    /* StreamGrabber or non-SFNC nodemaps may not expose DeviceFamilyName */
  }

  try {
    Pylon::CStringParameter fw_param(nodemap, "DeviceFirmwareVersion");
    if (fw_param.IsReadable()) {
      fw_version = fw_param.GetValue().c_str();
    }
  } catch (const Pylon::GenericException &) {
    /* StreamGrabber or non-SFNC nodemaps may not expose DeviceFirmwareVersion
     */
  }

  const GstPylonDeviceProfile detected =
      gst_pylon_detect_device_profile(family_name, fw_version);
  GST_DEBUG("Detected device profile %s (DeviceFamilyName='%s', FW='%s')",
            detected == GstPylonDeviceProfile::kBaslerGen2 ? "basler-gen2"
                                                           : "generic",
            family_name.c_str(), fw_version.c_str());
  return detected;
}

GstPylonTlLockBehavior gst_pylon_prior_tl_lock_behavior(
    GstPylonDeviceProfile profile, const std::string &feature_name) {
  if (profile != GstPylonDeviceProfile::kBaslerGen2) {
    return GstPylonTlLockBehavior::kUnknown;
  }

  if (gen2_tl_always_locked().count(feature_name) > 0) {
    return GstPylonTlLockBehavior::kLockedPlaying;
  }

  if (gen2_tl_conditionally_locked().count(feature_name) > 0) {
    return GstPylonTlLockBehavior::kUnknown;
  }

  return GstPylonTlLockBehavior::kUnlockedPlaying;
}

GstPylonIntLimitRule gst_pylon_prior_int_limit_rule(
    GstPylonDeviceProfile profile, const std::string &feature_name) {
  const auto &table = (profile == GstPylonDeviceProfile::kBaslerGen2)
                          ? gen2_int_rules()
                          : generic_int_rules();
  const auto it = table.find(feature_name);
  if (it != table.end()) {
    return it->second;
  }
  return GstPylonIntLimitRule{};
}

GstPylonDoubleLimitRule gst_pylon_prior_double_limit_rule(
    GstPylonDeviceProfile profile, const std::string &feature_name) {
  const auto &table = (profile == GstPylonDeviceProfile::kBaslerGen2)
                          ? gen2_double_rules()
                          : generic_double_rules();
  const auto it = table.find(feature_name);
  if (it != table.end()) {
    return it->second;
  }
  return GstPylonDoubleLimitRule{};
}

gboolean gst_pylon_prior_resolve_int_limits(GstPylonDeviceProfile profile,
                                            GenApi::INode *node,
                                            gint64 &min_out, gint64 &max_out) {
  g_return_val_if_fail(node, FALSE);

  const std::string name(node->GetName().c_str());
  const GstPylonIntLimitRule rule =
      gst_pylon_prior_int_limit_rule(profile, name);
  if (rule.strategy == GstPylonLimitStrategy::kDynamicSearch) {
    return FALSE;
  }

  GenApi::INodeMap *nodemap = node->GetNodeMap();
  Pylon::CIntegerParameter param(node);

  switch (rule.strategy) {
    case GstPylonLimitStrategy::kQueryCurrent:
      min_out = param.GetMin();
      max_out = param.GetMax();
      widen_to_include_current(param, min_out, max_out);
      GST_DEBUG("Prior (QueryCurrent) for %s: [%" G_GINT64_FORMAT
                ", %" G_GINT64_FORMAT "]",
                name.c_str(), min_out, max_out);
      return TRUE;

    case GstPylonLimitStrategy::kFixedRange:
      min_out = rule.fixed_min;
      max_out = rule.fixed_max;
      widen_to_include_current(param, min_out, max_out);
      GST_DEBUG("Prior (FixedRange) for %s: [%" G_GINT64_FORMAT
                ", %" G_GINT64_FORMAT "]",
                name.c_str(), min_out, max_out);
      return TRUE;

    case GstPylonLimitStrategy::kOffsetXFromSensor: {
      Pylon::CIntegerParameter sensor_width(nodemap->GetNode("SensorWidth"));
      Pylon::CIntegerParameter width(nodemap->GetNode("Width"));
      if (sensor_width.IsValid() && width.IsValid()) {
        min_out = 0;
        max_out = sensor_width.GetValue() - width.GetInc();
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (OffsetXFromSensor) for %s: [%" G_GINT64_FORMAT
                  ", %" G_GINT64_FORMAT "]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    case GstPylonLimitStrategy::kOffsetYFromSensor: {
      Pylon::CIntegerParameter sensor_height(nodemap->GetNode("SensorHeight"));
      Pylon::CIntegerParameter height(nodemap->GetNode("Height"));
      if (sensor_height.IsValid() && height.IsValid()) {
        min_out = 0;
        max_out = sensor_height.GetValue() - height.GetInc();
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (OffsetYFromSensor) for %s: [%" G_GINT64_FORMAT
                  ", %" G_GINT64_FORMAT "]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    case GstPylonLimitStrategy::kSizeFromSensorWidth: {
      Pylon::CIntegerParameter sensor_width(nodemap->GetNode("SensorWidth"));
      if (sensor_width.IsValid()) {
        min_out = param.GetMin();
        max_out = sensor_width.GetValue();
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (SizeFromSensorWidth) for %s: [%" G_GINT64_FORMAT
                  ", %" G_GINT64_FORMAT "]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    case GstPylonLimitStrategy::kSizeFromSensorHeight: {
      Pylon::CIntegerParameter sensor_height(nodemap->GetNode("SensorHeight"));
      if (sensor_height.IsValid()) {
        min_out = param.GetMin();
        max_out = sensor_height.GetValue();
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (SizeFromSensorHeight) for %s: [%" G_GINT64_FORMAT
                  ", %" G_GINT64_FORMAT "]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    case GstPylonLimitStrategy::kDerivedFromSiblingBoth: {
      Pylon::CIntegerParameter sibling(nodemap->GetNode(rule.sibling_node));
      if (sibling.IsReadable()) {
        min_out = std::min<gint64>(param.GetMin(), sibling.GetMin());
        max_out = std::max<gint64>(param.GetMax(), sibling.GetMax());
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (DerivedFromSiblingBoth) for %s: [%" G_GINT64_FORMAT
                  ", %" G_GINT64_FORMAT "]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    default:
      return FALSE;
  }
}

gboolean gst_pylon_prior_resolve_double_limits(GstPylonDeviceProfile profile,
                                               GenApi::INode *node,
                                               gdouble &min_out,
                                               gdouble &max_out) {
  g_return_val_if_fail(node, FALSE);

  const std::string name(node->GetName().c_str());
  const GstPylonDoubleLimitRule rule =
      gst_pylon_prior_double_limit_rule(profile, name);
  if (rule.strategy == GstPylonLimitStrategy::kDynamicSearch) {
    return FALSE;
  }

  GenApi::INodeMap *nodemap = node->GetNodeMap();
  Pylon::CFloatParameter param(node);

  switch (rule.strategy) {
    case GstPylonLimitStrategy::kQueryCurrent:
      min_out = param.GetMin();
      max_out = param.GetMax();
      widen_to_include_current(param, min_out, max_out);
      GST_DEBUG("Prior (QueryCurrent) for %s: [%f, %f]", name.c_str(), min_out,
                max_out);
      return TRUE;

    case GstPylonLimitStrategy::kFixedRange:
      min_out = rule.fixed_min;
      max_out = rule.fixed_max;
      widen_to_include_current(param, min_out, max_out);
      GST_DEBUG("Prior (FixedRange) for %s: [%f, %f]", name.c_str(), min_out,
                max_out);
      return TRUE;

    case GstPylonLimitStrategy::kDerivedFromSiblingMin: {
      Pylon::CFloatParameter sibling(nodemap->GetNode(rule.sibling_node));
      if (sibling.IsReadable()) {
        min_out = std::min<gdouble>(param.GetMin(), sibling.GetMin());
        max_out = std::max<gdouble>(param.GetMax(), sibling.GetMax());
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (DerivedFromSiblingMin) for %s: [%f, %f]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    case GstPylonLimitStrategy::kDerivedFromSiblingMax: {
      Pylon::CFloatParameter sibling(nodemap->GetNode(rule.sibling_node));
      if (sibling.IsReadable()) {
        min_out = 0.0;
        max_out = std::max<gdouble>(param.GetMax(), sibling.GetMax());
        widen_to_include_current(param, min_out, max_out);
        GST_DEBUG("Prior (DerivedFromSiblingMax) for %s: [%f, %f]",
                  name.c_str(), min_out, max_out);
        return TRUE;
      }
      return FALSE;
    }

    default:
      return FALSE;
  }
}
