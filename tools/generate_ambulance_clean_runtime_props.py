from __future__ import annotations

import math
from pathlib import Path

import generate_ambulance_proxy_meshes as base


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "rooms" / "ambulance" / "clean"


def writer(name: str) -> base.ObjWriter:
    base.OUT = OUT
    return base.ObjWriter(name)


def write_materials() -> None:
    base.OUT = OUT
    base.write_materials()
    extra_materials = {
        "tape_white": ((0.92, 0.91, 0.84), (0.016, 0.015, 0.012), 8.0),
        "warning_orange": ((0.92, 0.38, 0.08), (0.060, 0.026, 0.010), 18.0),
        "dried_wear": ((0.23, 0.18, 0.14), (0.006, 0.005, 0.004), 6.0),
        "cable_black": ((0.010, 0.012, 0.012), (0.020, 0.020, 0.018), 14.0),
    }
    path = OUT / "ambulance_proxy_materials.mtl"
    with path.open("a", encoding="utf-8") as file:
        for name, (color, specular, shininess) in extra_materials.items():
            r, g, b = color
            sr, sg, sb = specular
            file.write(f"newmtl {name}\n")
            file.write(f"Kd {r:.3f} {g:.3f} {b:.3f}\n")
            file.write(f"Ks {sr:.3f} {sg:.3f} {sb:.3f}\n")
            file.write(f"Ns {shininess:.1f}\n")


def runtime_shell() -> None:
    w = writer("ambulance_runtime_shell")
    # Compact ambulance capsule: off-white ABS panels, dark rubber floor, serviceable metal rails.
    w.box(0.0, -0.035, -0.36, 2.48, 0.07, 6.17, "rubber_floor")
    w.box(0.0, 2.035, -0.36, 2.48, 0.07, 6.17, "ceiling_panel")
    w.box(-1.24, 1.015, -0.36, 0.07, 2.03, 6.17, "abs_panel")
    w.box(1.24, 1.015, -0.36, 0.07, 2.03, 6.17, "abs_panel")
    w.box(0.0, 0.93, -3.45, 2.48, 1.86, 0.07, "abs_shadow")
    w.box(0.0, 0.93, 2.72, 2.48, 1.86, 0.07, "abs_shadow")

    # Wall and floor seams stop the scene reading as a single plastic box.
    for z in (-2.80, -1.70, -0.55, 0.62, 1.78):
        w.box(-1.185, 1.03, z, 0.035, 1.70, 0.030, "grime")
        w.box(1.185, 1.03, z, 0.035, 1.70, 0.030, "grime")
        w.box(0.0, 2.000, z, 2.02, 0.026, 0.035, "abs_shadow")
    for x in (-0.72, -0.36, 0.36, 0.72):
        w.box(x, 0.012, -0.36, 0.030, 0.020, 5.72, "grime")
    for z in (-2.35, -1.05, 0.28, 1.38):
        w.box(-0.02, 0.024, z, 1.78, 0.018, 0.035, "dried_wear")
    w.box(0.62, 0.025, 2.08, 0.46, 0.020, 0.20, "warning_orange")
    w.box(0.62, 0.030, 2.08, 0.38, 0.018, 0.050, "tape_white")

    # Left-side storage and attendant bench, visible from the stretcher camera.
    w.box(-1.19, 1.20, -1.88, 0.12, 0.90, 2.24, "cabinet_off_white")
    for z in (-2.48, -1.90, -1.32):
        w.box(-1.255, 1.22, z, 0.035, 0.64, 0.42, "cabinet_glass")
        w.box(-1.278, 0.88, z, 0.025, 0.06, 0.34, "stainless")
    w.box(-1.06, 0.36, 0.40, 0.34, 0.30, 3.60, "abs_shadow")
    w.box(-1.04, 0.54, 0.40, 0.38, 0.08, 3.70, "blanket_desaturated_blue")
    w.box(-1.02, 0.71, 0.40, 0.055, 0.30, 3.66, "stainless")

    # Right-side equipment panels, ports and rear-door shapes.
    w.box(1.19, 1.18, 0.24, 0.12, 0.78, 2.32, "cabinet_off_white")
    for z in (-0.55, 0.12, 0.78):
        w.box(1.255, 1.34, z, 0.035, 0.18, 0.34, "paper_label")
        w.box(1.275, 1.05, z, 0.025, 0.11, 0.11, "oxygen_green")
        w.box(1.278, 0.88, z + 0.10, 0.020, 0.09, 0.09, "label_blue")
        w.box(1.282, 1.21, z - 0.18, 0.018, 0.035, 0.24, "cable_black")
        w.box(1.284, 1.03, z - 0.05, 0.016, 0.24, 0.024, "cable_black")
    for x in (-0.44, 0.44):
        w.box(x, 1.34, 2.675, 0.46, 0.54, 0.030, "cabinet_glass")
        w.box(x, 0.78, 2.675, 0.52, 0.10, 0.035, "stainless")

    # Ceiling lighting and rails: warm practicals plus cold clinical strip.
    w.box(0.0, 1.965, 0.82, 1.18, 0.035, 0.28, "amber_light")
    w.box(0.0, 1.962, -1.72, 1.00, 0.030, 0.22, "soft_white")
    for x in (-0.82, 0.82):
        w.box(x, 1.86, -0.35, 0.045, 0.045, 5.20, "stainless")
    for z in (-2.40, -0.25, 1.56):
        w.box(0.0, 1.82, z, 1.70, 0.040, 0.040, "stainless")

    # Small readable labels and wear markers.
    w.box(-1.278, 1.66, -2.88, 0.026, 0.20, 0.48, "label_blue")
    w.box(1.278, 1.62, -1.15, 0.026, 0.16, 0.42, "label_yellow")
    w.box(1.279, 1.48, -1.15, 0.024, 0.028, 0.36, "warning_orange")
    w.box(-1.279, 1.50, -2.20, 0.024, 0.034, 0.30, "tape_white")
    w.box(-1.279, 1.44, -2.20, 0.024, 0.026, 0.20, "label_blue")
    w.box(0.0, 0.018, 1.92, 1.20, 0.018, 0.26, "grime")
    w.write("ambulance_runtime_shell.obj")


