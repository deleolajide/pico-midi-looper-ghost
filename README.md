# Pico MIDI Looper "Ghost" Edition

![Social Preview](https://github.com/user-attachments/assets/6663c239-353e-4700-8ac1-3b25fa713bc6)

> *Simple beats in. Ghost notes out.*  
> "Ghost" explores how complexity and musicality can emerge from the simplest possible interface.  
> This is not just a MIDI sequencer. It’ s a haunted drum machine.  
> Born from a single button, running on a $4 microcontroller, it plays rhythms shaped by invisible hands.


## Concept and Function

**Pico MIDI Looper "Ghost" Edition** is a USB-MIDI drum machine that transforms simple rhythmic input into rich, expressive patterns —  all on a stock Raspberry Pi Pico.  
You tap out a rhythm using a single button. The device records the sequence of button presses over a two-bar span, and then replays it with ghost notes —  subtle, generative variations that breathe life into the beat.

There is no screen. No knobs. No software to install. You connect the Pico to your computer or mobile device, and it shows up as a USB-MIDI instrument.  
From there, you can use it with GarageBand, Ableton Live, or any DAW or soft synth that supports MIDI input.

Everything runs directly on the Pico itself —  no additional components, soldering, or host software are required.  
A ghost lives within the device.

## Key Features

- **Two-bar, four-track MIDI drum machine** built from the simplest possible components
  A one-button interface records your rhythm. Ghost notes expand it.
  It’ s an exploration of how complex rhythm can arise from minimal control.
- **Ghost note generation**: adds swing, density, and surprise to basic patterns
- **USB-MIDI output**: works with DAWs and mobile music apps (e.g., GarageBand on iOS)
- **No display or controls**: the interface is minimalist, but expressive
- **Runs on an unmodified Pico**: no soldering, batteries, or external parts
- **Fully standalone firmware**: no host-side software or configuration needed

## Demo

Watch it in action:
[YouTube –  "Ghost" Edition Demo](https://www.youtube.com/shorts/HNQDrlHFJ74)

## Getting Started

1. Download the prebuilt `.uf2` firmware from the [Releases](https://github.com/oyama/pico-midi-looper-ghost/releases) page.
2. Hold the **BOOTSEL** button while connecting your Raspberry Pi Pico to your computer via USB.
3. Drag and drop the `.uf2` file onto the USB mass storage device that appears.
4. The device will reboot and show up as a USB-MIDI class-compliant device.
5. Open your favorite DAW or mobile music app (GarageBand, Ableton, etc.). When you hear a click, it's ready. Press the button to begin your dialogue with the ghost.

## Technology Background

This project is a fork of [**pico-midi-looper**](https://github.com/oyama/pico-midi-looper), a minimal BLE/USB MIDI drum machine designed for educational and artistic use on Raspberry Pi Pico boards.  
"Ghost" extends the original looper with generative ghost note logic and a more autonomous feel, while preserving the same ultra-minimal hardware and interface.

All functionality is implemented in C using the [Pico SDK](https://github.com/raspberrypi/pico-sdk). The firmware is compact, stateless, and hackable, intended for both musical performance and embedded experimentation.

## License

This project is released under the [BSD 3-Clause License](LICENSE).  
Feel free to remix, adapt, and haunt your own hardware.
