"""저장된 Pyramid LVI를 별도 에디터 프로세스에서 베이크한다."""
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
namespace={"__name__":"pyramid_assembly_helpers"}
exec(compile((root/"Scripts/AssemblePyramidOctant.py").read_text(encoding="utf-8"),
             "AssemblePyramidOctant.py","exec"),namespace)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(namespace["LEVEL"])
exterior=unreal.load_class(None,'/Game/Maps/Pyramid_00/BP_PyramidExteriorPanel.BP_PyramidExteriorPanel_C')
scale=unreal.get_default_object(exterior).get_editor_property('panel_mesh').get_editor_property('relative_scale3d')
assert abs(scale.x-24)<0.001 and abs(scale.y-24)<0.001,('Exterior panel defaults did not persist',scale)
unreal.log('PYRAMID_EXTERIOR_PANEL_SCALE '+str(scale))
namespace["bake"]()
namespace['audit_final_assets']()
unreal.log("PYRAMID_READY_FOR_VALIDATION")
