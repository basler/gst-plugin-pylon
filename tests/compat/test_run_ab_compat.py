"""Regression tests for the A/B snapshot comparison."""

import unittest

from run_ab_compat import compare_snapshots


def snapshot(monitor_started):
    """Small valid snapshot with no API differences or capture failures."""
    return {
        "plugin_features": {},
        "element_properties": {},
        "element_signals": [],
        "pad_templates": {},
        "child_proxy_null_state": {
            "cam_resolved": True,
            "stream_resolved": True,
            "cam_properties": {},
            "stream_properties": {},
        },
        "caps_negotiation": {},
        "metadata": {
            "all_have_meta": True,
            "pts_monotonic": True,
            "image_numbers_increasing": True,
        },
        "controls": {
            "repeated_restarts_ok": True,
            "capture_error_modes": {},
            "enable_correction_true_ok": True,
        },
        "errors": {
            name: {"failed": True}
            for name in ("ambiguous_device", "missing_serial", "missing_pfs")
        },
        "device_monitor": {"started": monitor_started, "device_count": 0},
    }


class DeviceMonitorCompatibilityTest(unittest.TestCase):
    def test_unchanged_monitor_result_is_compatible(self):
        for started in (False, True):
            with self.subTest(started=started):
                failures, _ = compare_snapshots(snapshot(started), snapshot(started), {})
                self.assertEqual(failures, [])

    def test_changed_monitor_result_is_incompatible(self):
        for started in (False, True):
            with self.subTest(baseline_started=started):
                failures, _ = compare_snapshots(snapshot(started), snapshot(not started), {})
                self.assertEqual(len(failures), 1)
                self.assertIn("GstDeviceMonitor", failures[0])


if __name__ == "__main__":
    unittest.main()
