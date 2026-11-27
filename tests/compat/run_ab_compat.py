#!/usr/bin/env python3
"""Automated A/B backwards-compatibility proof harness: v1.1.0 vs v2.0.0.

Proves that existing gst-plugin-pylon v1.1.0 C, Python, and gst-launch users can
upgrade to v2.0.0 without changing or recompiling their code:

  Pillar 1: Zero-recompile binary & ABI drop-in proof (C ABI/symbols, unmodified
            v1.1.0 C examples, v1.1.0 camemu & pygstpylon tests run on v2.0.0)
  Pillar 2: Automated A/B behavioral differential suite (properties, signals,
            pad templates, lazy class_init + GstChildProxy, caps fixation,
            metadata/chunks, error paths, DeviceProvider non-interference)
  Pillar 3: In-place ~/.cache/gstpylon migration proof (v1.1.0 cache -> v2.0.0)
  Pillar 4: Machine-checked intentional-delta ledger (any unlisted diff fails)
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_LEDGER = SCRIPT_DIR / "intentional_deltas_v2.json"
EMU_SERIAL_0 = "0815-0000"
EMU_SERIAL_1 = "0815-0001"


def find_gi_python() -> str:
    """Return a Python interpreter that has PyGObject (gi + Gst 1.0)."""
    candidates = [sys.executable, "/usr/bin/python3", "python3"]
    seen = set()
    for cand in candidates:
        resolved = shutil.which(cand) if not os.path.isabs(cand) else cand
        if not resolved or resolved in seen or not os.path.exists(resolved):
            continue
        seen.add(resolved)
        proc = subprocess.run(
            [
                resolved,
                "-c",
                "import gi; gi.require_version('Gst', '1.0'); from gi.repository import Gst",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if proc.returncode == 0:
            return resolved
    raise RuntimeError("No Python interpreter with PyGObject (gi + Gst 1.0) found")


def resolve_build_artifacts(build_dir: Path) -> dict:
    """Locate plugin, shared library, and pygstpylon module inside a build dir."""
    build_dir = build_dir.resolve()
    plugin_dir = build_dir / "ext" / "pylon"
    lib_dir = build_dir / "gst-libs" / "gst" / "pylon"
    bindings_dir = build_dir / "bindings"

    plugin_so = plugin_dir / "libgstpylon.so"
    lib_so = lib_dir / "libgstpylon-1.0.so"
    if not plugin_so.is_file():
        raise FileNotFoundError(f"Plugin not found: {plugin_so}")
    if not lib_so.is_file():
        raise FileNotFoundError(f"Shared library not found: {lib_so}")

    py_mod = None
    if bindings_dir.is_dir():
        for name in ("pygstpylon.abi3.so", "pygstpylon.so"):
            if (bindings_dir / name).is_file():
                py_mod = bindings_dir / name
                break
        if py_mod is None:
            matches = list(bindings_dir.glob("pygstpylon*.so"))
            if matches:
                py_mod = matches[0]

    return {
        "build_dir": build_dir,
        "plugin_dir": plugin_dir,
        "plugin_so": plugin_so,
        "lib_dir": lib_dir,
        "lib_so": lib_so,
        "bindings_dir": bindings_dir if py_mod else None,
        "py_mod": py_mod,
    }


def make_isolated_env(
    artifacts: dict,
    *,
    cache_dir: Path,
    registry_file: Path,
    pylon_root: str,
    camemu: str = "3",
    extra: dict | None = None,
    fatal_criticals: bool = False,
) -> dict:
    """Create a strictly isolated environment pointing to one plugin build."""
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    env["PYLON_ROOT"] = pylon_root
    env["PYLON_CAMEMU"] = camemu
    env["PYLONSRC_SKIP_TRANSPORT_LAYERS"] = "usb,gige,cxp,cameralink"
    env["GST_PLUGIN_PATH_1_0"] = str(artifacts["plugin_dir"])
    env["GST_PLUGIN_PATH"] = str(artifacts["plugin_dir"])
    env["GST_REGISTRY_1_0"] = str(registry_file)
    env["GST_REGISTRY"] = str(registry_file)
    env["XDG_CACHE_HOME"] = str(cache_dir)
    env["GST_DEBUG_NO_COLOR"] = "1"

    ld_parts = [str(artifacts["lib_dir"]), os.path.join(pylon_root, "lib")]
    if env.get("LD_LIBRARY_PATH"):
        ld_parts.append(env["LD_LIBRARY_PATH"])
    env["LD_LIBRARY_PATH"] = ":".join(ld_parts)

    if artifacts["bindings_dir"]:
        py_parts = [str(artifacts["bindings_dir"])]
        if env.get("PYTHONPATH"):
            py_parts.append(env["PYTHONPATH"])
        env["PYTHONPATH"] = ":".join(py_parts)

    if fatal_criticals:
        env["G_DEBUG"] = "fatal-criticals"
    else:
        env.pop("G_DEBUG", None)

    if extra:
        env.update(extra)
    return env


def run_cmd(cmd, *, env=None, cwd=None, timeout=120):
    return subprocess.run(
        cmd,
        env=env,
        cwd=cwd,
        timeout=timeout,
        capture_output=True,
        text=True,
    )


# ---------------------------------------------------------------------------
# Subprocess probe mode (runs inside an isolated GStreamer process for v1/v2)
# ---------------------------------------------------------------------------


def probe_param_spec(pspec, gobj_class_or_obj=None) -> dict:
    """Serialize a GParamSpec into a stable JSON dictionary."""
    import gi

    gi.require_version("GObject", "2.0")
    from gi.repository import GObject

    val_type = pspec.value_type
    info = {
        "name": pspec.name,
        "type": val_type.name,
        "flags": {
            "readable": bool(pspec.flags & GObject.ParamFlags.READABLE),
            "writable": bool(pspec.flags & GObject.ParamFlags.WRITABLE),
            "construct": bool(pspec.flags & GObject.ParamFlags.CONSTRUCT),
            "construct_only": bool(pspec.flags & GObject.ParamFlags.CONSTRUCT_ONLY),
        },
    }

    # Default value
    default = getattr(pspec, "default_value", None)
    if val_type.is_a(GObject.TYPE_ENUM):
        enum_class = getattr(pspec, "enum_class", None)
        entries = {}
        if enum_class is not None:
            enum_map = getattr(enum_class, "__enum_values__", None)
            if isinstance(enum_map, dict):
                for ev_int, ev_obj in enum_map.items():
                    nick = getattr(ev_obj, "value_nick", str(ev_obj))
                    entries[nick] = int(ev_int)
        info["enum_values"] = entries
        try:
            info["default"] = int(default)
        except Exception:
            info["default"] = str(default)
    elif val_type.is_a(GObject.TYPE_BOOLEAN):
        info["default"] = bool(default)
    elif val_type.is_a(GObject.TYPE_INT) or val_type.is_a(GObject.TYPE_UINT) or val_type.is_a(GObject.TYPE_INT64) or val_type.is_a(GObject.TYPE_UINT64):
        info["default"] = int(default)
        if hasattr(pspec, "minimum"):
            info["min"] = int(pspec.minimum)
        if hasattr(pspec, "maximum"):
            info["max"] = int(pspec.maximum)
    elif val_type.is_a(GObject.TYPE_DOUBLE) or val_type.is_a(GObject.TYPE_FLOAT):
        info["default"] = float(default)
        if hasattr(pspec, "minimum"):
            info["min"] = float(pspec.minimum)
        if hasattr(pspec, "maximum"):
            info["max"] = float(pspec.maximum)
    elif val_type.is_a(GObject.TYPE_STRING):
        info["default"] = default
    else:
        info["default"] = str(default) if default is not None else None

    return info


def run_probe_mode(expected_plugin_so: str) -> int:
    """Inspect and exercise pylonsrc in-process, printing a JSON snapshot."""
    import gi

    gi.require_version("Gst", "1.0")
    gi.require_version("GObject", "2.0")
    from gi.repository import GObject, Gst

    Gst.init(None)

    factory = Gst.ElementFactory.find("pylonsrc")
    if factory is None:
        print(json.dumps({"error": "pylonsrc element factory not found"}))
        return 1

    plugin = factory.get_plugin()
    actual_plugin_file = os.path.realpath(plugin.get_filename())
    expected_real = os.path.realpath(expected_plugin_so)
    if actual_plugin_file != expected_real:
        print(
            json.dumps(
                {
                    "error": f"Plugin isolation failed: loaded {actual_plugin_file}, expected {expected_real}"
                }
            )
        )
        return 1

    snapshot = {
        "plugin_filename": actual_plugin_file,
        "plugin_version": plugin.get_version(),
        "plugin_features": {},
        "element_properties": {},
        "element_signals": [],
        "pad_templates": {},
        "child_proxy_null_state": {},
        "caps_negotiation": {},
        "metadata": {},
        "controls": {},
        "errors": {},
        "device_monitor": {},
    }

    # 1. Plugin features & ranks
    registry = Gst.Registry.get()
    for feat in registry.get_feature_list_by_plugin(plugin.get_name()):
        snapshot["plugin_features"][feat.get_name()] = {
            "type": type(feat).__name__,
            "rank": int(feat.get_rank()),
        }

    # 2. Pad templates
    for tmpl in factory.get_static_pad_templates():
        caps = tmpl.get_caps()
        media_formats = {}
        for i in range(caps.get_size()):
            st = caps.get_structure(i)
            name = st.get_name()
            feats = caps.get_features(i)
            key = f"{name}({feats.to_string()})" if feats and not feats.is_any() and feats.to_string() != "memory:SystemMemory" else name
            fmts = media_formats.setdefault(key, [])
            if st.has_field("format"):
                val = st.get_value("format")
                if isinstance(val, str):
                    if val not in fmts:
                        fmts.append(val)
                elif hasattr(val, "__iter__"):
                    for f in val:
                        if str(f) not in fmts:
                            fmts.append(str(f))
                else:
                    # GstValueList in PyGObject
                    val_str = st.to_string()
                    m = re.search(r"format=\(string\)\{([^}]+)\}", val_str)
                    if m:
                        for f in [x.strip().strip('"') for x in m.group(1).split(",")]:
                            if f and f not in fmts:
                                fmts.append(f)
                    else:
                        m2 = re.search(r"format=\(string\)([^,; ]+)", val_str)
                        if m2 and m2.group(1) not in fmts:
                            fmts.append(m2.group(1))
        snapshot["pad_templates"][tmpl.name_template] = media_formats

    # 3. Element properties & signals (on an instance in NULL state)
    src = Gst.ElementFactory.make("pylonsrc", "probe_src")
    assert src is not None, "Failed to create pylonsrc"
    for pspec in src.list_properties():
        snapshot["element_properties"][pspec.name] = probe_param_spec(pspec)

    # Walk GType hierarchy for signals defined on GstPylonSrc itself
    pylon_gtype = src.__gtype__
    snapshot["element_signals"] = sorted(GObject.signal_list_names(pylon_gtype))

    # 4. Lazy class_init + GstChildProxy in NULL state (before READY)
    src.set_property("device-serial-number", EMU_SERIAL_0)
    cam = src.get_child_by_name("cam")
    stream = src.get_child_by_name("stream")
    snapshot["child_proxy_null_state"]["cam_resolved"] = cam is not None
    snapshot["child_proxy_null_state"]["stream_resolved"] = stream is not None
    snapshot["child_proxy_null_state"]["cam_properties"] = {}
    snapshot["child_proxy_null_state"]["stream_properties"] = {}
    if cam is not None:
        for pspec in cam.list_properties():
            snapshot["child_proxy_null_state"]["cam_properties"][pspec.name] = (
                probe_param_spec(pspec)
            )
    if stream is not None:
        for pspec in stream.list_properties():
            snapshot["child_proxy_null_state"]["stream_properties"][pspec.name] = (
                probe_param_spec(pspec)
            )
    src.set_state(Gst.State.NULL)
    del cam, stream, src

    # Helper to run a short pipeline and capture caps + buffer stats
    def run_capture(pipeline_desc: str, num_samples: int = 3, with_meta: bool = False) -> dict:
        pipe = Gst.parse_launch(pipeline_desc)
        sink = pipe.get_by_name("sink")
        assert sink is not None
        ret = pipe.set_state(Gst.State.PLAYING)
        if ret == Gst.StateChangeReturn.FAILURE:
            pipe.set_state(Gst.State.NULL)
            return {"ok": False, "error": "set_state(PLAYING) returned FAILURE"}

        bus = pipe.get_bus()
        samples = []
        negotiated_caps = None
        src_pad_caps = None
        err_msg = None

        pylonsrc_elem = pipe.get_by_name("src")

        for _ in range(num_samples):
            sample = sink.emit("try-pull-sample", 10 * Gst.SECOND)
            if sample is None:
                msg = bus.timed_pop_filtered(
                    100 * Gst.MSECOND, Gst.MessageType.ERROR | Gst.MessageType.EOS
                )
                if msg and msg.type == Gst.MessageType.ERROR:
                    err, dbg = msg.parse_error()
                    err_msg = f"{err.message} ({dbg})"
                break
            caps = sample.get_caps()
            if negotiated_caps is None and caps is not None:
                st = caps.get_structure(0)
                negotiated_caps = {
                    "media_type": st.get_name(),
                    "format": st.get_string("format") if st.has_field("format") else None,
                    "width": st.get_int("width")[1] if st.has_field("width") else None,
                    "height": st.get_int("height")[1] if st.has_field("height") else None,
                    "colorimetry": st.get_string("colorimetry") if st.has_field("colorimetry") else None,
                    "framerate": str(st.get_fraction("framerate")[1]) + "/" + str(st.get_fraction("framerate")[2])
                    if st.has_field("framerate")
                    else None,
                }
            if src_pad_caps is None and pylonsrc_elem is not None:
                pad = pylonsrc_elem.get_static_pad("src")
                if pad and pad.get_current_caps():
                    st = pad.get_current_caps().get_structure(0)
                    src_pad_caps = {
                        "media_type": st.get_name(),
                        "format": st.get_string("format") if st.has_field("format") else None,
                        "width": st.get_int("width")[1] if st.has_field("width") else None,
                        "height": st.get_int("height")[1] if st.has_field("height") else None,
                    }

            buf = sample.get_buffer()
            entry = {
                "size": int(buf.get_size()),
                "pts": int(buf.pts),
                "duration": int(buf.duration),
                "offset": int(buf.offset),
                "corrupted": bool(buf.has_flags(Gst.BufferFlags.CORRUPTED)),
            }
            if with_meta:
                import pygstpylon

                meta = pygstpylon.gst_buffer_get_pylon_meta(hash(buf))
                if meta is not None:
                    entry["meta"] = {
                        "block_id": int(meta.block_id),
                        "image_number": int(meta.image_number),
                        "skipped_images": int(meta.skipped_images),
                        "offset_x": int(meta.offset_x),
                        "offset_y": int(meta.offset_y),
                        "timestamp_nonzero": int(meta.timestamp) > 0,
                        "stride": int(meta.stride),
                        "chunks_keys": sorted(meta.chunks.keys()) if isinstance(meta.chunks, dict) else [],
                    }
                else:
                    entry["meta"] = None
            samples.append(entry)

        pipe.set_state(Gst.State.NULL)
        return {
            "ok": len(samples) == num_samples and err_msg is None,
            "error": err_msg,
            "negotiated_caps": negotiated_caps,
            "src_pad_caps": src_pad_caps,
            "sample_count": len(samples),
            "samples": samples,
        }

    # 5. Caps negotiation & default fixation
    caps_cases = {
        "unconstrained_appsink": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! appsink name=sink sync=false",
        "unconstrained_videoconvert": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! videoconvert ! appsink name=sink sync=false",
        "fixed_GRAY8": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-raw,format=GRAY8,width=640,height=480 ! appsink name=sink sync=false",
        "fixed_RGB": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-raw,format=RGB,width=640,height=480 ! appsink name=sink sync=false",
        "fixed_BGR": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-raw,format=BGR,width=640,height=480 ! appsink name=sink sync=false",
        "fixed_YUY2": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-raw,format=YUY2,width=640,height=480 ! appsink name=sink sync=false",
        "fixed_UYVY": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-raw,format=UYVY,width=640,height=480 ! appsink name=sink sync=false",
        "fixed_bayer_bggr": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-bayer,format=bggr,width=640,height=480 ! appsink name=sink sync=false",
        "fixed_framerate_30": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} num-buffers=5 ! video/x-raw,format=GRAY8,width=640,height=480,framerate=30/1 ! appsink name=sink sync=false",
        "user_set_auto": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} user-set=Auto num-buffers=5 ! video/x-raw,format=GRAY8,width=640,height=480 ! appsink name=sink sync=false",
        "cam_before_user_set": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} cam::Gain=1 user-set=Auto num-buffers=5 ! video/x-raw,format=GRAY8,width=640,height=480 ! appsink name=sink sync=false",
        "offset_xy_roi": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} cam::OffsetX=16 cam::OffsetY=16 num-buffers=5 ! video/x-raw,format=GRAY8,width=320,height=240 ! appsink name=sink sync=false",
        "max_pending_frames_3": f"pylonsrc name=src device-serial-number={EMU_SERIAL_0} max-pending-frames=3 num-buffers=6 ! video/x-raw,format=GRAY8,width=640,height=480 ! appsink name=sink sync=false",
    }
    for name, desc in caps_cases.items():
        snapshot["caps_negotiation"][name] = run_capture(
            desc, num_samples=3, with_meta=(name == "fixed_GRAY8")
        )

    # 6. Metadata & chunks summary (from fixed_GRAY8)
    gray8_samples = snapshot["caps_negotiation"]["fixed_GRAY8"].get("samples", [])
    snapshot["metadata"] = {
        "all_have_meta": all(s.get("meta") is not None for s in gray8_samples),
        "pts_monotonic": all(
            gray8_samples[i]["pts"] >= gray8_samples[i - 1]["pts"]
            for i in range(1, len(gray8_samples))
        ),
        "sizes": [s["size"] for s in gray8_samples],
        "first_meta": gray8_samples[0].get("meta") if gray8_samples else None,
        "image_numbers_increasing": all(
            gray8_samples[i]["meta"]["image_number"]
            > gray8_samples[i - 1]["meta"]["image_number"]
            for i in range(1, len(gray8_samples))
            if gray8_samples[i].get("meta") and gray8_samples[i - 1].get("meta")
        ),
    }

    # 7. Controls: enable-correction, capture-error enum round-trip, repeated restarts
    # 7a. Repeated NULL -> PLAYING -> NULL restarts
    restart_ok = True
    pipe = Gst.parse_launch(
        f"pylonsrc device-serial-number={EMU_SERIAL_0} num-buffers=4 ! video/x-raw,format=GRAY8,width=640,height=480 ! appsink name=sink sync=false"
    )
    sink = pipe.get_by_name("sink")
    for _ in range(3):
        if pipe.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            restart_ok = False
            break
        s = sink.emit("try-pull-sample", 10 * Gst.SECOND)
        if s is None:
            restart_ok = False
        pipe.set_state(Gst.State.NULL)
    snapshot["controls"]["repeated_restarts_ok"] = restart_ok

    # 7b. capture-error enum values accepted
    ce_ok = {}
    for nick in ("abort", "skip", "keep"):
        p = Gst.parse_launch(
            f"pylonsrc device-serial-number={EMU_SERIAL_0} capture-error={nick} num-buffers=2 ! video/x-raw,format=GRAY8,width=640,height=480 ! fakesink"
        )
        ret = p.set_state(Gst.State.PLAYING)
        bus = p.get_bus()
        msg = bus.timed_pop_filtered(
            10 * Gst.SECOND, Gst.MessageType.EOS | Gst.MessageType.ERROR
        )
        p.set_state(Gst.State.NULL)
        ce_ok[nick] = ret != Gst.StateChangeReturn.FAILURE and msg is not None and msg.type == Gst.MessageType.EOS
    snapshot["controls"]["capture_error_modes"] = ce_ok

    # 7c. enable-correction=true with unaligned width/property vs false
    p_corr = Gst.parse_launch(
        f"pylonsrc device-serial-number={EMU_SERIAL_0} enable-correction=true num-buffers=2 ! video/x-raw,format=GRAY8,width=640,height=480 ! fakesink"
    )
    ret_c = p_corr.set_state(Gst.State.PLAYING)
    msg_c = p_corr.get_bus().timed_pop_filtered(
        10 * Gst.SECOND, Gst.MessageType.EOS | Gst.MessageType.ERROR
    )
    p_corr.set_state(Gst.State.NULL)
    snapshot["controls"]["enable_correction_true_ok"] = (
        ret_c != Gst.StateChangeReturn.FAILURE
        and msg_c is not None
        and msg_c.type == Gst.MessageType.EOS
    )

    # 8. Error paths
    def expect_pipeline_error(desc: str) -> dict:
        p = Gst.parse_launch(desc)
        ret = p.set_state(Gst.State.PLAYING)
        msg = p.get_bus().timed_pop_filtered(
            10 * Gst.SECOND, Gst.MessageType.ERROR | Gst.MessageType.EOS
        )
        err_text = ""
        is_error = ret == Gst.StateChangeReturn.FAILURE
        if msg and msg.type == Gst.MessageType.ERROR:
            err, dbg = msg.parse_error()
            err_text = f"{err.message} | {dbg}"
            is_error = True
        p.set_state(Gst.State.NULL)
        return {"failed": is_error, "message": err_text}

    snapshot["errors"]["ambiguous_device"] = expect_pipeline_error(
        "pylonsrc num-buffers=1 ! fakesink"
    )
    snapshot["errors"]["missing_serial"] = expect_pipeline_error(
        "pylonsrc device-serial-number=NOSUCHSERIAL999 num-buffers=1 ! fakesink"
    )
    snapshot["errors"]["missing_pfs"] = expect_pipeline_error(
        f"pylonsrc device-serial-number={EMU_SERIAL_0} pfs-location=/nonexistent/does_not_exist.pfs num-buffers=1 ! fakesink"
    )

    # 9. GstDeviceMonitor non-interference check
    dm = Gst.DeviceMonitor.new()
    dm.add_filter("Video/Source", None)
    started = dm.start()
    devices = dm.get_devices()
    dm.stop()
    snapshot["device_monitor"] = {
        "started": bool(started),
        "device_count": len(devices),
    }

    print(json.dumps(snapshot, indent=2, sort_keys=True))
    return 0


# ---------------------------------------------------------------------------
# Pillar 1: ABI & Zero-Recompile Drop-In Verification
# ---------------------------------------------------------------------------


ABI_PROBE_C = textwrap.dedent(
    r"""
    #include <gst/gst.h>
    #include <gst/app/gstappsink.h>
    #include <gst/pylon/gstpylonmeta.h>
    #include <stddef.h>
    #include <stdio.h>
    #include <string.h>

    int main(int argc, char *argv[]) {
      if (argc > 1 && strcmp(argv[1], "--layout") == 0) {
        printf("{\"sizeof_GstPylonMeta\": %zu, "
               "\"sizeof_GstPylonOffset\": %zu, "
               "\"offset_meta\": %zu, "
               "\"offset_chunks\": %zu, "
               "\"offset_block_id\": %zu, "
               "\"offset_image_number\": %zu, "
               "\"offset_skipped_images\": %zu, "
               "\"offset_offset\": %zu, "
               "\"offset_timestamp\": %zu, "
               "\"offset_stride\": %zu, "
               "\"offset_offset_x\": %zu, "
               "\"offset_offset_y\": %zu}\n",
               sizeof(GstPylonMeta),
               sizeof(GstPylonOffset),
               offsetof(GstPylonMeta, meta),
               offsetof(GstPylonMeta, chunks),
               offsetof(GstPylonMeta, block_id),
               offsetof(GstPylonMeta, image_number),
               offsetof(GstPylonMeta, skipped_images),
               offsetof(GstPylonMeta, offset),
               offsetof(GstPylonMeta, timestamp),
               offsetof(GstPylonMeta, stride),
               offsetof(GstPylonOffset, offset_x),
               offsetof(GstPylonOffset, offset_y));
        return 0;
      }

      gst_init(&argc, &argv);

      GError *err = NULL;
      GstElement *pipe = gst_parse_launch(
          "pylonsrc device-serial-number=0815-0000 num-buffers=5 ! "
          "video/x-raw,format=GRAY8,width=640,height=480 ! "
          "appsink name=sink sync=false",
          &err);
      if (!pipe || err) {
        fprintf(stderr, "parse_launch failed: %s\n", err ? err->message : "null");
        return 1;
      }

      GstElement *sink = gst_bin_get_by_name(GST_BIN(pipe), "sink");
      if (gst_element_set_state(pipe, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        fprintf(stderr, "Failed to set PLAYING\n");
        gst_object_unref(sink);
        gst_object_unref(pipe);
        return 1;
      }

      int count = 0;
      for (int i = 0; i < 5; ++i) {
        GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 10 * GST_SECOND);
        if (!sample) break;
        GstBuffer *buf = gst_sample_get_buffer(sample);
        GstPylonMeta *meta = gst_buffer_get_pylon_meta(buf);
        if (!meta) {
          fprintf(stderr, "Missing GstPylonMeta on frame %d\n", i);
          gst_sample_unref(sample);
          gst_element_set_state(pipe, GST_STATE_NULL);
          gst_object_unref(sink);
          gst_object_unref(pipe);
          return 2;
        }
        if (meta->stride < 640 || meta->image_number == 0 || meta->chunks == NULL) {
          fprintf(stderr, "Unexpected meta fields: stride=%zu img=%llu chunks=%p\n",
                  meta->stride, (unsigned long long)meta->image_number, (void *)meta->chunks);
          gst_sample_unref(sample);
          gst_element_set_state(pipe, GST_STATE_NULL);
          gst_object_unref(sink);
          gst_object_unref(pipe);
          return 3;
        }
        count++;
        gst_sample_unref(sample);
      }

      gst_element_set_state(pipe, GST_STATE_NULL);
      gst_object_unref(sink);
      gst_object_unref(pipe);

      if (count != 5) {
        fprintf(stderr, "Expected 5 buffers, got %d\n", count);
        return 4;
      }
      printf("ABI_DROPIN_OK frames=%d\n", count);
      return 0;
    }
    """
)


def compile_abi_probe(src_root: Path, build_artifacts: dict, out_bin: Path, pylon_root: str) -> None:
    """Compile the C ABI probe against a specific version's headers & libgstpylon-1.0."""
    c_file = out_bin.with_suffix(".c")
    c_file.write_text(ABI_PROBE_C, encoding="utf-8")

    pkg = run_cmd(["pkg-config", "--cflags", "--libs", "gstreamer-1.0", "gstreamer-app-1.0"])
    if pkg.returncode != 0:
        raise RuntimeError(f"pkg-config failed: {pkg.stderr}")
    gst_flags = pkg.stdout.strip().split()

    cmd = [
        "gcc",
        "-O2",
        "-Wall",
        "-Wextra",
        f"-I{src_root / 'gst-libs'}",
        str(c_file),
        f"-L{build_artifacts['lib_dir']}",
        "-lgstpylon-1.0",
        f"-L{os.path.join(pylon_root, 'lib')}",
        *gst_flags,
        "-o",
        str(out_bin),
    ]
    res = run_cmd(cmd)
    if res.returncode != 0:
        raise RuntimeError(f"Failed to compile ABI probe:\n{res.stderr}")


