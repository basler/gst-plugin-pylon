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

int main(int argc, char** argv) {
  gst_init(&argc, &argv);

  TestLoadAndValidSelector();

  if (failures > 0) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All synthetic nodemap tests passed\n");
  return EXIT_SUCCESS;
}
