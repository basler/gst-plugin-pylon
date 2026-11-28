# Preconfigured startup prototype

Can an application load a PFS or a camera user set, start grabbing, and look up
GenICam children later without running register-write probes during acquisition?

After building the plugin, run the A/B probe with three same-model CamEmu cameras.

```sh
python3 tests/prototypes/preconfigured_startup.py builddir --user-set Default
```

Use `--camemu-count 8` for a larger test. To benchmark USB hardware, pass a
`--serial` for each connected camera and optionally `--pfs /path/to/camera.pfs`.
The hardware benchmark loads the selected user set and PFS into the camera.
The child lookup runs after all appsinks have delivered a first sample.

The probe enables the experiment using
`GST_PYLON_PROTOTYPE_FAST_PRECONFIGURED=1`. The session applies the saved
configuration, then defers each child until first lookup. It preserves explicit
user-set timing and skips writes of matching geometry and format. Caps still
go through normal GStreamer negotiation.

The prototype builds a separate GObject class per device. This keeps
state-dependent property descriptions local to each camera. Late schema
construction uses current access information, broad numeric bounds, and
placeholder defaults. It avoids selector and transport-lock probing. Actual
property reads and writes still go through the selected camera's nodemap.
The prototype's property inventory and flags can vary with the camera state
at first lookup. Before making this the default, derive a stable schema from
static descriptions and validate any sharing by nodemap identity, including
firmware and transport. Keep live feature access and configuration per camera.

The benchmark reports startup, total late child lookup time, negotiated caps,
and CamEmu frame offset gaps. Its same-model test checks that changing gain on
one emulated camera leaves the others' gain values intact.

## Measured results

Optimized build with pylon 12.2.1 and GStreamer 1.28.9. Each cell is median
time from pipeline construction to the first sample from every camera.

| Configuration | Eager cold | Deferred cold | Eager repeat | Deferred repeat |
|---|---:|---:|---:|---:|
| ace 1 USB, explicit `user-set=Auto` | 3,701 ms | 717 ms | 772 ms | 684 ms |
| ace 1 USB, 640×480 Mono8 PFS | 3,832 ms | 672 ms | 781 ms | 652 ms |
| Three CamEmu cameras, distinct PFS files | 376 ms | 64 ms | 78 ms | 71 ms |
| Eight CamEmu cameras, distinct PFS files | 1,224 ms | 184 ms | 185 ms | 206 ms |

Three trials per hardware and eight-camera condition. Five per three-camera
condition. The ace 1 negotiated the saved 20 fps in the deferred user-set
case and 30 fps in the eager case. The PFS run used 20 fps in both modes.
Median first child lookups on the ace 1 took 1.0 seconds with PFS and
1.8–1.9 seconds with its full-resolution user set. Eight emulated cameras
showed frame-offset gaps in both modes. The physical test used one camera.
