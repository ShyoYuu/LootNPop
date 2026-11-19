"""Phase 1-BR 지형 후보와 Migrate 의존성을 UE 에디터에서 읽기 전용으로 조사한다."""
import gc
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
OUT = HERE.parents[2] / "Saved/Profiling/Graphics/Phase01/EDTerrain_V001"
CATALOG = HERE / "EDTerrainCatalog_V001.json"
MEGASCANS = "/Game/Megascans/3D_Assets/"
FAMILIES = {
    "HugeSandstoneCliff": ("cliff", 8),
    "MassiveSandstoneCliff": ("cliff", 6),
    "CanyonSandstoneRidge": ("ridge", 4),
    "MassiveCanyonSandstoneMesa": ("ridge", 1),
    "GiganticSandstoneTerrain": ("ridge", 1),
    "GrassyEmbankment": ("bank", 2),
    "SandstoneRockyGround": ("bank", 5),
    "ForestRockFormation": ("spire", 6),
    "NordicBoulder": ("spire", 2),
}
GENERATED_BANKS = (
    "/Game/Meshes/_GENERATED/RockFormation/SM_GrassyEmbankment_01",
    "/Game/Meshes/_GENERATED/SM_RiverbedEmbankment_01",
)
# ElectricDreams_PCG의 기준 구도에서 읽은 컴포넌트 재질 덮어쓰기다.
SCENE_OVERRIDES = {
    **{f"/Game/SmartAssets/AssetMeshes/SM_Cliff_{i:02d}": f"/Game/SmartAssets/AssetMaterials/MI_Cliff_{i:02d}"
       for i in (2, 5)},
    MEGASCANS + "HugeSandstoneCliff/SM_HugeSandstoneCliff_07": MEGASCANS + "HugeSandstoneCliff/MI_HugeSandstoneCliff_07",
}


def xyz(vector):
    return [round(getattr(vector, key), 3) for key in ("x", "y", "z")]


def package(asset):
    return asset.get_path_name().split(".")[0]


def inspect_mesh(path, role, mesh_editor):
    mesh = unreal.load_asset(path)
    assert isinstance(mesh, unreal.StaticMesh), path
    bounds = mesh.get_bounding_box()
    settings = mesh.get_editor_property("nanite_settings")
    nanite = settings.get_editor_property("enabled")
    body = mesh.get_editor_property("body_setup")
    slots = sorted({mesh_editor.get_lod_material_slot(mesh, 0, i) for i in range(mesh.get_num_sections(0))})
    assert slots and 0 <= slots[0], path
    materials = []
    for slot in slots:
        default = mesh.get_material(slot)
        override = SCENE_OVERRIDES.get(path) if slot == 0 else None
        effective = unreal.load_asset(override) if override else default
        assert isinstance(effective, unreal.MaterialInterface), (path, slot, override)
        materials.append({"slot": slot, "asset_material": package(default) if default else None,
                          "scene_override": override, "effective_material": package(effective),
                          "base_material": package(effective.get_base_material())})
    lod0 = mesh.get_num_triangles(0)  # UStaticMesh는 RenderData의 LODResources를 읽는다.
    assert lod0 > 0, path
    complex_mesh = mesh.get_editor_property("complex_collision_mesh")
    return {"mesh": path, "role": role, "bounds_min_cm": xyz(bounds.min), "bounds_max_cm": xyz(bounds.max),
            "size_cm": xyz(bounds.max - bounds.min), "nanite": nanite, "render_lod0_triangles": lod0,
            "nanite_fallback_lod0_triangles": lod0 if nanite else None, "materials": materials,
            "collision": {"trace_flag": str(body.get_editor_property("collision_trace_flag")),
                          "profile": str(body.get_editor_property("default_instance").get_editor_property("collision_profile_name")),
                          "simple_shapes": mesh_editor.get_simple_collision_count(mesh),
                          "complex_mesh": package(complex_mesh) if complex_mesh else None,
                          "lod_for_collision": mesh.get_editor_property("lod_for_collision")}}


def inspect(candidates):
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    rows = []
    for i, (path, role) in enumerate(candidates, 1):
        rows.append(inspect_mesh(path, role, mesh_editor))
        if i % 6 == 0:
            gc.collect()
            unreal.SystemLibrary.collect_garbage()
    return rows


def candidate_paths():
    for name, (role, count) in FAMILIES.items():
        for i in range(1, count + 1):
            yield f"{MEGASCANS}{name}/SM_{name}_{i:02d}", role
    for i in range(2, 20):
        yield f"/Game/SmartAssets/AssetMeshes/SM_Cliff_{i:02d}", "cliff"
    for path in GENERATED_BANKS:
        yield path, "bank"


def dependencies(roots):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                                    include_soft_package_references=True)
    pending = list(roots)
    packages, external = {}, set()
    while pending:
        path = pending.pop()
        if not path.startswith("/Game/"):
            external.add(path)
            continue
        if path in packages:
            continue
        data = registry.get_assets_by_package_name(path)
        packages[path] = str(data[0].asset_class_path.asset_name) if data else None
        pending.extend(map(str, registry.get_dependencies(path, options) or []))
    return {"roots": sorted(roots), "game_packages": dict(sorted(packages.items())),
            "external_dependencies": sorted(external)}


def dirty_packages():
    saving = unreal.EditorLoadingAndSavingUtils
    return sorted(p.get_path_name() for p in list(saving.get_dirty_content_packages()) + list(saving.get_dirty_map_packages()))


def run(mode):
    project = Path(unreal.Paths.project_dir()).resolve().name
    assert project == ("LootNPop" if mode == "target" else "ElectricDreamsEnv"), project
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    before = dirty_packages()
    if mode == "source":
        result = {"meshes": inspect(candidate_paths())}
    else:
        selection = json.loads(CATALOG.read_text(encoding="utf-8"))["selection"]
        if mode == "dependencies":
            result = dependencies(selection["roots"])
        else:
            dependency_report = dependencies(selection["roots"])
            missing = [path for path, kind in dependency_report["game_packages"].items() if kind is None]
            assert not missing, missing
            riverbed = unreal.load_asset(selection["riverbed_material"])
            assert isinstance(riverbed, unreal.MaterialInterface), selection["riverbed_material"]
            result = {"meshes": inspect((row["mesh"], row["role"]) for row in selection["meshes"]),
                      "riverbed_material": package(riverbed), **dependency_report}
    after = dirty_packages()
    result.update(project=project, engine_version=unreal.SystemLibrary.get_engine_version(), dirty_before=before, dirty_after=after)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{mode}.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    assert after == before, (before, after)
    unreal.log("RIDGE_ED_TERRAIN_INSPECTION_READY " + mode)


if __name__ == "__main__":
    assert len(sys.argv) == 2 and sys.argv[1] in {"source", "dependencies", "target"}
    run(sys.argv[1])
