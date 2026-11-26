"""U5a-6 시작 전 에디터 상태를 파일로 기록한다. 에셋/맵 저장·수정은 하지 않는다."""
import datetime
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
dirty = [p.get_path_name() for p in
         unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() +
         unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
data = {'time': datetime.datetime.now().isoformat(), 'world': world.get_path_name(),
        'pie': unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),
        'all_dirty_packages': dirty}
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'InitialEditorState.json').write_text(json.dumps(data, indent=1) + '\n', encoding='utf-8')
unreal.log('U5A6_INITIAL_EDITOR_STATE ' + json.dumps(data))
