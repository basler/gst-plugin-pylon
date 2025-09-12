#!/bin/bash

# Test script for PYLONSRC_SKIP_TRANSPORT_LAYERS environment variable
# This script tests the transport layer filtering functionality

echo "Testing PYLONSRC_SKIP_TRANSPORT_LAYERS environment variable"
echo "========================================================="
echo

# Set up the environment
export GST_PLUGIN_PATH=$PWD/build/ext/pylon
export LD_LIBRARY_PATH=$PWD/build/gst-libs/gst/pylon:$LD_LIBRARY_PATH

# Test 1: Default enumeration (no environment variable)
echo "Test 1: Default enumeration (no env var set)"
unset PYLONSRC_SKIP_TRANSPORT_LAYERS
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -E "(devices match|Found [0-9]+ device|Available transport|enumerating all)" || echo "No devices found or timeout"
echo

# Test 2: Skip GigE and USB (keep CXP and CameraLink)
echo "Test 2: Skip GigE and USB transport layers"
export PYLONSRC_SKIP_TRANSPORT_LAYERS=gige,usb
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -E "(devices match|Found [0-9]+ device|Skipping|Enumerating)" || echo "No devices found or timeout"
echo

# Test 3: Skip only GigE
echo "Test 3: Skip GigE transport layer only"
export PYLONSRC_SKIP_TRANSPORT_LAYERS=gige
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -E "(devices match|Found [0-9]+ device|Skipping.*gige|Enumerating)" || echo "No devices found or timeout"
echo

# Test 4: Skip only USB
echo "Test 4: Skip USB transport layer only"
export PYLONSRC_SKIP_TRANSPORT_LAYERS=usb
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -E "(devices match|Found [0-9]+ device|Skipping.*usb|Enumerating)" || echo "No devices found or timeout"
echo

# Test 5: Invalid transport layer (should show warning)
echo "Test 5: Invalid transport layer (should show warning)"
export PYLONSRC_SKIP_TRANSPORT_LAYERS=invalid
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -E "(Unknown transport layer|Warning)" || echo "No warning shown"
echo

# Test 6: Skip CXP transport layer
echo "Test 6: Skip CXP transport layer"
export PYLONSRC_SKIP_TRANSPORT_LAYERS=cxp
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -E "(devices match|Found [0-9]+ device|Skipping.*cxp)" || echo "No devices found or timeout"
echo

# Test 7: Debug output showing available transport layers
echo "Test 7: Show available transport layers in debug output"
unset PYLONSRC_SKIP_TRANSPORT_LAYERS
timeout 5 gst-launch-1.0 --gst-debug=pylonsrc:5 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep "Available transport layers" || echo "Debug message not found"
echo

echo "Test completed!"
echo
echo "Note: This test requires actual cameras to be connected to see meaningful results."
echo "The test verifies that the environment variable is parsed correctly and doesn't cause crashes."
echo "Use --gst-debug=pylonsrc:5 to see detailed debug information about transport layer processing."