def medical_cabinet() -> None:
    w = writer("ambulance_medical_cabinet")
    w.box(0.0, 0.0, 0.0, 1.70, 1.62, 0.52, "cabinet_off_white")
    for x in (-0.43, 0.43):
        w.box(x, 0.02, -0.30, 0.76, 1.48, 0.045, "soft_white")
        w.box(x + 0.25, 0.02, -0.34, 0.035, 0.92, 0.05, "stainless")
        w.box(x - 0.18, 0.44, -0.35, 0.22, 0.12, 0.035, "paper_label")
    for y in (-0.46, 0.0, 0.46):
        w.box(0.0, y, -0.35, 1.56, 0.035, 0.08, "stainless")
    w.box(0.0, -0.84, 0.02, 1.82, 0.12, 0.62, "dark_rubber")
    w.box(-0.62, 0.64, -0.35, 0.28, 0.08, 0.035, "strap_red")
    w.box(-0.62, 0.54, -0.35, 0.18, 0.06, 0.035, "screen_glass")
    for x in (-0.66, -0.24, 0.24, 0.66):
        w.box(x, 0.72, -0.355, 0.18, 0.055, 0.030, "tape_white")
        w.box(x, -0.70, -0.355, 0.16, 0.045, 0.030, "label_yellow")
    w.box(0.58, 0.58, -0.356, 0.22, 0.030, 0.028, "warning_orange")
    w.write("ambulance_medical_cabinet.obj")


