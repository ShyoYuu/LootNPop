"""1-B5e 같은 계열 변형 반입본을 읽기 전용으로 검사한다(로드·의존성·재질·Nanite·충돌 형상).

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/InspectRidgeVariantImport.py"
에셋을 저장하거나 고치지 않는다. 결과는 Saved 아래 LoadInspection.json으로 남긴다.
비교 기준은 이미 쓰고 있는 Sandstone_01·Moss_06·Clover_01·Fern_01이다.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeVariantImport_20261117"

SAND = "/Game/Megascans/3D_Assets/SandstoneBoulder/SM_SandstoneBoulder_%02d"
MOSS = "/Game/Megascans/3D_Assets/MossyForestBoulder/SM_MossyForestBoulder_%02d"
CLOVER = "/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_%02d"
FERN = "/Game/Megascans/3D_Plants/Fern/SM_Fern_%02d"

# (계열, 기존 기준, 새 변형)
FAMILIES = [
    ("Sandstone", [SAND % 1], [SAND % i for i in range(2, 6)]),
    ("Moss", [MOSS % 6], [MOSS % i for i in range(1, 6)]),
    ("Clover", [CLOVER % 1], [CLOVER % i for i in range(2, 6)]),
    ("Fern", [FERN % 1], [FERN % i for i in range(2, 7)]),
]


def safe(fn, default=None):
    try:
        return fn()
    except Exception as e:  # 검사 항목 하나의 실패가 전체 검사를 멈추지 않게 한다.
        return "ERROR: %s" % e


def material_info(material):
    if material is None:
        return None
    base = safe(lambda: material.get_base_material())
    info = {"path": material.get_path_name(), "class": material.get_class().get_name()}
    if isinstance(base, unreal.Material):
        info["base"] = base.get_path_name()
        info["used_with_ism"] = safe(lambda: base.get_editor_property("used_with_instanced_static_meshes"))
        info["used_with_nanite"] = safe(lambda: base.get_editor_property("used_with_nanite"))
    else:
        info["base"] = base
    return info


def missing_dependencies(registry, package):
    """하드 의존성 가운데 디스크/레지스트리에 없는 /Game 패키지."""
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False,
                                                    include_hard_package_references=True)
    deps = registry.get_dependencies(package, options) or []
    missing = []
    for dep in deps:
        name = str(dep)
        if name.startswith("/Game/") and not unreal.EditorAssetLibrary.does_directory_exist(name) \
                and not registry.get_assets_by_package_name(name):
            missing.append(name)
    return [str(d) for d in deps], missing


def mesh_info(registry, path):
    package = path
    row = {"path": path, "in_registry": bool(registry.get_assets_by_package_name(package))}
    mesh = unreal.load_asset(path)
    row["loaded"] = mesh is not None
    if mesh is None:
        return row
    deps, missing = missing_dependencies(registry, package)
    row["dependencies"] = deps
    row["missing_dependencies"] = missing
    nanite = safe(lambda: mesh.get_editor_property("nanite_settings"))
    row["nanite"] = safe(lambda: nanite.get_editor_property("enabled"))
    row["lods"] = safe(lambda: mesh.get_num_lods())
    row["triangles_lod0"] = safe(lambda: mesh.get_num_triangles(0))
    box = safe(lambda: mesh.get_bounding_box())
    if isinstance(box, unreal.Box):
        size = box.max - box.min
        row["bounds_size_cm"] = [round(size.x, 1), round(size.y, 1), round(size.z, 1)]
        row["bounds_min_z_cm"] = round(box.min.z, 1)
    materials = safe(lambda: mesh.get_editor_property("static_materials")) or []
    if isinstance(materials, str):
        row["materials"] = materials
    else:
        row["materials"] = [dict(slot=str(m.get_editor_property("material_slot_name")),
                                 material=material_info(m.get_editor_property("material_interface")))
                            for m in materials]
    body = safe(lambda: mesh.get_editor_property("body_setup"))
    if isinstance(body, unreal.BodySetup):
        geom = body.get_editor_property("agg_geom")
        row["collision"] = {
            "trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "convex": len(geom.get_editor_property("convex_elems")),
            "box": len(geom.get_editor_property("box_elems")),
            "sphere": len(geom.get_editor_property("sphere_elems")),
            "sphyl": len(geom.get_editor_property("sphyl_elems")),
            "profile": str(body.get_editor_property("default_instance").get_editor_property("collision_profile_name")),
            "phys_material": safe(lambda: (body.get_editor_property("phys_material") or None) and
                                  body.get_editor_property("phys_material").get_path_name()),
        }
    else:
        row["collision"] = body
    row["lod_for_collision"] = safe(lambda: mesh.get_editor_property("lod_for_collision"))
    return row


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    dirty = lambda: sorted(p.get_path_name() for p in
                           list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                           list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    result = {"world": world.get_path_name() if world else None,
              "pie": level_editor.is_in_play_in_editor(), "dirty_before": dirty()}
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    families = []
    for name, baseline, variants in FAMILIES:
        families.append({"family": name,
                         "baseline": [mesh_info(registry, p) for p in baseline],
                         "variants": [mesh_info(registry, p) for p in variants]})
    result["families"] = families
    result["dirty_after"] = dirty()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "LoadInspection.json").write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("Ridge variant import inspection written: %s" % (OUT / "LoadInspection.json"))


main()
