#!/usr/bin/env python3
import sys, os, glob, datetime, subprocess, signal, time
import tkinter as tk
from tkinter import font as tkfont

SW, SH = 480, 320
GIF_DIR = '/home/dietpi/Pictures/gif_frames'
GIF_COUNT = 33
GIF_INTERVAL = 62  # 16 fps, roughly

class DesktopPanel:
    def __init__(self):
        self.root = tk.Tk()
        self.root.overrideredirect(True)
        self.root.geometry(f'{SW}x{SH}+0+0')
        self.root.attributes('-topmost', False)
        self.cv = tk.Canvas(self.root, width=SW, height=SH, highlightthickness=0, bg='#000000')
        self.cv.pack()
        self._fonts()
        self.root.after(200, self._set_desktop_type)
        self._render()
        self.root.after(100, self._first_update)
        self.cv.bind('<Button-1>', self._click)
        signal.signal(signal.SIGTERM, lambda *a: self._quit())
        self.root.mainloop()

    def _first_update(self):
        self._update_clock()
        self._update_stats()

    def _fonts(self):
        # try jetbrains first, fall back to dejavu
        for s in [26,22,18,14,12,10,9,8]:
            n = f'f{s}'
            try: setattr(self, n, tkfont.Font(family='JetBrains Mono', size=s))
            except: setattr(self, n, tkfont.Font(family='DejaVu Sans Mono', size=s))
        try: self.fb = tkfont.Font(family='JetBrains Mono', size=14, weight='bold')
        except: self.fb = tkfont.Font(family='DejaVu Sans Mono', size=14, weight='bold')

    def _set_desktop_type(self):
        # xdotool works, xlib kept giving badwindow errors
        try:
            wid = hex(self.root.winfo_id())
            subprocess.run(['xdotool', 'set_window', '--type', 'desktop', wid],
                           capture_output=True, timeout=3)
        except: pass

    def _load_gif_frames(self):
        # preload all 33 frames, some might fail idk
        self._gif_frames = []
        for i in range(GIF_COUNT):
            path = os.path.join(GIF_DIR, f'frame_{i:04d}.ppm')
            if os.path.exists(path):
                try:
                    photo = tk.PhotoImage(file=path)
                    self._gif_frames.append(photo)
                except:
                    self._gif_frames.append(None)
            else:
                self._gif_frames.append(None)

    def _get_stats(self):
        # read from sysfs, should work on dietpi
        stats = {}
        try:
            with open('/sys/class/thermal/thermal_zone0/temp') as f:
                stats['cpu_temp'] = round(int(f.read().strip()) / 1000, 1)
        except: stats['cpu_temp'] = '?'
        try:
            with open('/proc/meminfo') as f:
                for line in f:
                    if line.startswith('MemTotal:'): total = int(line.split()[1]) // 1024
                    if line.startswith('MemAvailable:'): avail = int(line.split()[1]) // 1024
            stats['ram'] = f'{total - avail}M / {total}M'
        except: stats['ram'] = '?'
        try:
            with open('/proc/uptime') as f:
                up = float(f.read().split()[0])
                h, m = int(up // 3600), int((up % 3600) // 60)
                stats['uptime'] = f'{h}h {m}m'
        except: stats['uptime'] = '?'
        return stats

    def _render(self):
        self.cv.delete('all')
        # load the animated rain gif
        self._load_gif_frames()
        if any(f is not None for f in self._gif_frames):
            self._gif_idx = 0
            self._gif_bg = self.cv.create_image(0, 0, image=self._gif_frames[0], anchor='nw')
            self.root.after(GIF_INTERVAL, self._animate_gif)
        else:
            self.cv.create_rectangle(0, 0, SW, SH, fill='#262A32', outline='')
            self.cv.create_rectangle(0, 0, SW, SH, fill='#000000', stipple='gray25', outline='')

        # clock top center
        now = datetime.datetime.now()
        self._clock_id = self.cv.create_text(SW//2, 70, text='', fill='#FFFFFF', font=self.f26)
        self.cv.create_text(SW//2, 108, text='', fill='#D0D0D0', font=self.f12, tags='date')

        # shortcuts in the middle
        shortcuts = [
            ('WWW', 'Firefox', 'firefox-esr'),
            ('>_', 'Terminal', 'urxvt'),
            ('N', 'Notes', '/usr/local/bin/aesthetic-notes'),
            ('■', 'Apps', '/usr/bin/urxvt -e /usr/local/bin/tux'),
            ('~', 'Surf', 'surf'),
        ]
        n = len(shortcuts)
        bw = 60; bh = 56; gap = 8
        total_w = n * bw + (n-1) * gap
        ox = (SW - total_w) // 2
        oy = 140
        self._shortcut_buttons = []
        for i, (icon, label, cmd) in enumerate(shortcuts):
            bx = ox + i * (bw + gap)
            self.cv.create_rectangle(bx, oy, bx+bw, oy+bh, fill='#333333', outline='#666666', width=1)
            self.cv.create_text(bx+bw//2, oy+18, text=icon, fill='#FFFFFF', font=self.f18)
            self.cv.create_text(bx+bw//2, oy+42, text=label, fill='#BBBBBB', font=self.f9)
            self._shortcut_buttons.append({'cmd': cmd, 'x1': bx, 'y1': oy, 'x2': bx+bw, 'y2': oy+bh})

        # stats centered under the shortcuts (moved from 220 to 245)
        # TODO: maybe add a transparent bg rect if its hard to read
        self._stats_ids = {}
        sx = SW//2; sy = 245
        self._stats_ids['cpu_temp'] = self.cv.create_text(sx, sy, text='', fill='#BBBBBB', font=self.f10)
        self._stats_ids['ram'] = self.cv.create_text(sx, sy+17, text='', fill='#BBBBBB', font=self.f10)
        self._stats_ids['uptime'] = self.cv.create_text(sx, sy+34, text='', fill='#BBBBBB', font=self.f9)

    def _animate_gif(self):
        # cycle through the preloaded frames at ~16fps
        if not hasattr(self, '_gif_frames') or not self._gif_frames:
            return
        self._gif_idx = (self._gif_idx + 1) % GIF_COUNT
        frame = self._gif_frames[self._gif_idx]
        if frame:
            try:
                self.cv.itemconfig(self._gif_bg, image=frame)
            except:
                pass
        self.root.after(GIF_INTERVAL, self._animate_gif)

    def _update_clock(self):
        now = datetime.datetime.now()
        time_str = now.strftime('%I:%M %p').lstrip('0')
        date_str = now.strftime('%A, %B %d').lstrip('0').replace('  ', ' ')
        try:
            self.cv.itemconfig(self._clock_id, text=time_str)
            for tag in self.cv.find_withtag('date'):
                self.cv.itemconfig(tag, text=date_str)
        except: pass
        self.root.after(30000, self._update_clock)

    def _update_stats(self):
        stats = self._get_stats()
        try:
            self.cv.itemconfig(self._stats_ids['cpu_temp'], text=f'CPU {stats["cpu_temp"]}°C')
            self.cv.itemconfig(self._stats_ids['ram'], text=f'RAM {stats["ram"]}')
            self.cv.itemconfig(self._stats_ids['uptime'], text=f'Up {stats["uptime"]}')
        except: pass
        self.root.after(10000, self._update_stats)

    def _click(self, e):
        # TODO: maybe add long press for context menu? idk if we need it rn
        for b in self._shortcut_buttons:
            if b['x1'] <= e.x <= b['x2'] and b['y1'] <= e.y <= b['y2']:
                env = os.environ.copy()
                env['DISPLAY'] = ':0'
                for auth in ['/tmp/serverauth.*', '/home/dietpi/.Xauthority']:
                    for p in glob.glob(auth):
                        env['XAUTHORITY'] = p; break
                    if 'XAUTHORITY' in env: break
                try:
                    subprocess.Popen(['bash', '-c', b['cmd']],
                                     env=env, start_new_session=True,
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                except: pass
                break

    def _quit(self):
        self.root.destroy(); os._exit(0)

if __name__ == '__main__':
    if os.fork() > 0: sys.exit(0)
    os.setsid()
    if os.fork() > 0: sys.exit(0)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    DesktopPanel()