def patient_stretcher() -> None:
    w = writer("ambulance_patient_stretcher_runtime")
    w.box(0.0, 0.08, 0.0, 0.78, 0.11, 1.86, "stainless")
    w.box(0.0, 0.23, -0.10, 0.70, 0.14, 1.56, "linen_off_white")
    w.box(0.0, 0.32, -0.26, 0.72, 0.12, 1.05, "blanket_desaturated_blue")
    w.box(0.0, 0.34, 0.67, 0.52, 0.11, 0.36, "linen_off_white")
    for z in (-0.72, -0.44, -0.16, 0.12):
        w.box(0.0, 0.39, z, 0.68, 0.018, 0.024, "abs_shadow")
    for z in (-0.42, 0.26):
        w.box(0.0, 0.43, z, 0.80, 0.045, 0.08, "dark_rubber")
    for z in (-0.55, -0.06, 0.42):
        w.box(0.0, 0.475, z, 0.86, 0.026, 0.050, "strap_red")
        w.box(-0.43, 0.488, z, 0.055, 0.034, 0.070, "painted_metal")
    w.box(0.32, 0.455, 0.72, 0.18, 0.025, 0.11, "paper_label")
    w.box(0.32, 0.473, 0.72, 0.12, 0.016, 0.022, "label_blue")
    for x in (-0.47, 0.47):
        w.box(x, 0.35, 0.0, 0.035, 0.12, 1.76, "painted_metal")
        w.box(x, 0.50, 0.0, 0.035, 0.035, 1.68, "painted_metal")
        for z in (-0.65, 0.65):
            w.box(x, -0.34, z, 0.055, 0.74, 0.055, "painted_metal")
            w.cylinder_y(x * 0.78, -0.76, z, 0.085, 0.050, "dark_rubber", 16)
    for z in (-0.78, 0.78):
        w.box(0.0, 0.26, z, 0.92, 0.045, 0.045, "painted_metal")
    w.write("ambulance_patient_stretcher_runtime.obj")


def equipment_rack() -> None:
    w = writer("ambulance_equipment_rack")
    for x in (-0.78, 0.78):
        for z in (-0.43, 0.43):
            w.box(x, 0.0, z, 0.06, 1.18, 0.06, "painted_metal")
    for y in (-0.48, 0.0, 0.48):
        w.box(0.0, y, 0.0, 1.65, 0.05, 0.88, "painted_metal")
    w.box(-0.34, 0.33, -0.18, 0.70, 0.38, 0.36, "soft_white")
    w.box(0.43, 0.30, -0.19, 0.46, 0.32, 0.32, "screen_glass")
    w.box(0.43, 0.30, -0.36, 0.34, 0.20, 0.035, "screen_glass")
    w.box(0.30, 0.34, -0.385, 0.16, 0.018, 0.018, "monitor_green")
    w.box(0.48, 0.26, -0.385, 0.12, 0.018, 0.018, "monitor_green")
    for x in (0.25, 0.40, 0.55):
        w.box(x, 0.08, -0.37, 0.045, 0.045, 0.035, "oxygen_green")
    for x in (-0.58, -0.44, -0.30):
        w.box(x, 0.05, -0.385, 0.040, 0.040, 0.028, "warning_orange")
    w.box(0.08, 0.14, -0.392, 0.72, 0.018, 0.018, "cable_black")
    w.box(-0.28, -0.04, -0.392, 0.018, 0.34, 0.018, "cable_black")
    w.box(-0.44, -0.20, -0.392, 0.34, 0.018, 0.018, "cable_black")
    w.box(-0.36, -0.26, -0.10, 0.58, 0.28, 0.42, "painted_metal")
    w.box(0.35, -0.30, -0.08, 0.48, 0.22, 0.36, "soft_white")
    w.cylinder_y(-0.05, -0.03, -0.43, 0.12, 0.035, "dark_rubber", 18)
    w.write("ambulance_equipment_rack.obj")


