/* Copyright (C) 2026 Basler AG
 *
 * Unit tests for device-profile feature priors (profile detection,
 * TLParamsLocked behavior, and dynamic limit rules).
 */

#include "../../../gst-libs/gst/pylon/gstpylonfeatureprior.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int failures = 0;

#define CHECK(cond, msg)                       \
  do {                                         \
    if (!(cond)) {                             \
      std::fprintf(stderr, "FAIL: %s\n", msg); \
      failures++;                              \
    } else {                                   \
      std::printf("ok: %s\n", msg);            \
    }                                          \
  } while (0)

static void TestDeviceProfileDetection() {
  const std::string gen2_fw = "p=cam_model_a/s=r/v=2.1.0/i=1234/h=abcdef";
  const std::string dart2_fw = "p=cam_model_b/s=r/v=1.5.0/i=42/h=123456";
  const std::string legacy_fw = "107428-05";

  /* Exclusive 2nd-gen Basler families */
  CHECK(gst_pylon_detect_device_profile("ace 2", gen2_fw) ==
            GstPylonDeviceProfile::kBaslerGen2,
        "ace 2 with structured fw -> kBaslerGen2");
  CHECK(gst_pylon_detect_device_profile("ace 2", "") ==
            GstPylonDeviceProfile::kBaslerGen2,
        "ace 2 without fw string -> kBaslerGen2");
  CHECK(gst_pylon_detect_device_profile("boost", gen2_fw) ==
            GstPylonDeviceProfile::kBaslerGen2,
        "boost -> kBaslerGen2");
  CHECK(gst_pylon_detect_device_profile("racer 2", gen2_fw) ==
            GstPylonDeviceProfile::kBaslerGen2,
        "racer 2 -> kBaslerGen2");
  CHECK(gst_pylon_detect_device_profile("racer 2S", gen2_fw) ==
            GstPylonDeviceProfile::kBaslerGen2,
        "racer 2S -> kBaslerGen2");

  /* Shared family name 'dart': requires structured DeviceFirmwareVersion */
  CHECK(gst_pylon_detect_device_profile("dart", dart2_fw) ==
            GstPylonDeviceProfile::kBaslerGen2,
        "dart with structured fw (dart 2) -> kBaslerGen2");
  CHECK(gst_pylon_detect_device_profile("dart", legacy_fw) ==
            GstPylonDeviceProfile::kGeneric,
        "dart with legacy fw (dart 1) -> kGeneric");
  CHECK(gst_pylon_detect_device_profile("dart", "") ==
            GstPylonDeviceProfile::kGeneric,
        "dart with empty fw -> kGeneric");
  CHECK(gst_pylon_detect_device_profile("dart", "p=cam_model_b/s=r") ==
            GstPylonDeviceProfile::kGeneric,
        "dart with incomplete structured fw -> kGeneric");

  /* First-generation / generic cameras and StreamGrabber */
  CHECK(gst_pylon_detect_device_profile("ace", legacy_fw) ==
            GstPylonDeviceProfile::kGeneric,
        "ace 1 -> kGeneric");
  CHECK(gst_pylon_detect_device_profile("Basler CamEmu", "") ==
            GstPylonDeviceProfile::kGeneric,
        "CamEmu -> kGeneric");
  CHECK(gst_pylon_detect_device_profile("", "") ==
            GstPylonDeviceProfile::kGeneric,
        "empty (StreamGrabber) -> kGeneric");
}

static void TestTlLockBehaviorPrior() {
  /* Generic cameras must always fall back to live TLParamsLocked probing */
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kGeneric,
                                         "ExposureTime") ==
            GstPylonTlLockBehavior::kUnknown,
        "generic ExposureTime -> kUnknown");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kGeneric,
                                         "BinningHorizontal") ==
            GstPylonTlLockBehavior::kUnknown,
        "generic BinningHorizontal -> kUnknown");

  /* 2nd-gen always-locked features */
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "BinningHorizontal") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 BinningHorizontal -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "ImageCompressionMode") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 ImageCompressionMode -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "BslImageCompressionRatio") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 BslImageCompressionRatio -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "ChunkModeActive") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 ChunkModeActive -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "BslFlatFieldCorrectionMode") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 BslFlatFieldCorrectionMode -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "BslScalingFactor") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 BslScalingFactor -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "UserSetDefault") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 UserSetDefault -> kLockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "UserSetSelector") ==
            GstPylonTlLockBehavior::kLockedPlaying,
        "gen2 UserSetSelector -> kLockedPlaying");

  /* 2nd-gen conditionally-locked features must probe live */
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "TriggerMode") ==
            GstPylonTlLockBehavior::kUnknown,
        "gen2 TriggerMode -> kUnknown (shutter-mode dependent)");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "BslDemosaicingMethod") ==
            GstPylonTlLockBehavior::kUnknown,
        "gen2 BslDemosaicingMethod -> kUnknown (runtime dependent)");

  /* 2nd-gen unlocked-during-playing features */
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "ExposureTime") ==
            GstPylonTlLockBehavior::kUnlockedPlaying,
        "gen2 ExposureTime -> kUnlockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "Gain") ==
            GstPylonTlLockBehavior::kUnlockedPlaying,
        "gen2 Gain -> kUnlockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "Gamma") ==
            GstPylonTlLockBehavior::kUnlockedPlaying,
        "gen2 Gamma -> kUnlockedPlaying");
  CHECK(gst_pylon_prior_tl_lock_behavior(GstPylonDeviceProfile::kBaslerGen2,
                                         "BalanceRatio") ==
            GstPylonTlLockBehavior::kUnlockedPlaying,
        "gen2 BalanceRatio -> kUnlockedPlaying");
}