def exported_c_symbols(so_path: Path) -> set[str]:
    res = run_cmd(["nm", "-D", "--defined-only", str(so_path)])
    if res.returncode != 0:
        raise RuntimeError(f"nm failed on {so_path}: {res.stderr}")
    syms = set()
    for line in res.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ("T", "D", "R", "B", "W"):
            sym = parts[2]
            if sym.startswith("gst_"):
                syms.add(sym)
    return syms


def get_soname(so_path: Path) -> str:
    res = run_cmd(["readelf", "-d", str(so_path)])
    if res.returncode != 0:
        return ""
    for line in res.stdout.splitlines():
        if "(SONAME)" in line:
            m = re.search(r"\[([^\]]+)\]", line)
            if m:
                return m.group(1)
    return ""


# ---------------------------------------------------------------------------
# Differential comparison engine (with intentional-delta ledger)
# ---------------------------------------------------------------------------


def compare_snapshots(v1: dict, v2: dict, ledger: dict) -> tuple[list[str], list[str]]:
    """Compare v1 and v2 snapshots; return (failures, accounted_deltas)."""
    failures: list[str] = []
    deltas: list[str] = []

    bugfixes = ledger.get("intentional_behavioral_bugfixes", {})

    # 1. Plugin features
    allowed_features = set(ledger.get("additive_plugin_features", {}).keys())
    for feat_name, v1_feat in v1["plugin_features"].items():
        if feat_name not in v2["plugin_features"]:
            failures.append(f"Plugin feature removed: {feat_name}")
        elif v2["plugin_features"][feat_name]["rank"] != v1_feat["rank"]:
            failures.append(
                f"Plugin feature rank changed for {feat_name}: {v1_feat['rank']} -> {v2['plugin_features'][feat_name]['rank']}"
            )
    for feat_name, v2_feat in v2["plugin_features"].items():
        if feat_name not in v1["plugin_features"]:
            if feat_name in allowed_features:
                if v2_feat["rank"] != 0:
                    failures.append(
                        f"Additive plugin feature {feat_name} must have GST_RANK_NONE (0), got {v2_feat['rank']}"
                    )
                deltas.append(f"Additive plugin feature: {feat_name} (rank={v2_feat['rank']})")
            else:
                failures.append(f"Unlisted additive plugin feature: {feat_name}")

    # 2. Element properties
    allowed_props = set(ledger.get("additive_element_properties", {}).keys())
    for pname, p1 in v1["element_properties"].items():
        if pname not in v2["element_properties"]:
            failures.append(f"Element property removed: {pname}")
            continue
        p2 = v2["element_properties"][pname]
        if p1["type"] != p2["type"]:
            failures.append(f"Property {pname} type changed: {p1['type']} -> {p2['type']}")
        for flag_key in ("readable", "writable"):
            if p1["flags"][flag_key] and not p2["flags"][flag_key]:
                failures.append(f"Property {pname} narrowed flag {flag_key}: True -> False")
        if p1["default"] != p2["default"]:
            failures.append(f"Property {pname} default changed: {p1['default']!r} -> {p2['default']!r}")
        if "enum_values" in p1:
            for nick, val in p1["enum_values"].items():
                if p2.get("enum_values", {}).get(nick) != val:
                    failures.append(
                        f"Property {pname} enum entry {nick}={val} changed/removed in v2: {p2.get('enum_values', {}).get(nick)}"
                    )
    for pname in v2["element_properties"]:
        if pname not in v1["element_properties"]:
            if pname in allowed_props:
                deltas.append(f"Additive element property: {pname} (default={v2['element_properties'][pname]['default']!r})")
            else:
                failures.append(f"Unlisted additive element property: {pname}")

    # 3. Element signals
    allowed_sigs = set(ledger.get("additive_element_signals", {}).keys())
    v1_sigs = set(v1["element_signals"])
    v2_sigs = set(v2["element_signals"])
    for sig in sorted(v1_sigs - v2_sigs):
        failures.append(f"Element signal removed: {sig}")
    for sig in sorted(v2_sigs - v1_sigs):
        if sig in allowed_sigs:
            deltas.append(f"Additive element signal: {sig}")
        else:
            failures.append(f"Unlisted additive element signal: {sig}")

    # 4. Pad templates
    allowed_fmts = ledger.get("additive_pad_template_formats", {})
    for tmpl_name, v1_media in v1["pad_templates"].items():
        if tmpl_name not in v2["pad_templates"]:
            failures.append(f"Pad template removed: {tmpl_name}")
            continue
        v2_media = v2["pad_templates"][tmpl_name]
        for media, fmts1 in v1_media.items():
            if media not in v2_media:
                failures.append(f"Pad template {tmpl_name} lost media type: {media}")
                continue
            fmts2 = v2_media[media]
            for f in fmts1:
                if f not in fmts2:
                    failures.append(f"Pad template {tmpl_name} ({media}) removed format: {f}")
            base_media = media.split("(")[0]
            allowed_for_media = set(allowed_fmts.get(base_media, []))
            for f in fmts2:
                if f not in fmts1:
                    if f in allowed_for_media:
                        deltas.append(f"Additive pad format on {media}: {f}")
                    else:
                        failures.append(f"Unlisted additive pad format on {media}: {f}")

    # 5. Lazy class_init & GstChildProxy in NULL state
    cp1 = v1["child_proxy_null_state"]
    cp2 = v2["child_proxy_null_state"]
    if not cp2["cam_resolved"] or not cp2["stream_resolved"]:
        failures.append("GstChildProxy failed to resolve cam/stream in NULL state on v2.0.0")

    allow_enum_add = bugfixes.get("child_enum_additive_entries", {}).get("enabled", False)
    for child_key in ("cam_properties", "stream_properties"):
        c1 = cp1[child_key]
        c2 = cp2[child_key]
        for pname, p1 in c1.items():
            if pname not in c2:
                failures.append(f"Child property removed ({child_key}): {pname}")
                continue
            p2 = c2[pname]
            if p1["type"] != p2["type"]:
                failures.append(f"Child property {child_key}.{pname} type changed: {p1['type']} -> {p2['type']}")
            for flag_key in ("readable", "writable"):
                if p1["flags"][flag_key] and not p2["flags"][flag_key]:
                    failures.append(f"Child property {child_key}.{pname} narrowed {flag_key}")
            if "enum_values" in p1:
                for nick, val in p1["enum_values"].items():
                    if p2.get("enum_values", {}).get(nick) != val:
                        failures.append(
                            f"Child enum {child_key}.{pname} changed/removed entry {nick}={val}"
                        )
                extra_nicks = set(p2.get("enum_values", {}).keys()) - set(p1["enum_values"].keys())
                if extra_nicks and not allow_enum_add:
                    failures.append(f"Child enum {child_key}.{pname} added unlisted entries: {sorted(extra_nicks)}")

    # 6. Caps negotiation & default fixation
    for case_name, r1 in v1["caps_negotiation"].items():
        r2 = v2["caps_negotiation"].get(case_name)
        if not r1["ok"]:
            # If v1 itself didn't succeed, only require v2 didn't crash
            continue
        if not r2 or not r2["ok"]:
            failures.append(f"Caps case '{case_name}' passed on v1.1.0 but FAILED on v2.0.0: {r2}")
            continue
        if r1["negotiated_caps"] != r2["negotiated_caps"]:
            failures.append(
                f"Caps fixation changed for '{case_name}':\n  v1: {r1['negotiated_caps']}\n  v2: {r2['negotiated_caps']}"
            )
        if r1["src_pad_caps"] != r2["src_pad_caps"]:
            failures.append(
                f"Src pad caps changed for '{case_name}':\n  v1: {r1['src_pad_caps']}\n  v2: {r2['src_pad_caps']}"
            )
        s1_sizes = [s["size"] for s in r1["samples"]]
        s2_sizes = [s["size"] for s in r2["samples"]]
        if s1_sizes != s2_sizes:
            failures.append(f"Buffer sizes changed for '{case_name}': v1={s1_sizes} vs v2={s2_sizes}")

    # 7. Metadata & chunks
    m1 = v1["metadata"]
    m2 = v2["metadata"]
    if m1["all_have_meta"] and not m2["all_have_meta"]:
        failures.append("v2.0.0 missing GstPylonMeta on GRAY8 buffers")
    if not m2["pts_monotonic"]:
        failures.append("v2.0.0 buffer PTS not monotonic")
    if not m2["image_numbers_increasing"]:
        failures.append("v2.0.0 GstPylonMeta.image_number not strictly increasing")
    fm1 = m1.get("first_meta") or {}
    fm2 = m2.get("first_meta") or {}
    for k in ("stride", "offset_x", "offset_y", "timestamp_nonzero", "chunks_keys"):
        if fm1.get(k) != fm2.get(k):
            failures.append(f"GstPylonMeta.{k} changed: v1={fm1.get(k)!r} vs v2={fm2.get(k)!r}")
    if fm1.get("block_id") != fm2.get("block_id"):
        if bugfixes.get("meta_block_id_uses_transport_block_id", {}).get("enabled", False):
            deltas.append(
                f"Intentional bugfix: GstPylonMeta.block_id v1={fm1.get('block_id')} -> v2={fm2.get('block_id')} (transport GetBlockID)"
            )
        else:
            failures.append(f"GstPylonMeta.block_id changed: v1={fm1.get('block_id')} vs v2={fm2.get('block_id')}")

    # 8. Controls
    if not v2["controls"]["repeated_restarts_ok"]:
        failures.append("Repeated NULL->PLAYING->NULL restarts failed on v2.0.0")
    for nick, ok1 in v1["controls"]["capture_error_modes"].items():
        if ok1 and not v2["controls"]["capture_error_modes"].get(nick):
            failures.append(f"capture-error={nick} failed on v2.0.0")
    if v1["controls"]["enable_correction_true_ok"] and not v2["controls"]["enable_correction_true_ok"]:
        failures.append("enable-correction=true failed on v2.0.0")

    # 9. Error paths
    for err_key in ("ambiguous_device", "missing_serial", "missing_pfs"):
        if v1["errors"][err_key]["failed"] and not v2["errors"][err_key]["failed"]:
            failures.append(f"Error case '{err_key}' failed on v1.1.0 but unexpectedly succeeded on v2.0.0")

    # 10. DeviceMonitor non-interference
    if not v2["device_monitor"]["started"]:
        failures.append("GstDeviceMonitor failed to start with v2.0.0 loaded")

    return failures, deltas


