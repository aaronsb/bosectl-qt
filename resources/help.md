<!--
Help text for bosectl-qt's Help window, compiled into the app.

  # Title {#frame-id}    a frame in the Help window (menu, nc, modes, eq)
  ## Title {#item-id}    a clickable item; its body is the Markdown below it
  ### Title {#item-id}   a choice under the item above it

The ids are fixed: the Help window looks them up, and tests/test_help.cpp
fails if an id is missing here or no longer used by the window. Titles and
bodies are free to change. The first sentence of each body also becomes
the tooltip on the matching control in the real menu, so keep it short and
self-contained.
-->

# Tray menu {#menu}

## Headphones {#menu.device}

The name of the connected headphones, or "disconnected". Hover it to reach **Rename...**.

### Rename... {#menu.rename}

Change the name your headphones show to phones and computers when they pair. Names can be up to 31 bytes, which is fewer characters if you use accents or emoji.

## Battery {#menu.battery}

Charge level of the headphones. bosectl-qt also shares it with your desktop, so it appears in the system battery indicator next to your laptop battery.

## About {#menu.about}

Details about the headphones and this app.

### Firmware {#menu.firmware}

The software version running on the headphones. Update it with Bose's own app; bosectl-qt never changes firmware.

### MAC {#menu.mac}

The Bluetooth address of the headphones. Useful when you have more than one pair or are reporting a problem.

## Help... {#menu.help}

Opens this window.

## Noise Cancellation... {#menu.noise-cancellation}

Opens the noise cancellation slider, for fine control between blocking the world out and hearing it.

## Modes... {#menu.modes}

Opens the list of listening modes. Switch between them, or create your own with a saved noise cancellation level, spatial audio and wind setting.

## Equalizer... {#menu.equalizer}

Opens the three-band equalizer for adjusting bass, mid and treble.

## Spatial Audio {#menu.spatial}

Makes stereo sound as if it comes from speakers around you instead of from inside your head.

### Off {#menu.spatial.off}

Normal stereo.

### Room {#menu.spatial.room}

Sound stays anchored in front of you, like speakers in a room. Turn your head and the sound stays put.

### Head Tracking {#menu.spatial.head}

Sound follows your head, so the stage is always in front of you wherever you face.

## Sidetone {#menu.sidetone}

Plays your own voice back into the headphones during calls, so you can hear yourself and don't end up talking too loudly.

### Off {#menu.sidetone.off}

You hear only the other person.

### Low {#menu.sidetone.low}

A little of your own voice.

### Medium {#menu.sidetone.medium}

More of your own voice.

### High {#menu.sidetone.high}

The most of your own voice.

## Noise Cancellation (ANC) {#menu.anc}

Turns active noise cancellation on or off. It has to be on for the noise cancellation slider to do anything.

## Wind Block {#menu.wind-block}

Reduces the roar of wind on the microphones outdoors. While it is on, it overrides the noise cancellation slider.

## Multipoint {#menu.multipoint}

Stays connected to two devices at once, such as a laptop and a phone, so you can move between them without re-pairing.

## Auto-Pause {#menu.auto-pause}

Pauses playback when you take the headphones off.

## Connect {#menu.connect}

Connects to the headphones. They must already be paired and connected to this computer in your Bluetooth settings.

## Power Off {#menu.power-off}

Turns the headphones off. Press their power button to turn them back on.

## Start on Login {#menu.start-on-login}

Starts bosectl-qt automatically when you log in. It also appears under System Settings → Autostart, where you can remove it too.

## Quit {#menu.quit}

Closes bosectl-qt. The headphones keep their current settings.

# Noise Cancellation {#nc}

## Noise cancellation level {#nc.slider}

How much outside sound is blocked, from **Max NC** (0, quietest) to **Ambient** (10, hear your surroundings). Needs ANC on and Wind Block off to take effect.

## Apply {#nc.apply}

Sends the chosen level to the headphones.

# Modes {#modes}

## Mode list {#modes.list}

Every listening mode on the headphones. Built-in modes such as Quiet and Aware come with the headphones and can't be edited. Custom modes are yours to change. The active mode is marked with ◀.

## New {#modes.new}

Creates a custom mode from the settings below. A custom mode can't use a built-in mode's name, such as Quiet or Aware.

## Delete {#modes.delete}

Deletes the selected custom mode. Built-in modes can't be deleted.

## Name {#modes.name}

The mode's name, as it appears in this list and in the Bose app.

## Noise Cancel {#modes.cnc}

The noise cancellation level this mode switches to, from 0 (most blocking) to 10 (most ambient).

## Spatial {#modes.spatial}

The spatial audio setting this mode switches to: Off, Room or Head Tracking. See Spatial Audio in the tray menu.

## Wind Block {#modes.wind-block}

Whether this mode reduces wind noise on the microphones.

## ANC Toggle {#modes.anc-toggle}

Whether active noise cancellation is on in this mode.

## Activate {#modes.activate}

Switches the headphones to the selected mode.

## Save {#modes.save}

Saves your changes to the selected custom mode.

# Equalizer {#eq}

## Bass {#eq.bass}

Low frequencies: kick drums, bass lines, rumble. From −10 to +10, where 0 is Bose's default tuning.

## Mid {#eq.mid}

The middle range where voices and most instruments sit.

## Treble {#eq.treble}

High frequencies: cymbals, the edges of consonants, detail. Too much can sound harsh.

## Try {#eq.try}

Sends the slider settings to the headphones so you can listen, without remembering them.

## Save {#eq.save}

Sends the settings to the headphones and remembers them.

## Reset {#eq.reset}

Sets all three bands back to 0, Bose's default tuning, and sends that to the headphones. Press Save to keep it.