def oxygen_cylinders() -> None:
    w = writer("ambulance_oxygen_cylinders")
    for x in (-0.30, 0.30):
        w.cylinder_y(x, 0.0, 0.0, 0.19, 1.58, "oxygen_green", 24)
        w.box(x, 0.84, 0.0, 0.22, 0.10, 0.22, "painted_metal")
        w.box(x, 0.93, 0.0, 0.13, 0.08, 0.13, "painted_metal")
    for y in (-0.02, 0.42):
        w.box(0.0, y, -0.22, 0.96, 0.07, 0.06, "painted_metal")
        w.box(0.0, y, -0.29, 0.88, 0.05, 0.05, "dark_rubber")
    for x in (-0.30, 0.30):
        w.box(x, -0.32, -0.205, 0.24, 0.070, 0.025, "tape_white")
        w.box(x, 0.48, -0.205, 0.22, 0.055, 0.025, "label_blue")
    w.box(-0.30, 0.12, -0.205, 0.22, 0.18, 0.025, "paper_label")
    w.box(0.30, 0.12, -0.205, 0.22, 0.18, 0.025, "paper_label")
    w.box(0.0, 0.0, 0.25, 0.86, 1.70, 0.08, "painted_metal")
    w.box(0.0, -0.86, 0.0, 0.90, 0.10, 0.52, "dark_rubber")
    w.write("ambulance_oxygen_cylinders.obj")


def medical_bag() -> None:
    w = writer("ambulance_medical_bag")
    w.box(0.0, -0.12, 0.0, 1.55, 0.78, 0.84, "strap_red")
    w.box(0.0, 0.30, 0.0, 1.40, 0.12, 0.72, "strap_red")
    w.box(0.0, 0.45, -0.02, 0.80, 0.12, 0.14, "dark_rubber")
    w.box(0.0, -0.10, -0.46, 1.68, 0.08, 0.08, "dark_rubber")
    w.box(-0.38, -0.10, -0.50, 0.10, 0.86, 0.08, "dark_rubber")
    w.box(0.38, -0.10, -0.50, 0.10, 0.86, 0.08, "dark_rubber")
    w.box(0.0, -0.12, -0.91, 0.44, 0.10, 0.035, "soft_white")
    w.box(0.0, -0.12, -0.93, 0.10, 0.36, 0.035, "soft_white")
    w.box(-0.48, 0.02, -0.90, 0.16, 0.18, 0.035, "paper_label")
    w.box(0.52, 0.04, -0.90, 0.20, 0.16, 0.035, "label_yellow")
    w.box(0.00, 0.18, -0.91, 1.18, 0.040, 0.032, "dried_wear")
    w.box(-0.68, -0.40, -0.90, 0.18, 0.11, 0.035, "tape_white")
    w.write("ambulance_medical_bag.obj")


def iv_stand() -> None:
    w = writer("ambulance_iv_stand")
    w.cylinder_y(0.0, 0.0, 0.0, 0.025, 1.74, "painted_metal", 16)
    w.box(0.0, 0.90, 0.0, 0.55, 0.035, 0.035, "painted_metal")
    w.box(-0.26, 0.82, 0.0, 0.035, 0.18, 0.035, "painted_metal")
    w.box(0.26, 0.82, 0.0, 0.035, 0.18, 0.035, "painted_metal")
    w.box(-0.24, 0.54, 0.0, 0.18, 0.36, 0.035, "soft_white")
    w.box(-0.24, 0.34, 0.0, 0.035, 0.20, 0.025, "painted_metal")
    w.box(-0.24, 0.16, 0.0, 0.018, 0.40, 0.018, "cable_black")
    w.box(-0.10, -0.03, 0.0, 0.30, 0.016, 0.018, "cable_black")
    for angle in (0.0, math.tau / 3.0, 2.0 * math.tau / 3.0):
        x = math.cos(angle) * 0.23
        z = math.sin(angle) * 0.23
        w.box(x * 0.5, -0.88, z * 0.5, abs(x) + 0.05, 0.045, abs(z) + 0.05, "painted_metal")
    for angle in (0.0, math.tau / 3.0, 2.0 * math.tau / 3.0):
        w.cylinder_y(math.cos(angle) * 0.23, -0.91, math.sin(angle) * 0.23, 0.055, 0.05, "dark_rubber", 12)
    w.write("ambulance_iv_stand.obj")


def main() -> None:
    write_materials()
    runtime_shell()
    patient_stretcher()
    medical_cabinet()
    equipment_rack()
    oxygen_cylinders()
    medical_bag()
    iv_stand()


if __name__ == "__main__":
    main()