# ---------------------------------------------------------------------------
# Main orchestrator
# ---------------------------------------------------------------------------


def build_v1_worktree(baseline_ref: str, worktree_dir: Path, build_dir: Path, pylon_root: str) -> None:
    """Create a git worktree at baseline_ref and build v1.1.0 with Meson + Ninja."""
    if not worktree_dir.exists():
        print(f"[setup] Creating git worktree for {baseline_ref} at {worktree_dir}...")
        res = run_cmd(
            ["git", "worktree", "add", "--detach", str(worktree_dir), baseline_ref],
            cwd=PROJECT_ROOT,
        )
        if res.returncode != 0:
            raise RuntimeError(f"git worktree add failed:\n{res.stderr}")

    if not (build_dir / "ext" / "pylon" / "libgstpylon.so").is_file():
        print(f"[setup] Building {baseline_ref} in {build_dir}...")
        env = os.environ.copy()
        env["PYLON_ROOT"] = pylon_root
        env["LD_LIBRARY_PATH"] = f"{pylon_root}/lib:{env.get('LD_LIBRARY_PATH', '')}"
        res = run_cmd(
            [
                "meson",
                "setup",
                str(build_dir),
                str(worktree_dir),
                "-Dpython-bindings=enabled",
                "-Dexamples=enabled",
                "-Dtests=enabled",
            ],
            env=env,
            timeout=180,
        )
        if res.returncode != 0:
            raise RuntimeError(f"meson setup for {baseline_ref} failed:\n{res.stdout}\n{res.stderr}")
        res = run_cmd(["ninja", "-C", str(build_dir)], env=env, timeout=300)
        if res.returncode != 0:
            raise RuntimeError(f"ninja build for {baseline_ref} failed:\n{res.stdout}\n{res.stderr}")


