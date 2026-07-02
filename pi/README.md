# Pi AI Node — 3.5" SPI Display + Edge AI

Evolved from the CYD-Phone hackathon project.

## Overview

Full-stack Raspberry Pi project: driving a 3.5" SPI display (Gowin FPGA → ILI9486) via the kernel's DRM driver, with a Tkinter homescreen, touch calibration, animated wallpaper, and a planned upgrade to a **Raspberry Pi 5 (16 GB)** for fully local, cloud-free AI inference.

## Project Structure

```
pi/
├── desktop-panel.py          # Tkinter homescreen (clock, shortcuts, stats, animated GIF)
├── dt-overlay/
│   └── rpi-lcd-35-dc.dts     # Device Tree overlay for ILI9486 (DC=GPIO24, 4-wire SPI)
├── config/
│   ├── config.txt            # Deployed Pi config.txt
│   ├── openbox-autostart.sh  # Openbox autostart (touch cal, compositor, panels)
│   ├── picom.conf            # Picom compositor config
│   ├── Xresources            # URxvt colors/font/geometry
│   └── autostart             # Desktop panel autostart
└── scripts/
    ├── start-desktop.sh      # Boot flow (network → DRM → X)
    ├── calibrate-touch.py    # 4-corner touch calibration (libinput matrix)
    └── live-glass.sh         # Aesthetic glass overlay script
```

## Hardware

| Component | Detail |
|-----------|--------|
| **Pi** | 3B (upgrading to Pi 5 16 GB) |
| **Display** | 3.5" SPI, 480×320, Gowin FPGA → ILI9486 |
| **Touch** | ADS7846 resistive (SPI CE1) |
| **Stack** | Pi OS (DietPi/Trixie, aarch64), Xorg/modesetting, Openbox, Tkinter |

## Current Features

- **4-wire SPI + DC pin (GPIO 24)** — kernel `ili9486.ko` DRM driver
- **Touch calibration** — 6-parameter affine transform via libinput
- **Desktop panel** — animated rain GIF wallpaper, glass clock, app shortcuts, live CPU/RAM/uptime stats
- **Boot flow** — waits for network (Ethernet/WiFi), then DRM device, then startx
- **Compositor** — picom with blur + opacity (xrender backend)

## Next: Edge AI Node

Target hardware: **Raspberry Pi 5 (16 GB)** + Hailo-8L AI Kit + NVMe SSD

Goals:
- Run quantized LLMs (Llama, Mistral) fully offline
- Local vector DB for RAG (retrieval-augmented generation)
- No cloud dependency — complete data sovereignty
- Document the build as a guide for other teens

## Origin

This project grew out of the [CYD-Phone](https://github.com/TheDrop123/CYD-Phone) hackathon project at **Jugend Hackt Berlin**. The phone taught me hardware-hacking; the Pi teaches me Linux, drivers, display protocols, and now AI on the edge.

---

Built by Elias / The Drop, 15, Berlin.
