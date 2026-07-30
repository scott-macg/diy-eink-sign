#!/usr/bin/env python3
"""
Battery Curve Refinement & C++ Array Generator
DIY 2.9" E-Ink Sign Project

Reads utils/battery_curve.csv and processes empirical voltage samples using:
1. Startup transient filtering (t >= 180s)
2. Moving median noise reduction (window = 11 samples)
3. Empirical mapping for measured range (100% down to current test voltage)
4. Hybrid blending with standard LiPo discharge chemistry curves for remaining range.
"""

import sys
import os

LOG_FILE = os.path.join(os.path.dirname(__file__), "battery_curve.csv")

def process_curve(log_path=LOG_FILE):
    if not os.path.exists(log_path):
        print(f"Error: {log_path} not found.")
        sys.exit(1)

    raw_data = []
    with open(log_path, "r") as f:
        lines = f.readlines()[1:]
        for l in lines:
            parts = l.strip().split(",")
            if len(parts) >= 3:
                t = int(parts[0])
                v = float(parts[2])
                raw_data.append((t, v))

    total_samples = len(raw_data)
    duration_sec = raw_data[-1][0] if raw_data else 0
    duration_hrs = duration_sec / 3600.0

    print("==================================================")
    print(" 🔋 Battery Discharge Curve Analysis")
    print("==================================================")
    print(f" • Log File:        {log_path}")
    print(f" • Total Samples:   {total_samples}")
    print(f" • Total Duration:  {duration_sec}s ({duration_hrs:.2f} hours)")
    print(f" • Peak Voltage:    {max(v for _, v in raw_data):.4f} V")
    print(f" • Latest Voltage:  {raw_data[-1][1]:.4f} V")

    # Filter out initial 180 seconds startup transient
    valid_data = [(t, v) for t, v in raw_data if t >= 180]
    if not valid_data:
        valid_data = raw_data

    pts = [v for _, v in valid_data]

    # Moving median filter (window size = 11)
    def moving_median(pts, w=11):
        res = []
        half = w // 2
        for i in range(len(pts)):
            s = max(0, i - half)
            e = min(len(pts), i + half + 1)
            sub = sorted(pts[s:e])
            res.append(sub[len(sub) // 2])
        return res

    smoothed = moving_median(pts, w=11)
    v_max = max(smoothed)
    v_min = min(smoothed)

    print(f" • Filtered Range:  {v_max:.3f} V down to {v_min:.3f} V")
    print("==================================================\n")

    # LiPo Chemistry Reference Model (for unmeasured lower region)
    lipo_model = {
        60: 3.840,
        50: 3.790,
        40: 3.750,
        30: 3.700,
        20: 3.650,
        10: 3.520,
         0: 3.300
    }

    # Empirical Mapping for measured upper region (100% -> 70%)
    empirical_targets = {
        100: v_max,
         90: v_max - (v_max - v_min) * 0.33,
         80: v_max - (v_max - v_min) * 0.66,
         70: v_min
    }

    cpp_lines = [
        "// Auto-generated Hybrid LiPo Discharge Curve",
        f"// Based on {duration_hrs:.2f} hours empirical profiling data ({total_samples} samples)",
        "static const VoltagePoint PROGMEM BATTERY_CURVE[] = {"
    ]

    pcts = [100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0]

    for i, pct in enumerate(pcts):
        if pct in empirical_targets and empirical_targets[pct] >= v_min:
            v_val = empirical_targets[pct]
            source = "Empirical Data"
        else:
            v_val = lipo_model[pct]
            source = "LiPo Chemistry Model"

        mv = int(v_val * 1000)
        comma = "," if i < len(pcts) - 1 else ""
        cpp_lines.append(f"    {{ {mv:4d}, {pct:3d} }}{comma}  // {v_val:.3f}V = {pct}% ({source})")

    cpp_lines.append("};")

    code_block = "\n".join(cpp_lines)
    print(code_block)
    return code_block

if __name__ == "__main__":
    process_curve()
