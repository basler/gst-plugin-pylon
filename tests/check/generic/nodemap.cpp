/* Copyright (C) 2026 Basler AG
 *
 * Synthetic GenApi XML nodemap unit tests for gst-libs/gst/pylon.
 * Runs without a physical camera or emulator.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <GenApi/GenApi.h>
#include <gst/pylon/gstpylonintrospection.h>
#include <gst/pylon/gstpylonobject.h>

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

static const char* kSyntheticXml = R"XML(<?xml version="1.0" encoding="utf-8"?>
<RegisterDescription ModelName="SyntheticCam" VendorName="Basler"
  StandardNameSpace="None" SchemaMajorVersion="1" SchemaMinorVersion="1"
  SchemaSubMinorVersion="0" MajorVersion="1" MinorVersion="0" SubMinorVersion="0"
  ToolTip="Synthetic test nodemap" ProductGuid="00000000-0000-0000-0000-000000000000"
  VersionGuid="00000000-0000-0000-0000-000000000001"
  xmlns="http://www.genicam.org/GenApi/Version_1_1">

  <Category Name="Root">
    <pFeature>TestSelector</pFeature>
    <pFeature>TestValue</pFeature>
    <pFeature>MultiChildParent</pFeature>
    <pFeature>WideIntSelector</pFeature>
    <pFeature>WideSelectedVal</pFeature>
  </Category>

  <Enumeration Name="TestSelector">
    <EnumEntry Name="EntryA">
      <Value>0</Value>
    </EnumEntry>
    <EnumEntry Name="EntryB">
      <Value>1</Value>
    </EnumEntry>
    <pValue>TestSelectorReg</pValue>
    <pSelected>TestValue</pSelected>
  </Enumeration>

  <Integer Name="TestSelectorReg">
    <Value>0</Value>
    <Min>0</Min>
    <Max>1</Max>
  </Integer>

  <Integer Name="TestValue">
    <Value>42</Value>
    <Min>0</Min>
    <Max>100</Max>
  </Integer>

  <Integer Name="LockFlag">
    <Value>1</Value>
    <Min>0</Min>
    <Max>1</Max>
  </Integer>

  <Integer Name="UnlockedChild">
    <Value>10</Value>
    <Min>0</Min>
    <Max>100</Max>
  </Integer>

  <Integer Name="LockedChild">
    <pIsLocked>LockFlag</pIsLocked>
    <Value>20</Value>
    <Min>0</Min>
    <Max>100</Max>
  </Integer>

  <Integer Name="MultiChildParent">
    <pValue>UnlockedChild</pValue>
    <Min>0</Min>
    <pMax>LockedChild</pMax>
  </Integer>

  <Integer Name="WideIntSelector">
    <Value>1</Value>
    <Min>1</Min>
    <Max>999999999999</Max>
    <Inc>1</Inc>
    <pSelected>WideSelectedVal</pSelected>
  </Integer>

  <Integer Name="WideSelectedVal">
    <Value>5</Value>
    <Min>0</Min>
    <Max>10</Max>
  </Integer>
</RegisterDescription>
)XML";

static void TestLoadAndValidSelector() {
  GenApi::CNodeMapRef nodemap;
  nodemap._LoadXMLFromString(kSyntheticXml);

  GenApi::INode* sel_node = nodemap._GetNode("TestSelector");
  CHECK(sel_node != nullptr, "synthetic XML loads TestSelector");

  gint64 val = 1;
  gst_pylon_object_set_pylon_selector(*nodemap._Ptr, "TestSelector", val);

  Pylon::CEnumParameter sel(*nodemap._Ptr, "TestSelector");
  CHECK(sel.GetIntValue() == 1,
        "gst_pylon_object_set_pylon_selector selects EntryB");
}

static void TestUnknownSelectorThrowsCleanly() {
  GenApi::CNodeMapRef nodemap;
  nodemap._LoadXMLFromString(kSyntheticXml);

  bool caught = false;
  try {
    gint64 val = 0;
    gst_pylon_object_set_pylon_selector(*nodemap._Ptr, "NonExistentSelector",
                                        val);
  } catch (const GenICam::GenericException&) {
    caught = true;
  }
  CHECK(caught,
        "gst_pylon_object_set_pylon_selector throws GenericException for "
        "unknown selector without null deref");
}

static void TestSecondChildWritableScan() {
  GenApi::CNodeMapRef nodemap;
  nodemap._LoadXMLFromString(kSyntheticXml);

  GenApi::INode* parent = nodemap._GetNode("MultiChildParent");
  CHECK(parent != nullptr, "synthetic XML loads MultiChildParent");
  CHECK(gst_pylon_can_feature_later_be_writable(parent) == TRUE,
        "can_feature_later_be_writable scans beyond first child");
}

static void TestWideIntegerSelectorRegistration() {
  GenApi::CNodeMapRef nodemap;
  nodemap._LoadXMLFromString(kSyntheticXml);

  /* Registering a GstPylonObject schema against a nodemap with an int64-range
   * integer selector must not overflow 32-bit int or throw std::out_of_range.
   */
  GstPylonCache cache("SyntheticWideIntSelCam");
  GstPylonObjectSchema schema;
  schema.device_full_name = "SyntheticWideIntSelCam";
  schema.schema_cache_key = "SyntheticWideIntSelCam";
  schema.feature_cache = &cache;
  schema.nodemap = nodemap._Ptr;

  GType t = gst_pylon_object_register_schema(schema);
  CHECK(t != G_TYPE_INVALID,
        "gst_pylon_object_register_schema succeeds with 64-bit range integer "
        "selector");
}

int main(int argc, char** argv) {
  gst_init(&argc, &argv);

  try {
    TestLoadAndValidSelector();
    TestUnknownSelectorThrowsCleanly();
    TestSecondChildWritableScan();
    TestWideIntegerSelectorRegistration();
  } catch (const GenICam::GenericException& e) {
    std::fprintf(stderr, "Uncaught GenICam exception: %s\n",
                 e.GetDescription());
    return EXIT_FAILURE;
  }

  if (failures > 0) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All synthetic nodemap tests passed\n");
  return EXIT_SUCCESS;
}
