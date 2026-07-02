#!/usr/bin/env python3
"""Interactive touch calibration for ADS7846 on 480x320 display.

Takes multiple taps per corner and averages to handle noisy readings.
"""
import evdev, subprocess, os, sys, math, time, signal

SCREEN_W = 480
SCREEN_H = 320

def find_touch():
    for fn in evdev.list_devices():
        d = evdev.InputDevice(fn)
        if 'ADS7846' in d.name:
            return fn, d
    print("Touch device not found")
    sys.exit(1)

def wait_for_tap(dev, timeout=30):
    touch_down = False
    x = y = None
    start = time.time()
    for event in dev.read_loop():
        if time.time() - start > timeout:
            return None, None
        if event.type == evdev.ecodes.EV_ABS:
            if event.code == evdev.ecodes.ABS_X:
                x = event.value
            elif event.code == evdev.ecodes.ABS_Y:
                y = event.value
        elif event.type == evdev.ecodes.EV_KEY and event.code == evdev.ecodes.BTN_TOUCH:
            if event.value == 1:
                touch_down = True
            elif event.value == 0 and touch_down and x is not None and y is not None:
                return x, y

def show_prompt(screen_x, screen_y, msg):
    env = os.environ.copy()
    import glob
    matches = glob.glob('/tmp/serverauth.*')
    if matches:
        env['XAUTHORITY'] = matches[0]
    env['DISPLAY'] = ':0'
    subprocess.run(['xdotool', 'mousemove', str(screen_x), str(screen_y)], env=env)
    msg_x = max(0, min(screen_x - 50, SCREEN_W - 120))
    msg_y = max(0, screen_y - 40)
    subprocess.Popen([
        'xmessage', '-geometry', f'130x30+{msg_x}+{msg_y}',
        '-buttons', '', '-center', msg
    ], env=env)
    def dismiss():
        subprocess.run(['pkill', 'xmessage'], capture_output=True)
        time.sleep(0.2)
    return dismiss

def calibrate():
    print("=== Touch Calibration for ADS7846 ===")
    print("Tap the crosshair position 5 times for each corner.")
    print("Hold still and tap firmly.")
    print()

    path, dev = find_touch()
    print(f"Device: {dev.name} at {path}")
    print(f"ABS range: X=0..4095, Y=0..4095")
    print()

    targets = [
        (5, 5, "TOP-LEFT"),
        (SCREEN_W - 5, 5, "TOP-RIGHT"),
        (5, SCREEN_H - 5, "BOTTOM-LEFT"),
        (SCREEN_W - 5, SCREEN_H - 5, "BOTTOM-RIGHT"),
    ]

    raw_points = []
    screen_points = []

    for sx, sy, label in targets:
        dismiss = show_prompt(sx, sy, f"Tap {label} 10x")
        raw_xs, raw_ys = [], []
        print(f"\n--- {label} at ({sx}, {sy}) ---")
        print("Tap 10 times (first tap is discarded as warmup)...")
        warmup = True
        while len(raw_xs) < 10:
            rx, ry = wait_for_tap(dev)
            if rx is None:
                print("Timeout! Tap again.")
                continue
            if warmup:
                print(f"  warmup: X={rx}, Y={ry} (discarded)")
                warmup = False
                continue
            raw_xs.append(rx)
            raw_ys.append(ry)
            print(f"  {len(raw_xs)}/10: raw X={rx}, raw Y={ry}")

        avg_x = sum(raw_xs) / len(raw_xs)
        avg_y = sum(raw_ys) / len(raw_ys)
        print(f"  Average: X={avg_x:.0f}, Y={avg_y:.0f}")

        raw_points.append((avg_x, avg_y))
        screen_points.append((sx, sy))
        dismiss()

    print("\n=== Calibration data (averaged) ===")
    for i in range(4):
        print(f"  Screen ({screen_points[i][0]}, {screen_points[i][1]}) "
              f"<-> Raw ({raw_points[i][0]:.0f}, {raw_points[i][1]:.0f})")

    # Compute affine transform using least squares
    # screen_x = a*raw_x + b*raw_y + c
    # screen_y = d*raw_x + e*raw_y + f
    A = []
    b_vec = []
    for (rx, ry), (sx, sy) in zip(raw_points, screen_points):
        A.append([rx, ry, 1, 0, 0, 0])
        A.append([0, 0, 0, rx, ry, 1])
        b_vec.append(sx)
        b_vec.append(sy)

    # Normal equations: (A^T * A) * M = A^T * b
    AtA = [[0]*6 for _ in range(6)]
    Atb = [0]*6
    for i in range(8):
        row = A[i]
        bi = b_vec[i]
        for j in range(6):
            Atb[j] += row[j] * bi
            for k in range(6):
                AtA[j][k] += row[j] * row[k]

    # Gaussian elimination
    n = 6
    for col in range(n):
        max_row = col
        for row in range(col+1, n):
            if abs(AtA[row][col]) > abs(AtA[max_row][col]):
                max_row = row
        AtA[col], AtA[max_row] = AtA[max_row], AtA[col]
        Atb[col], Atb[max_row] = Atb[max_row], Atb[col]

        pivot = AtA[col][col]
        if abs(pivot) < 1e-15:
            print(f"WARNING: Singular matrix at col {col}")
            continue

        for row in range(col+1, n):
            factor = AtA[row][col] / pivot
            for k in range(col, n):
                AtA[row][k] -= factor * AtA[col][k]
            Atb[row] -= factor * Atb[col]

    M = [0]*n
    for i in range(n-1, -1, -1):
        s = Atb[i]
        for j in range(i+1, n):
            s -= AtA[i][j] * M[j]
        M[i] = s / AtA[i][i]

    a, b, c, d, e, f = M

    # Convert to libinput normalized matrix (0-1 coords)
    norm_a = a * 4095.0 / (SCREEN_W - 1)
    norm_b = b * 4095.0 / (SCREEN_W - 1)
    norm_c = c / (SCREEN_W - 1)
    norm_d = d * 4095.0 / (SCREEN_H - 1)
    norm_e = e * 4095.0 / (SCREEN_H - 1)
    norm_f = f / (SCREEN_H - 1)

    print(f"\n=== Libinput Calibration Matrix ===")
    print(f"  [{norm_a:.6f}, {norm_b:.6f}, {norm_c:.6f}]")
    print(f"  [{norm_d:.6f}, {norm_e:.6f}, {norm_f:.6f}]")
    print(f"  [0, 0, 1]")
    print()

    args = [
        'xinput', 'set-prop', '6',
        'libinput Calibration Matrix',
        f'{norm_a:.6f}', f'{norm_b:.6f}', f'{norm_c:.6f}',
        f'{norm_d:.6f}', f'{norm_e:.6f}', f'{norm_f:.6f}',
        '0', '0', '1'
    ]
    print(f"Running: {' '.join(args)}")
    env = os.environ.copy()
    import glob
    matches = glob.glob('/tmp/serverauth.*')
    if matches:
        env['XAUTHORITY'] = matches[0]
    env['DISPLAY'] = ':0'
    subprocess.run(args, env=env)

    print("\nDone! Tap around to test.")

if __name__ == '__main__':
    calibrate()
