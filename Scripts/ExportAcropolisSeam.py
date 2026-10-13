"""새 옥탄트 전용 프로세스에서 기준 지각만 읽어 FBX로 내보낸다."""

import json
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir()).resolve()
output = root / "Saved/SliceOctant/Acropolis_00/V001"
output.mkdir(parents=True, exist_ok=True)
source = unreal.load_asset("/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000")
if not source:
    raise RuntimeError("Missing Meadow seam source")
task = unreal.AssetExportTask()
task.object = source
task.filename = str(output / "MeadowSeam.fbx")
task.automated = True
task.prompt = False
task.replace_identical = False
task.exporter = unreal.StaticMeshExporterFBX()
options = unreal.FbxExportOption()
options.collision = False
options.level_of_detail = False
task.options = options
if not unreal.Exporter.run_asset_export_task(task):
    raise RuntimeError("Failed to export seam source")
(output / "export.json").write_text(json.dumps({"source": source.get_path_name(),
    "file": task.filename}, indent=2), encoding="utf-8")
unreal.log("ACROPOLIS_SEAM_EXPORT_SUCCESS")
