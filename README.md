# CYD-Phone

```
cyd/          CYD-Phone ESP32 school tablet
```

---

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

### Apps

#### Calc (Taschenrechner)
- Grundrechenarten: Plus, Minus, Mal, Geteilt
- Spezialfunktionen: x2 (Quadrieren), Wr. (Wurzel), 1/x (Kehrwert), ^ (Potenz)
- C loscht die aktuelle Eingabe

#### Draw (Zeichnen)
- 8 Farben in der Leiste unten links
- RAD schaltet Radiergummi ein/aus
- NEU loscht das Bild
- SZ+/SZ- andert die Stiftdicke

#### Notes (Notizen)
- + fur neue Notiz
- Tippen zum Lesen/Bearbeiten
- Rotes X loscht die Notiz

#### Chat
- Globaler CYD-Chat uber WLAN
- Nachrichten per virtueller Tastatur
- Automatische Synchronisation

#### Read (MD-Reader)
- List Markdown-Dateien von der SD-Karte
- Zeigt bis zu 20 Dateien

#### WebUntis (Stundenplan)
- Erfordert WLAN
- Tagesauswahl (Mo-Sa)
- Zeigt Fach, Lehrer, Raum, Uhrzeit

#### Settings (Einstellungen)
- WiFi/Zeit ein/aus
- Zeitzone umschalten
- WLAN/SD-Karte neuladen

### Contributors

- WX-79
- RominoKowalski
- TheDrop123
- Gartenprofi
- gnampfkuchen-oss