import math
import os

import unreal


OBJ_PATH = "/tmp/stargame_milky_way_sky_sphere.obj"
MESH_DIR = "/Game/Meshes/Sky"
MESH_NAME = "SM_MilkyWaySkySphere"
MESH_PATH = f"{MESH_DIR}/{MESH_NAME}"
SEGMENTS = 192
RINGS = 96
RADIUS = 100.0


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def write_obj():
    with open(OBJ_PATH, "w", encoding="utf-8") as obj:
        obj.write("# Stargame Milky Way lat-long sky sphere\n")
        for ring in range(RINGS + 1):
            v = ring / RINGS
            theta = v * math.pi
            sin_theta = math.sin(theta)
            cos_theta = math.cos(theta)
            for segment in range(SEGMENTS + 1):
                u = segment / SEGMENTS
                phi = u * math.tau
                x = RADIUS * sin_theta * math.cos(phi)
                y = RADIUS * sin_theta * math.sin(phi)
                z = RADIUS * cos_theta
                obj.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
        for ring in range(RINGS + 1):
            v = ring / RINGS
            for segment in range(SEGMENTS + 1):
                u = segment / SEGMENTS
                obj.write(f"vt {u:.8f} {1.0 - v:.8f}\n")
        for ring in range(RINGS + 1):
            theta = (ring / RINGS) * math.pi
            sin_theta = math.sin(theta)
            cos_theta = math.cos(theta)
            for segment in range(SEGMENTS + 1):
                phi = (segment / SEGMENTS) * math.tau
                nx = sin_theta * math.cos(phi)
                ny = sin_theta * math.sin(phi)
                nz = cos_theta
                obj.write(f"vn {nx:.6f} {ny:.6f} {nz:.6f}\n")
        for ring in range(RINGS):
            for segment in range(SEGMENTS):
                a = ring * (SEGMENTS + 1) + segment + 1
                b = a + 1
                c = a + (SEGMENTS + 1)
                d = c + 1
                # Reverse winding so the primary face orientation points inward.
                obj.write(f"f {a}/{a}/{a} {c}/{c}/{c} {b}/{b}/{b}\n")
                obj.write(f"f {b}/{b}/{b} {c}/{c}/{c} {d}/{d}/{d}\n")


def import_mesh():
    ensure_directory(MESH_DIR)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", OBJ_PATH)
    task.set_editor_property("destination_path", MESH_DIR)
    task.set_editor_property("destination_name", MESH_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not mesh:
        raise RuntimeError(f"Failed to import/load {MESH_PATH}")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def main():
    write_obj()
    import_mesh()
    print(f"Milky Way sky sphere mesh ready: {MESH_PATH}")


if __name__ == "__main__":
    main()
