"""기존 지각을 읽기만 하여 Grove 전용 원본과 복구 기준선을 저장한다."""
import hashlib
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root / "Saved/SliceOctant/Grove_00/Baseline"
out.mkdir(parents=True, exist_ok=True)
source = unreal.load_asset("/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000")
task = unreal.AssetExportTask()
task.object = source
task.filename = str(out / "MeadowSeam.fbx")
task.automated = True
task.prompt = False
task.replace_identical = False
task.exporter = unreal.StaticMeshExporterFBX()
options = unreal.FbxExportOption()
options.collision = False
options.level_of_detail = False
task.options = options
assert not Path(task.filename).exists(), "Baseline already exists"
assert unreal.Exporter.run_asset_export_task(task)
files = [p for folder in ("Meadow_00", "Bastion_00", "Hollow_00", "Acropolis_00", "Pyramid_00")
         for p in (root / "Content/Maps" / folder).rglob("*") if p.is_file()]
files += [root / "Content/Maps/OctantPoolData.uasset"]
hashes = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
shutil.copy2(root / "Content/Maps/OctantPoolData.uasset", out / "OctantPoolData.uasset")
(out / "existing-assets.json").write_text(json.dumps(hashes, indent=2), encoding="utf-8")
unreal.log("GROVE_SEAM_EXPORT_SUCCESS")