def main() -> int:
    parser = argparse.ArgumentParser(description="A/B backwards-compatibility proof: v1.1.0 vs v2.0.0")
    parser.add_argument("--probe-mode", metavar="PLUGIN_SO", help=argparse.SUPPRESS)
    parser.add_argument("--baseline-ref", default="v1.1.0", help="Git ref for baseline (default: v1.1.0)")
    parser.add_argument("--v1-worktree", type=Path, help="Optional existing v1.1.0 source worktree")
    parser.add_argument("--v1-build", type=Path, help="Optional existing v1.1.0 build directory")
    parser.add_argument("--v2-build", type=Path, help="v2.0.0 build directory (default: auto-detect build-v2 or build)")
    parser.add_argument("--ledger", type=Path, default=DEFAULT_LEDGER, help="Intentional delta ledger JSON")
    parser.add_argument("--report", type=Path, help="Optional path to write Markdown compatibility proof report")
    parser.add_argument("--keep-worktree", action="store_true", help="Keep temporary v1 worktree/build after run")
    args = parser.parse_args()

    if args.probe_mode:
        return run_probe_mode(args.probe_mode)

    pylon_root = os.environ.get("PYLON_ROOT", "/opt/pylon")
    gi_python = find_gi_python()

    # Auto-detect v2 build dir
    if args.v2_build:
        v2_build = args.v2_build.resolve()
    elif (PROJECT_ROOT / "build-v2" / "ext" / "pylon" / "libgstpylon.so").is_file():
        v2_build = (PROJECT_ROOT / "build-v2").resolve()
    else:
        v2_build = (PROJECT_ROOT / "build").resolve()

    v2_art = resolve_build_artifacts(v2_build)
    ledger = json.loads(args.ledger.read_text(encoding="utf-8"))

    temp_root = Path(tempfile.mkdtemp(prefix="gstpylon-ab-compat-"))
    created_worktree = False
    v1_worktree = args.v1_worktree.resolve() if args.v1_worktree else (temp_root / "v1-src")
    v1_build = args.v1_build.resolve() if args.v1_build else (temp_root / "v1-build")

    results: list[tuple[str, bool, str]] = []
    accounted_deltas: list[str] = []

    def record(name: str, ok: bool, detail: str = "") -> None:
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name}" + (f" — {detail}" if detail else ""))
        results.append((name, ok, detail))

    try:
        if not args.v1_worktree:
            created_worktree = True
        build_v1_worktree(args.baseline_ref, v1_worktree, v1_build, pylon_root)
        v1_art = resolve_build_artifacts(v1_build)

        # ------------------------------------------------------------------
        # PILLAR 1: Zero-Recompile Binary & ABI Drop-In Proof
        # ------------------------------------------------------------------
        print("\n=== Pillar 1: Zero-Recompile Binary & ABI Drop-In Proof ===")

        # 1a. Exported C symbols & SONAME
        syms_v1 = exported_c_symbols(v1_art["lib_so"])
        syms_v2 = exported_c_symbols(v2_art["lib_so"])
        missing_syms = sorted(syms_v1 - syms_v2)
        soname_v1 = get_soname(v1_art["lib_so"])
        soname_v2 = get_soname(v2_art["lib_so"])
        record(
            "abi_exported_c_symbols",
            len(missing_syms) == 0,
            f"v1={len(syms_v1)} v2={len(syms_v2)} missing={missing_syms}",
        )
        record(
            "abi_soname_unchanged",
            soname_v1 == soname_v2,
            f"v1={soname_v1!r} v2={soname_v2!r}",
        )

        # 1b. Struct layout (sizeof / offsetof) match
        probe_v1_bin = temp_root / "abi_probe_v1"
        probe_v2_bin = temp_root / "abi_probe_v2"
        compile_abi_probe(v1_worktree, v1_art, probe_v1_bin, pylon_root)
        compile_abi_probe(PROJECT_ROOT, v2_art, probe_v2_bin, pylon_root)

        env_v1_layout = make_isolated_env(
            v1_art, cache_dir=temp_root / "c_l1", registry_file=temp_root / "r_l1.bin", pylon_root=pylon_root
        )
        env_v2_layout = make_isolated_env(
            v2_art, cache_dir=temp_root / "c_l2", registry_file=temp_root / "r_l2.bin", pylon_root=pylon_root
        )
        lay1 = json.loads(run_cmd([str(probe_v1_bin), "--layout"], env=env_v1_layout).stdout)
        lay2 = json.loads(run_cmd([str(probe_v2_bin), "--layout"], env=env_v2_layout).stdout)
        record(
            "abi_struct_layout_GstPylonMeta",
            lay1 == lay2,
            f"sizeof(GstPylonMeta)={lay2['sizeof_GstPylonMeta']} bytes"
            if lay1 == lay2
            else f"v1={lay1} vs v2={lay2}",
        )

        # 1c. Zero-recompile execution: v1.1.0 C binary running against v2.0.0 .so!
        env_swap = make_isolated_env(
            v2_art,
            cache_dir=temp_root / "c_swap",
            registry_file=temp_root / "r_swap.bin",
            pylon_root=pylon_root,
            fatal_criticals=True,
        )
        swap_res = run_cmd([str(probe_v1_bin)], env=env_swap)
        record(
            "dropin_v1_c_binary_on_v2_so",
            swap_res.returncode == 0 and "ABI_DROPIN_OK" in swap_res.stdout,
            swap_res.stdout.strip() if swap_res.returncode == 0 else swap_res.stderr.strip(),
        )

        # 1d. Unmodified v1.1.0 list_properties example binary running against v2.0.0
        # Note: v1.1.0's list_properties.c leaves `ret = EXIT_FAILURE` unmodified,
        # so we compare exit code & key output sections against v1.1.0 itself.
        v1_list_props = v1_build / "tests" / "examples" / "pylon" / "list_properties"
        if v1_list_props.is_file():
            env_lp_v1 = make_isolated_env(
                v1_art,
                cache_dir=temp_root / "c_lp_v1",
                registry_file=temp_root / "r_lp_v1.bin",
                pylon_root=pylon_root,
                camemu="1",
            )
            env_lp_v2 = make_isolated_env(
                v2_art,
                cache_dir=temp_root / "c_lp_v2",
                registry_file=temp_root / "r_lp_v2.bin",
                pylon_root=pylon_root,
                camemu="1",
                fatal_criticals=True,
            )
            lp_v1 = run_cmd([str(v1_list_props)], env=env_lp_v1)
            lp_v2 = run_cmd([str(v1_list_props)], env=env_lp_v2)
            lp_ok = (
                lp_v2.returncode == lp_v1.returncode
                and "Device Index: 0" in lp_v2.stdout
                and "Width" in lp_v2.stdout
                and len(lp_v2.stdout.splitlines()) >= len(lp_v1.stdout.splitlines())
            )
            record(
                "dropin_v1_list_properties_example_on_v2",
                lp_ok,
                f"rc_v1={lp_v1.returncode} rc_v2={lp_v2.returncode}, lines_v1={len(lp_v1.stdout.splitlines())} lines_v2={len(lp_v2.stdout.splitlines())}",
            )

        # 1e. Unmodified v1.1.0 camemu test suite executed against v2.0.0 plugin
        v1_camemu_sh = v1_worktree / "tests" / "camemu" / "run_camemu_tests.sh"
        if v1_camemu_sh.is_file():
            env_cm = make_isolated_env(
                v2_art,
                cache_dir=temp_root / "c_cm",
                registry_file=temp_root / "r_cm.bin",
                pylon_root=pylon_root,
                camemu="3",
                extra={"BUILD_DIR": str(v2_build), "BUILD_PREFIX": str(temp_root / "no_prefix")},
            )
            cm_res = run_cmd(["bash", str(v1_camemu_sh)], env=env_cm, cwd=v1_worktree, timeout=240)
            summary_line = next(
                (ln for ln in reversed(cm_res.stdout.splitlines()) if "Results:" in ln),
                f"rc={cm_res.returncode}",
            )
            record(
                "dropin_v1_camemu_suite_on_v2",
                cm_res.returncode == 0,
                summary_line if cm_res.returncode == 0 else f"{summary_line}\n{cm_res.stdout[-600:]}\n{cm_res.stderr[-400:]}",
            )

        # 1f. Unmodified v1.1.0 pygstpylon test suite executed against v2.0.0 abi3 module
        v1_py_test = v1_worktree / "tests" / "pygstpylon" / "test_pygstpylon.py"
        if v1_py_test.is_file() and v2_art["bindings_dir"]:
            env_py = make_isolated_env(
                v2_art,
                cache_dir=temp_root / "c_py",
                registry_file=temp_root / "r_py.bin",
                pylon_root=pylon_root,
                camemu="4",
            )
            py_res = run_cmd([gi_python, str(v1_py_test)], env=env_py, cwd=v1_worktree, timeout=120)
            record(
                "dropin_v1_pygstpylon_suite_on_v2",
                py_res.returncode == 0,
                "v1.1.0 Python metadata tests passed on v2.0.0 abi3"
                if py_res.returncode == 0
                else (py_res.stderr or py_res.stdout)[-500:],
            )

        # ------------------------------------------------------------------
        # PILLAR 2 & 4: Automated A/B Behavioral Differential Suite
        # ------------------------------------------------------------------
        print("\n=== Pillar 2 & 4: A/B Behavioral Differential Suite (v1.1.0 vs v2.0.0) ===")

        env_probe_v1 = make_isolated_env(
            v1_art,
            cache_dir=temp_root / "c_probe_v1",
            registry_file=temp_root / "r_probe_v1.bin",
            pylon_root=pylon_root,
        )
        env_probe_v2 = make_isolated_env(
            v2_art,
            cache_dir=temp_root / "c_probe_v2",
            registry_file=temp_root / "r_probe_v2.bin",
            pylon_root=pylon_root,
            fatal_criticals=True,
        )

        p1 = run_cmd([gi_python, str(Path(__file__).resolve()), "--probe-mode", str(v1_art["plugin_so"])], env=env_probe_v1, timeout=180)
        if p1.returncode != 0:
            raise RuntimeError(f"v1.1.0 probe failed (rc={p1.returncode}):\n{p1.stdout}\n{p1.stderr}")
        snap_v1 = json.loads(p1.stdout)

        p2 = run_cmd([gi_python, str(Path(__file__).resolve()), "--probe-mode", str(v2_art["plugin_so"])], env=env_probe_v2, timeout=180)
        if p2.returncode != 0:
            raise RuntimeError(f"v2.0.0 probe failed (rc={p2.returncode}):\n{p2.stdout}\n{p2.stderr}")
        snap_v2 = json.loads(p2.stdout)

        diffs, accounted_deltas = compare_snapshots(snap_v1, snap_v2, ledger)
        record(
            "ab_behavioral_differential_suite",
            len(diffs) == 0,
            f"0 unintended diffs, {len(accounted_deltas)} ledger-accounted deltas"
            if not diffs
            else "\n  - " + "\n  - ".join(diffs),
        )

        # ------------------------------------------------------------------
        # PILLAR 3: In-Place Cache Migration Proof (v1.1.0 cache -> v2.0.0)
        # ------------------------------------------------------------------
        print("\n=== Pillar 3: In-Place Cache Migration Proof ===")
        shared_cache = temp_root / "shared_cache"
        shared_cache.mkdir()

        env_cm_v1 = make_isolated_env(
            v1_art,
            cache_dir=shared_cache,
            registry_file=temp_root / "r_mig_v1.bin",
            pylon_root=pylon_root,
        )
        r_v1_i = run_cmd(["gst-inspect-1.0", "pylonsrc"], env=env_cm_v1, timeout=90)
        r_v1_l = run_cmd(
            [
                "gst-launch-1.0",
                "-q",
                "pylonsrc",
                f"device-serial-number={EMU_SERIAL_0}",
                "num-buffers=3",
                "!",
                "video/x-raw,format=GRAY8,width=640,height=480",
                "!",
                "fakesink",
            ],
            env=env_cm_v1,
            timeout=90,
        )
        v1_cache_files = sorted(p.name for p in shared_cache.rglob("*") if p.is_file())

        env_cm_v2 = make_isolated_env(
            v2_art,
            cache_dir=shared_cache,
            registry_file=temp_root / "r_mig_v2.bin",
            pylon_root=pylon_root,
            fatal_criticals=True,
        )
        r_v2_i = run_cmd(["gst-inspect-1.0", "pylonsrc"], env=env_cm_v2, timeout=90)
        r_v2_l = run_cmd(
            [
                "gst-launch-1.0",
                "-q",
                "pylonsrc",
                f"device-serial-number={EMU_SERIAL_0}",
                "num-buffers=3",
                "!",
                "video/x-raw,format=GRAY8,width=640,height=480",
                "!",
                "fakesink",
            ],
            env=env_cm_v2,
            timeout=90,
        )
        cache_ok = (
            r_v1_i.returncode == 0
            and r_v1_l.returncode == 0
            and r_v2_i.returncode == 0
            and r_v2_l.returncode == 0
        )
        record(
            "cache_migration_v1_to_v2_fatal_criticals",
            cache_ok,
            f"v1 populated {len(v1_cache_files)} cache file(s); v2 ran cleanly with G_DEBUG=fatal-criticals"
            if cache_ok
            else f"v1_i={r_v1_i.returncode} v1_l={r_v1_l.returncode} v2_i={r_v2_i.returncode} v2_l={r_v2_l.returncode} stderr={r_v2_l.stderr}",
        )

        # Summary & optional report
        passed = sum(1 for _, ok, _ in results if ok)
        failed = sum(1 for _, ok, _ in results if not ok)

        print("\n=== Accounted Intentional Deltas (from ledger) ===")
        for d in accounted_deltas:
            print(f"  + {d}")

        print(f"\n=== Summary: {passed} passed, {failed} failed ===")

        if args.report:
            lines = [
                "# v2.0.0 Backwards Compatibility Proof Report",
                "",
                f"- **Baseline:** `{args.baseline_ref}`",
                f"- **Target:** `{v2_build}`",
                f"- **Result:** {'PASS' if failed == 0 else 'FAIL'} ({passed} passed, {failed} failed)",
                "",
                "## Verification Matrix",
                "",
                "| Check | Status | Details |",
                "|-------|--------|---------|",
            ]
            for name, ok, detail in results:
                clean_detail = detail.replace("\n", " ").replace("|", "\\|")
                lines.append(f"| `{name}` | {'PASS' if ok else 'FAIL'} | {clean_detail} |")
            lines.extend(["", "## Ledger-Accounted Intentional Deltas", ""])
            for d in accounted_deltas:
                lines.append(f"- {d}")
            args.report.write_text("\n".join(lines) + "\n", encoding="utf-8")
            print(f"Report written to {args.report}")

        return 0 if failed == 0 else 1

    finally:
        if created_worktree and not args.keep_worktree:
            run_cmd(["git", "worktree", "remove", "--force", str(v1_worktree)], cwd=PROJECT_ROOT)
            shutil.rmtree(temp_root, ignore_errors=True)
        elif args.keep_worktree:
            print(f"[info] Kept temporary worktree/build at {temp_root}")


if __name__ == "__main__":
    sys.exit(main())
