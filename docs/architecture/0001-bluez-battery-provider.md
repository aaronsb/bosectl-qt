# ADR 0001: Publish battery state via BlueZ Battery Provider API

- **Status:** Proposed
- **Date:** 2026-04-12
- **Deciders:** @aaronsb

## Context

bosectl-qt polls headphone state over a proprietary BMAP connection on a
classic Bluetooth rfcomm socket. Battery percentage is one of the fields
returned on every poll and is already surfaced in the tray menu and left-click
notification.

The rest of the Linux desktop — GNOME Power indicator, Plasma battery widget,
`upower -d`, kscreenlocker lock screen, etc. — gets peripheral battery levels
through UPower, which in turn reads `org.bluez.Battery1` on the D-Bus system
bus. That interface is populated by BlueZ in two ways:

1. **Built-in GATT Battery Service (UUID 0x180F)** — works automatically for
   BLE peripherals that expose the service. Not applicable to BMAP
   headphones, which are classic Bluetooth.
2. **Battery Provider API** (`org.bluez.BatteryProviderManager1`) — lets a
   userspace process register itself as a provider and push battery levels
   for a given device path. BlueZ then re-exports those values as
   `org.bluez.Battery1` so UPower sees them like any other source.

Today, a user who wants to know their QC Ultra 2 battery level without
opening our tray menu has no native option — nothing surfaces it system-wide.
HFP's AT-level battery indicators are sometimes available via BlueZ's
experimental flag, but coverage is device-firmware dependent and outside our
control.

## Decision

bosectl-qt will register as a BlueZ Battery Provider for the currently
connected device and publish the BMAP-reported battery percentage each time
`BmapWorker::emitStatus()` computes a fresh reading.

Concretely:

- On connect, call
  `org.bluez.BatteryProviderManager1.RegisterBatteryProvider(ObjectPath)` on
  the adapter.
- Export an object at that path implementing `org.bluez.BatteryProvider1`
  with `Device` (object path of the bluez device) and `Percentage` properties.
- Update `Percentage` on each poll; emit `PropertiesChanged` so UPower picks
  it up without latency.
- On disconnect or app exit, unregister the provider.

The implementation lives in a new `BluezBatteryProvider` class that the
`TrayIcon` / `BmapWorker` layers talk to via Qt signals — keeping the D-Bus
surface out of `BmapWorker` itself, which should stay focused on the BMAP
protocol.

## Non-goals

- **No "real driver" for Bose listening devices yet.** This ADR scopes
  strictly to userspace D-Bus plumbing for battery reporting. A more
  ambitious "bosectl as a proper driver" is worth exploring later — but
  not now, for two reasons:
    1. *Device coverage is thin.* bosectl-qt only kind of supports two
       headphone models today (QC Ultra 2 and one earlier QC Ultra). A
       driver-shaped abstraction is premature when the device matrix it
       would abstract over barely exists. The right time to design a
       driver is after we've onboarded enough devices to know what the
       common surface actually looks like.
    2. *The design is undecided.* The author's current leaning is that a
       "driver" here would still be a userspace daemon — probably a
       system-bus D-Bus service that owns the rfcomm connection and
       exposes a device-agnostic control surface — rather than a kernel
       module. But that's a gut feeling, not a decision. A real driver
       ADR should wait until we can make that call from evidence.

  The current decision is deliberately narrow: publish battery via an
  existing, well-defined BlueZ API. That buys most of the user-visible
  integration for a fraction of the design cost, and doesn't foreclose a
  future driver-shaped evolution.
- **No HFP battery dependency.** We do not rely on BlueZ's experimental HFP
  battery path; we push our own readings regardless of what HFP reports.
- **No changes to the existing tray / notification UI.** Battery continues
  to show in bosectl-qt's own surfaces; this ADR only adds a second sink.

## Alternatives considered

1. **Do nothing — keep battery in the tray only.** Lowest cost but misses
   native integration; users with a system battery widget have to open our
   menu just to glance.
2. **Rely on BlueZ HFP battery indicators via `Experimental = true`.** Not
   in our control, device dependent, and requires users to opt into an
   experimental BlueZ config. Path of least code but worst UX guarantees.
3. **Ship a shim tool (e.g. a small standalone daemon) that runs alongside
   bosectl-qt and publishes to BlueZ.** Adds packaging complexity (two
   binaries, one systemd user unit) for no real benefit over doing it
   in-process. Rejected.
4. **A proper "driver" for Bose listening devices** (likely a userspace
   daemon exposing a device-agnostic D-Bus service; kernel module not
   currently on the table). Deferred until device coverage justifies the
   abstraction — see Non-goals. Out of scope here.

## Consequences

**Positive**
- Battery appears in every Linux battery indicator that talks to UPower —
  Plasma, GNOME, tint2, waybar, `upower -d`, etc. — with zero user
  configuration.
- No new privileges: `org.bluez.BatteryProviderManager1` is on the system
  bus but userspace-callable; no polkit rule or setuid required.
- Tied to the BMAP poll loop we already have, so no new I/O budget.

**Negative / trade-offs**
- D-Bus plumbing adds ~150–250 lines, a new class, and a new runtime
  dependency on BlueZ ≥ 5.56 (when the Battery Provider API landed). AUR
  PKGBUILD will need to declare the minimum version.
- The Battery Provider API was still marked experimental in some distros
  until relatively recently; we should detect unavailability gracefully
  (log a warning, continue without it) rather than hard-fail startup.
- Battery updates are pull-driven by our poll interval. For most users this
  is fine, but UPower consumers may see coarser updates than they would
  from a BLE device that pushes GATT notifications.
- A second sink means two places to keep in sync if we ever expose more
  detail (e.g. charging state, which BMAP may or may not report).

## Implementation notes (non-binding)

- Qt has `QtDBus` as a first-class module — already a Qt dependency — so
  no new libraries are needed.
- The adapter object path comes from `org.bluez.Manager1.GetManagedObjects`
  or more simply from walking `/org/bluez/hci0` style paths. Use
  `ObjectManager` to avoid hardcoding.
- The device object path follows the pattern
  `/org/bluez/hci0/dev_68_F2_1F_05_94_2F`; we already know the MAC.
- Handle the case where multiple adapters exist: register against the one
  the device is connected to, not unconditionally `hci0`.
- Unregister on `BmapWorker::disconnected` so stale battery readings don't
  linger after the headphones go away.
