"""Unit tests for SDK input and Linux Debian archive selection."""

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from prepare_control_files import (
    download_file,
    main,
    parse_version,
    select_artifacts,
)


SYNTHETIC_PREFIX = "sdk-store/releases"


def artifact_bundle(
    version="26.09.2",
    build="20261001.4",
    platform="linux_aarch64",
    deb_arch="aarch64",
    prefix=SYNTHETIC_PREFIX,
):
    directory = f"{prefix}/{version}/{build}"
    items = [f"{directory}/report_{platform}.json"]
    if deb_arch:
        items.append(f"{directory}/pylon-{version}_linux-{deb_arch}_debs.tar.gz")
        items.append(f"{directory}/pylon-{version}_linux-{deb_arch}_setup.tar.gz")
    return items


class PrepareControlFilesTestSuite(unittest.TestCase):
    def test_sdk_version_override_two_and_three_segments(self):
        paths = artifact_bundle("26.09.1") + artifact_bundle("26.09.2")
        platforms = {"linux_aarch64": {"deb_arch": "aarch64"}}

        sel_month = select_artifacts(
            paths, platforms, SYNTHETIC_PREFIX, sdk_version="26.9"
        )
        self.assertEqual(sel_month["linux_aarch64"]["version"], "26.09.2")
        self.assertTrue(
            sel_month["linux_aarch64"]["deb_archive"].endswith(
                "pylon-26.09.2_linux-aarch64_debs.tar.gz"
            )
        )

        sel_patch = select_artifacts(
            paths, platforms, SYNTHETIC_PREFIX, sdk_version="26.09.1"
        )
        self.assertEqual(sel_patch["linux_aarch64"]["version"], "26.09.1")

    def test_invalid_versions_rejected(self):
        for value in ("v1.1.0", "26x9", "26.0", "26.13", "", "26.9.*", "26.9rc1"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_version(value, allow_two_segment=True)

    def test_default_selects_latest_version_within_configured_prefix(self):
        expected = artifact_bundle("26.10.1")
        paths = (
            artifact_bundle("26.08.1")
            + expected
            + artifact_bundle("26.11.0", prefix="other-store/staging")
        )
        sel = select_artifacts(
            paths, {"linux_aarch64": {"deb_arch": "aarch64"}}, SYNTHETIC_PREFIX
        )
        self.assertEqual(sel["linux_aarch64"]["report"], expected[0])
        self.assertEqual(sel["linux_aarch64"]["deb_archive"], expected[1])

    def test_numeric_ordering_for_patch_and_build(self):
        expected = artifact_bundle("26.09.10", "20261001.10")
        paths = (
            artifact_bundle("26.09.2", "20261231.99")
            + artifact_bundle("26.09.10", "20261001.9")
            + expected
        )
        sel = select_artifacts(
            paths,
            {"linux_aarch64": {"deb_arch": "aarch64"}},
            SYNTHETIC_PREFIX,
            sdk_version="26.9",
        )
        self.assertEqual(sel["linux_aarch64"]["report"], expected[0])

    def test_mixed_linux_deb_and_desktop_sdk_platforms(self):
        paths = (
            artifact_bundle("26.10.0", platform="linux_x86_64", deb_arch="x86_64")
            + artifact_bundle("26.10.0", platform="linux_aarch64", deb_arch="aarch64")
            + artifact_bundle("26.10.0", platform="windows_AMD64", deb_arch=None)
            + artifact_bundle("26.10.0", platform="macos_arm64", deb_arch=None)
        )
        platforms = {
            "linux_x86_64": {"deb_arch": "x86_64"},
            "linux_aarch64": {"deb_arch": "aarch64"},
            "windows": {},
            "macos": {},
        }
        sel = select_artifacts(paths, platforms, SYNTHETIC_PREFIX)
        self.assertIsNotNone(sel["linux_x86_64"]["deb_archive"])
        self.assertIsNotNone(sel["linux_aarch64"]["deb_archive"])
        self.assertIsNone(sel["windows"]["deb_archive"])
        self.assertIsNone(sel["macos"]["deb_archive"])

    def test_missing_deb_archive_fails_without_leaking_paths(self):
        paths = artifact_bundle("26.10.0", platform="linux_x86_64", deb_arch=None)
        with self.assertRaisesRegex(ValueError, "^Missing Debian package for x86_64$"):
            select_artifacts(
                paths, {"linux_x86_64": {"deb_arch": "x86_64"}}, SYNTHETIC_PREFIX
            )

    def test_standalone_deb_package_selected_and_supplementary_ignored(self):
        base = f"{SYNTHETIC_PREFIX}/26.10.1/20261027.7"
        paths = [
            f"{base}/report_linux_x86_64.json",
            f"{base}/report_linux_aarch64.json",
            f"{base}/pylon_linux-x86_64_26101.deb",
            f"{base}/pylon_linux-x86_64_26101.tar.gz",
            f"{base}/pylon_linux-aarch64_26101.deb",
            f"{base}/pylon_linux-aarch64_26101.tar.gz",
            f"{base}/pylon-supplementary-package-for-mpeg-4_2.0.0_amd64.deb",
            f"{base}/pylon-supplementary-package-for-mpeg-4_2.0.0_arm64.deb",
        ]
        sel = select_artifacts(
            paths,
            {
                "linux_x86_64": {"deb_arch": "x86_64"},
                "linux_aarch64": {"deb_arch": "aarch64"},
            },
            SYNTHETIC_PREFIX,
        )
        self.assertEqual(
            sel["linux_x86_64"]["deb_archive"],
            f"{base}/pylon_linux-x86_64_26101.deb",
        )
        self.assertEqual(
            sel["linux_aarch64"]["deb_archive"],
            f"{base}/pylon_linux-aarch64_26101.deb",
        )

    def test_missing_or_empty_download_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "control.json"
            destination.write_text("stale", encoding="utf-8")
            with patch(
                "prepare_control_files._open_with_retries",
                return_value=io.BytesIO(b""),
            ):
                with self.assertRaisesRegex(ValueError, "nonempty file"):
                    download_file("https://example.invalid/artifactory", "tok", "source", destination)

    def test_discover_candidate_paths_walks_only_latest_folders(self):
        from prepare_control_files import discover_candidate_paths

        tree = {
            SYNTHETIC_PREFIX: [("25.06.0", True), ("26.10.1", True), ("README.txt", False)],
            f"{SYNTHETIC_PREFIX}/26.10.1": [("20261101.1", True), ("20261102.1", True)],
            f"{SYNTHETIC_PREFIX}/26.10.1/20261102.1": [
                ("report_linux_x86_64.json", False),
                ("pylon_linux-x86_64_26101.deb", False),
                ("report_windows_AMD64.json", False),
            ],
        }
        visited = []

        def fake_list(path):
            visited.append(path)
            return tree[path]

        paths = discover_candidate_paths(
            fake_list,
            {"linux_x86_64": {"deb_arch": "x86_64"}, "windows": {}},
            search_prefix=SYNTHETIC_PREFIX,
        )
        # Never descended into older version 25.06.0 or older build 20261101.1
        self.assertEqual(
            visited,
            [
                SYNTHETIC_PREFIX,
                f"{SYNTHETIC_PREFIX}/26.10.1",
                f"{SYNTHETIC_PREFIX}/26.10.1/20261102.1",
            ],
        )
        self.assertIn(f"{SYNTHETIC_PREFIX}/26.10.1/20261102.1/pylon_linux-x86_64_26101.deb", paths)

    def test_main_ignores_semver_git_tag_and_downloads_expected_files(self):
        tree = {
            SYNTHETIC_PREFIX: [("26.10.0", True)],
            f"{SYNTHETIC_PREFIX}/26.10.0": [("20261101.2", True)],
            f"{SYNTHETIC_PREFIX}/26.10.0/20261101.2": [
                ("report_linux_x86_64.json", False),
                ("pylon-26.10.0_linux-x86_64_debs.tar.gz", False),
                ("report_windows_AMD64.json", False),
            ],
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            cwd = os.getcwd()
            os.chdir(tmpdir)
            try:
                cfg_dir = Path(".github/workflows")
                cfg_dir.mkdir(parents=True)
                (cfg_dir / "build_config.json").write_text(
                    json.dumps(
                        {
                            "linux_x86_64": {"deb_arch": "x86_64"},
                            "windows": {},
                        }
                    ),
                    encoding="utf-8",
                )

                def fake_download(api_base, token, source, destination):
                    destination = Path(destination)
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(b"ok")

                gh_out = Path(tmpdir) / "gh_output.txt"
                with patch.dict(
                    os.environ,
                    {
                        "TAG_NAME": "v1.1.0",
                        "SDK_URL": "https://sdk.example.invalid",
                        "SDK_TOKEN": "synthetic-secret-token",
                        "SDK_SEARCH_PREFIX": SYNTHETIC_PREFIX,
                        "PYLON_SDK_VERSION": "",
                        "GITHUB_OUTPUT": str(gh_out),
                    },
                    clear=False,
                ):
                    with patch(
                        "prepare_control_files.list_storage_children",
                        side_effect=lambda api_base, token, path: tree[path],
                    ):
                        with patch(
                            "prepare_control_files.download_file",
                            side_effect=fake_download,
                        ):
                            with redirect_stdout(io.StringIO()) as output:
                                main()

                self.assertNotIn(SYNTHETIC_PREFIX, output.getvalue())
                self.assertIn("pylon 26.10.0", output.getvalue())
                self.assertTrue(Path("linux_x86_64/pylon_debs.tar.gz").is_file())
                self.assertTrue(Path("windows/control_windows.json").is_file())
                self.assertIn("pylon_version=26.10.0", gh_out.read_text(encoding="utf-8"))
            finally:
                os.chdir(cwd)

    def test_network_failure_does_not_leak_url_token_or_prefix(self):
        secret_host = "internal-secret-host.invalid"
        secret_token = "super-secret-token-value"
        secret_path = f"{SYNTHETIC_PREFIX}/secret-repo"
        script = Path(__file__).resolve().parent / "prepare_control_files.py"
        with tempfile.TemporaryDirectory() as tmpdir:
            cfg_dir = Path(tmpdir) / ".github" / "workflows"
            cfg_dir.mkdir(parents=True)
            (cfg_dir / "build_config.json").write_text(
                json.dumps({"linux_x86_64": {"deb_arch": "x86_64"}}),
                encoding="utf-8",
            )
            env = os.environ.copy()
            env["SDK_URL"] = f"https://{secret_host}"
            env["SDK_TOKEN"] = secret_token
            env["SDK_SEARCH_PREFIX"] = secret_path
            proc = subprocess.run(
                ["python3", str(script)],
                cwd=tmpdir,
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(proc.returncode, 0)
            for secret in (secret_host, secret_token, SYNTHETIC_PREFIX):
                self.assertNotIn(secret, proc.stdout)
                self.assertNotIn(secret, proc.stderr)
            self.assertIn("SDK server request failed", proc.stderr)


if __name__ == "__main__":
    unittest.main()
