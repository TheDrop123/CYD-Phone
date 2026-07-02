# CYD-Phone & Pi AI Node

Two hardware projects by Elias / The Drop, 15, Berlin.

---

## CYD-Phone

A touchscreen school tablet based on the Cheap Yellow Display (ESP32 WROOM), designed as a privacy-safe alternative to smartphones in classrooms.

### English

Schools usually ban phones because they can be used to record people, take photos, cheat on tests, distract students, and enable cyberbullying. Since phones have cameras and microphones, students could record teachers or classmates without permission, which can cause privacy and legal issues especially in Germany where this project is based, in particular §201a StGB.

Schools enforce bans mainly through rules:
- Phones stay in bags, lockers, or locked pouches.
- Phones may be confiscated if used during class.

They generally cannot disable cameras or microphones on personal phones, so they reduce the risk by restricting phone use instead.

So this is where the CYD-Phone comes in. It is a touchscreen (Cheap Yellow Display) powered by an ESP32 WROOM. The key functions planned for this device are in the educational field: file sharing via a cloud service hosted on a server provided by THE DROP, messaging using the NOSTR protocol, a notes app with drawing support, and WebUntis API integration for school schedules.

### Deutsch

Schulen verbieten Mobiltelefone meist deshalb, weil sie dazu genutzt werden konnen, Personen aufzunehmen, Fotos zu machen, bei Prufungen zu schummeln, Schuler abzulenken und Cybermobbing zu ermoglichen.

Da Smartphones uber Kameras und Mikrofone verfugen, konnten Schuler Lehrer oder Mitschuler ohne deren Zustimmung aufnehmen. Dies kann zu Problemen hinsichtlich der Privatsphare und zu rechtlichen Schwierigkeiten fuhren - insbesondere in Deutschland, wo dieses Projekt angesiedelt ist (hier ist vor allem § 201a StGB relevant).

Schulen setzen Verbote hauptsachlich durch Regeln durch:
- Handys mussen in der Tasche, im Spind oder in verschliebbaren Beuteln aufbewahrt werden.
- Handys konnen bei Nutzung wahrend des Unterrichts eingezogen werden.

Da die Schulen die Kameras oder Mikrofone privater Smartphones im Allgemeinen nicht deaktivieren konnen, verringern sie das Risiko stattdessen durch eine Einschrankung der Handynutzung.

Genau hier kommt das CYD-Phone ins Spiel. Es handelt sich um ein Touchscreen-Gerat (Cheap Yellow Display), das von einem ESP32-WROOM-Modul angetrieben wird. Die geplanten Hauptfunktionen liegen im Bildungsbereich: Dateiaustausch uber einen Cloud-Dienst, Messaging uber das NOSTR-Protokoll, eine Notiz-App mit Zeichenfunktion und WebUntis-API-Integration fur den Stundenplan.

### SchoolOS

### Apps

#### Calc (Taschenrechner)
Ein Standard-Taschenrechner fur tagliche Rechnungen.
- Grundrechenarten: Plus, Minus, Mal, Geteilt
- Spezialfunktionen: x2 (Quadrieren), Wr. (Wurzel), 1/x (Kehrwert), ^ (Potenz)
- C loscht die aktuelle Eingabe

#### Draw (Zeichnen)
Schnelle kreative Skizzen.
- 8 Farben in der Leiste unten links
- RAD schaltet Radiergummi ein/aus
- NEU loscht das Bild
- SZ+ und SZ- andert die Stiftdicke
- RAUS / ZURUCK beendet die App

#### Notes (Notizen)
Textnotizen auf der SD-Karte.
- + fur neue Notiz
- Tippen auf eine Notiz zum Lesen/Bearbeiten
- Rotes X loscht die Notiz

#### Chat
Globaler CYD-Chat uber WLAN.
- Erfordert aktive WLAN-Verbindung
- Nachrichten per virtueller Tastatur
- Automatische Synchronisation

#### Read (MD-Reader)
Liest Markdown-Dateien von der SD-Karte.
- SD-Karte erforderlich
- Zeigt bis zu 20 Dateien
- Tippen zum Weiterblattern

#### WebUntis (Stundenplan)
Stundenplananzeige.
- Erfordert WLAN
- Tagesauswahl (Mo-Sa)
- Zeigt Fach, Lehrer, Raum, Uhrzeit

#### Settings (Einstellungen)
- WiFi/Zeit ein/aus
- Zeitzone: Sommer-/Winterzeit
- WLAN oder SD-Karte neu laden

#### Virtuelle Tastatur
- 123/ABC wechselt zwischen Buchstaben und Zahlen
- Pfeil nach oben: Gro-/Kleinbuchstaben
- SPACE: Leerzeichen
- Pfeil links: Zeichen loschen
- OK: Eingabe speichern

Von: RominoKowalski, Gartenprofi, gnampfkuchen-oss, WX_79, The_Drop

---

## Pi AI Node

Full-stack Raspberry Pi project: driving a 3.5" SPI display (Gowin FPGA to ILI9486) via the kernel DRM driver, with a Tkinter homescreen, touch calibration, animated wallpaper, and a planned upgrade to a Raspberry Pi 5 (16 GB) for fully local AI inference.

### Project Structure

```
pi-display/
  desktop-panel.py          Tkinter homescreen (clock, shortcuts, stats, animated GIF)
  dt-overlay/
    rpi-lcd-35-dc.dts       Device Tree overlay for ILI9486 (DC=GPIO24, 4-wire SPI)
  config/
    config.txt              Pi boot config
    openbox-autostart.sh    Openbox autostart
    picom.conf              Compositor config
    Xresources              URxvt colors/font/geometry
    autostart               Desktop panel autostart
  scripts/
    start-desktop.sh        Boot flow (network to DRM to X)
    calibrate-touch.py      4-corner touch calibration
    live-glass.sh           Glass overlay script
```

### Hardware

| Component | Detail |
|-----------|--------|
| Pi | 3B (upgrading to Pi 5 16 GB) |
| Display | 3.5" SPI, 480x320, Gowin FPGA to ILI9486 |
| Touch | ADS7846 resistive (SPI CE1) |
| Stack | DietPi/Trixie, aarch64, Xorg/modesetting, Openbox, Tkinter |

### Current Features

- 4-wire SPI with DC pin (GPIO 24) — kernel ili9486 DRM driver
- Touch calibration — 6-parameter affine transform via libinput
- Desktop panel — animated rain GIF wallpaper, glass clock, app shortcuts, live CPU/RAM/uptime stats
- Boot flow — waits for network, then DRM device, then startx
- Compositor — picom with blur and opacity (xrender backend)

### Next: Edge AI Node

Target hardware: Raspberry Pi 5 (16 GB) + Hailo-8L AI Kit + NVMe SSD

Goals:
- Run quantized LLMs (Llama, Mistral) fully offline
- Local vector DB for RAG
- No cloud dependency
- Document the build as a guide for other teens

### Origin

This project grew out of the CYD-Phone hackathon project at Jugend Hackt Berlin. The phone taught hardware hacking; the Pi teaches Linux, drivers, display protocols, and edge AI.