static void TestLimitRules() {
  /* 2nd-gen float rules */
  const auto exp_rule = gst_pylon_prior_double_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "ExposureTime");
  CHECK(exp_rule.strategy == GstPylonLimitStrategy::kFixedRange &&
            exp_rule.fixed_min == 1.0 && exp_rule.fixed_max == 1.0e7,
        "gen2 ExposureTime -> kFixedRange [1.0, 1e7]");

  const auto gain_rule = gst_pylon_prior_double_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "Gain");
  CHECK(gain_rule.strategy == GstPylonLimitStrategy::kQueryCurrent,
        "gen2 Gain -> kQueryCurrent");

  const auto auto_gain_upper = gst_pylon_prior_double_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "AutoGainUpperLimit");
  CHECK(
      auto_gain_upper.strategy ==
              GstPylonLimitStrategy::kDerivedFromSiblingMin &&
          std::strcmp(auto_gain_upper.sibling_node, "AutoGainLowerLimit") == 0,
      "gen2 AutoGainUpperLimit -> "
      "kDerivedFromSiblingMin(AutoGainLowerLimit)");

  const auto periodic_delay = gst_pylon_prior_double_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "BslPeriodicSignalDelay");
  CHECK(periodic_delay.strategy ==
                GstPylonLimitStrategy::kDerivedFromSiblingMax &&
            std::strcmp(periodic_delay.sibling_node,
                        "BslPeriodicSignalPeriod") == 0,
        "gen2 BslPeriodicSignalDelay -> "
        "kDerivedFromSiblingMax(BslPeriodicSignalPeriod)");

  /* 2nd-gen integer rules */
  const auto offset_x = gst_pylon_prior_int_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "OffsetX");
  CHECK(offset_x.strategy == GstPylonLimitStrategy::kOffsetXFromSensor,
        "gen2 OffsetX -> kOffsetXFromSensor");

  const auto bin_v = gst_pylon_prior_int_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "BinningVertical");
  CHECK(bin_v.strategy == GstPylonLimitStrategy::kDerivedFromSiblingBoth &&
            std::strcmp(bin_v.sibling_node, "BinningHorizontal") == 0,
        "gen2 BinningVertical -> kDerivedFromSiblingBoth(BinningHorizontal)");

  const auto dshift = gst_pylon_prior_int_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "DigitalShift");
  CHECK(dshift.strategy == GstPylonLimitStrategy::kQueryCurrent,
        "gen2 DigitalShift -> kQueryCurrent");

  /* Unknown feature falls back to dynamic search */
  const auto unknown_gen2 = gst_pylon_prior_int_limit_rule(
      GstPylonDeviceProfile::kBaslerGen2, "SomeFutureUnknownFeature");
  CHECK(unknown_gen2.strategy == GstPylonLimitStrategy::kDynamicSearch,
        "gen2 unknown feature -> kDynamicSearch");

  /* Generic camera keeps legacy workarounds and leaves gen2-only rules dynamic
   */
  const auto generic_gain = gst_pylon_prior_double_limit_rule(
      GstPylonDeviceProfile::kGeneric, "Gain");
  CHECK(generic_gain.strategy == GstPylonLimitStrategy::kDynamicSearch,
        "generic Gain -> kDynamicSearch");

  const auto generic_offset_x = gst_pylon_prior_int_limit_rule(
      GstPylonDeviceProfile::kGeneric, "OffsetX");
  CHECK(generic_offset_x.strategy == GstPylonLimitStrategy::kOffsetXFromSensor,
        "generic OffsetX -> kOffsetXFromSensor");
}

static void TestCamEmuNodemapDetection() {
  Pylon::PylonAutoInitTerm auto_init;
  Pylon::CTlFactory &factory = Pylon::CTlFactory::GetInstance();
  Pylon::DeviceInfoList_t devices;
  Pylon::DeviceInfoList_t filter;
  Pylon::CDeviceInfo camemu_filter;
  camemu_filter.SetDeviceClass("BaslerCamEmu");
  filter.push_back(camemu_filter);
  factory.EnumerateDevices(devices, filter);
  if (devices.empty()) {
    std::printf("skip: no CamEmu device available\n");
    return;
  }

  Pylon::CBaslerUniversalInstantCamera camera(
      factory.CreateDevice(devices.at(0)));
  camera.Open();

  CHECK(gst_pylon_detect_device_profile(camera.GetNodeMap()) ==
            GstPylonDeviceProfile::kGeneric,
        "CamEmu camera nodemap -> kGeneric");
  CHECK(gst_pylon_detect_device_profile(camera.GetStreamGrabberNodeMap()) ==
            GstPylonDeviceProfile::kGeneric,
        "CamEmu stream grabber nodemap -> kGeneric");

  camera.Close();
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);

  TestDeviceProfileDetection();
  TestTlLockBehaviorPrior();
  TestLimitRules();
  TestCamEmuNodemapDetection();

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All feature prior tests passed\n");
  return EXIT_SUCCESS;
}
