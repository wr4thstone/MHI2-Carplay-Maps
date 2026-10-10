# MOI3 H22 / 0334 investigation

Status: offline investigation only. This directory does not provide a MIB3
installer, compatible runtime, or validated vehicle modification.

Target reported by the owner: Passat GTE 2020, H22, installed MU 0334,
MOI3_SEQREL_EU_VWMQB_P33409P. Cluster 3G0920320F is provisional.
The requested behavior is independent displays: audio on the main CarPlay
screen and a phone-rendered navigation map on the instrument cluster.

## Evidence from supplied firmware

- `multi.mnf` describes a 0308 stage followed by a 0334 stage. The supplied
  0334 `main.mnf` identifies MOI3_EU_VWMQB_P33400P, version 334.0.5, MU 0334.
- The supplied application binaries are AArch64 Linux. The existing project's
  MHI2/QNX runtime cannot be treated as a compatible binary payload.
- SAL app 334.0.5 creates/registers a CarPlay screen and configures dimensions,
  physical size, frame rate, UUID and input properties. Its video window uses
  `ivi-app-id`. CarPlay screenstream 334.0.4 uses a shared delegate table.
- In AirPlay 334.0.4, `AirPlayReceiverSessionScreen_CopyDisplaysInfo` reads
  `ScreenCopyMain`. The session setup screen branch has a duplicate-setup
  guard. These observations concern inspected paths; they do not prove that
  every possible secondary-screen extension is absent.
- RouteGuidance.xml's Instrument Cluster entries describe navigation metadata,
  not a map video stream. The owner reports phone-driven guidance and artwork
  already working without factory navigation active.
- PictureLink libraries from 304.0.1 and 334.0.5 are byte-identical. Their
  exposed functions encode image-resource links; they do not implement a video
  transport.
- Navigation 334.0.5 requests dimensions and a window identifier through RSI
  `displaymanagement/renderingSurfaces`.
- Display manager 334.0.5 captures IVI shared buffers, uses a Renesas OpenMAX
  H.264 encoder, and queues output through V4L2. Its surface-name mapping
  assigns `x-navfpk` internal identifier 37 and `x-nav` identifier 19. These
  values are not established physical cluster display IDs.
- The display manager discovers `*talker*` entries beneath
  `/sys/devices/platform/avb_video`, reads `ip_dst_6_addr` and `rtp_dst_port`,
  matches them to requested stream parameters, then reads `device_path`.
  Its discovery code reads these attributes; it does not configure a new
  destination in that step.

## Reproducible binary locations

Offsets below are virtual addresses in the named supplied ELF build, not
patch instructions and not portable across versions.

| Binary | Address | Observation |
| --- | --- | --- |
| CarPlay support 334.0.4 | 0x214c | ScreenRegister appends a registered screen |
| CarPlay screenstream 334.0.4 | 0x13c4 | SetStreamScreenDelegate copies a shared callback table |
| CarPlay AirPlay 334.0.4 | 0x1a6d4 | Display-info path calls ScreenCopyMain |
| SAL app 334.0.5 | 0x56fe48 / 0x56ffcc | Calls ScreenCreate / ScreenRegister |
| Display manager MIB3 334.0.5 | 0x41ddb4 | Surface-name to internal-ID mapping |
| Display manager MIB3 334.0.5 | 0x41d4b8 | AVB talker discovery and matching |
| Display manager MIB3 334.0.5 | 0x41ef38 | Copies IP, port and frame rate from stream object |

## Candidate architecture and unresolved boundaries

A separate phone map stream could be decoded into a dedicated IVI surface,
then captured, encoded and handed to an existing matching AVB talker. The
middle stages exist in the inspected firmware. Neither end-to-end operation
nor the cluster destination has been demonstrated.

Required before a working port can be claimed:

1. Implement and validate separate CarPlay display advertisement, negotiation
   and video dispatch while retaining the main display.
2. Identify who provisions the cluster's AVB talker and RSI rendering surface.
3. Establish the real cluster dimensions, stream settings and display ownership.
4. Obtain a supported development execution and recovery route on an equivalent
   H22/0334 unit, then perform integration testing.

Main-screen mirroring would not satisfy the independent-map requirement.
Keep the existing MHI2 installer compatibility gates intact.

## Read-only diagnostic for a future development session

`tools/inspect-avb-talkers.sh` reads only the three known sysfs attributes.
It does not configure talkers, open video devices, start streams, remount
filesystems or change installed files. Run it only when shell access is
available; firmware archives cannot supply live sysfs values.

For offline checks, pass a fixture directory as its first argument. Exit 0
means readable talkers were listed; exit 2 means no directory/talkers; exit 3
means an attribute could not be read. Listing a talker is not proof that it
belongs to the cluster or that it can safely accept another stream.

No proprietary firmware payloads are included in this research directory.
