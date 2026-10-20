# 애니메이션 개선 2-4: CHT_PoseSearchDatabases_UEFN 행 정의 → (1) 결과 목록 파일, (2) MCP set_properties용 컬럼 JSON.
# GASP CHT_PoseSearchDatabases_Relaxed 중첩 테이블(Stand Idles·Walks·Runs·Sprint, InAir)을 평면 멀티 결과 표로 펼친다.
# 행 순서·조건 출처: .context/AnimationOverhaul/design/GASPReference.md §2.11. 시스템 python으로 실행한다(에디터 불필요).
#   python Scripts/AnimOverhaul/BuildLocomotionChooser_2-4.py <출력 폴더>
import json, os, sys

MM = '/Game/Characters/MotionMatchingData/'
DIRS = ['F', 'B', 'L', 'R']                      # ELNPMovementDirection 값 0~3
GASP_DIR = {'F': 'F', 'B': 'B', 'L': 'LL', 'R': 'RL'}
BIT = {'F': 1, 'B': 2, 'L': 4, 'R': 8}          # MultiEnum 마스크(값 i → 1 << i)
ALL_DIRS = 15
NOT = {d: ALL_DIRS & ~BIT[d] for d in DIRS}       # "최근 방향이 이 방향이 아님"(GASP 피벗·스핀 마스크)
GAIT_BIT = {'Walk': 1, 'Run': 2, 'Sprint': 4}

rows = []


def row(db, **c):
    rows.append((db, c))


NOLAND = dict(land_l=False, land_h=False)

# --- 지상·Idle (GASP Stand Idles) ---
row('PSD_Stand_Idles_UEFN',            mode='OnGround', state='Idle', speed=(None, 20), tip=False, **NOLAND)
row('PSD_Stand_Walk_Stops_UEFN',       mode='OnGround', state='Idle', speed=(20, 170), **NOLAND)
row('PSD_Stand_Run_Stops_UEFN',        mode='OnGround', state='Idle', speed=(170, 400), **NOLAND)
row('PSD_Stand_Sprint_Stops_UEFN',     mode='OnGround', state='Idle', speed=(400, None), **NOLAND)
row('PSD_Stand_TurnInPlace_UEFN',      mode='OnGround', state='Idle', tip=True, **NOLAND)
row('PSD_Stand_Idle_Lands_Light_UEFN', mode='OnGround', state='Idle', land_l=True, land_h=False)
row('PSD_Stand_Idle_Lands_Heavy_UEFN', mode='OnGround', state='Idle', land_l=False, land_h=True)

# --- 지상·Moving·Walk/Run (GASP Stand Walks / Stand Runs) ---
for gait in ('Walk', 'Run'):
    for d in DIRS:
        row(f'PSD_Stand_{gait}_{GASP_DIR[d]}_Starts_UEFN', mode='OnGround', state='Moving', gait=gait, dir=d, starting=True, **NOLAND)
    if gait == 'Run':
        row('PSD_Stand_Run_F_GaitTransitions_UEFN', mode='OnGround', state='Moving', gait=gait, dir='F',
            gait_last=GAIT_BIT['Walk'] | GAIT_BIT['Sprint'], mode_last='OnGround', **NOLAND)
    for d in DIRS:
        row(f'PSD_Stand_{gait}_{GASP_DIR[d]}_Loops_UEFN', mode='OnGround', state='Moving', gait=gait, dir=d, mode_last='OnGround', **NOLAND)
    row(f'PSD_Stand_{gait}_F_Turns_UEFN', mode='OnGround', state='Moving', gait=gait, dir='F', dir_recent=BIT['F'], pivoting=True, **NOLAND)
    for d in DIRS:
        # GASP: Walk F_Pivots만 최근 방향 마스크가 전부(63)다. 나머지는 "최근 방향 ≠ 지금 방향 계열"
        mask = ALL_DIRS if (gait == 'Walk' and d == 'F') else NOT[d]
        row(f'PSD_Stand_{gait}_{GASP_DIR[d]}_Pivots_UEFN', mode='OnGround', state='Moving', gait=gait, dir=d, dir_recent=mask, pivoting=True, **NOLAND)
    for d in DIRS:
        # GASP: Walk F 스핀과 Run 스핀 4개는 착지 조건이 없다
        land = {} if (gait == 'Run' or d == 'F') else NOLAND
        row(f'PSD_Stand_{gait}_{GASP_DIR[d]}_Spins_UEFN', mode='OnGround', state='Moving', gait=gait, dir=d, dir_recent=NOT[d], spin=True, **land)
    row(f'PSD_Stand_{gait}_Lands_Light_UEFN', mode='OnGround', state='Moving', gait=gait, land_l=True, land_h=False)
    row(f'PSD_Stand_{gait}_Lands_Heavy_UEFN', mode='OnGround', state='Moving', gait=gait, land_l=False, land_h=True)

# --- 지상·Moving·Sprint (GASP Stand Sprint, 방향은 늘 F) ---
row('PSD_Stand_Sprint_Starts_UEFN',      mode='OnGround', state='Moving', gait='Sprint', starting=True)
row('PSD_Stand_Sprint_Loops_UEFN',       mode='OnGround', state='Moving', gait='Sprint', pivoting=False, mode_last='OnGround')
row('PSD_Stand_Sprint_Turns_UEFN',       mode='OnGround', state='Moving', gait='Sprint', pivoting=True)
row('PSD_Stand_Sprint_Lands_Light_UEFN', mode='OnGround', state='Moving', gait='Sprint', land_l=True, land_h=False)
row('PSD_Stand_Sprint_Lands_Heavy_UEFN', mode='OnGround', state='Moving', gait='Sprint', land_l=False, land_h=True)

# --- 공중 (AN-007: MM 출력은 Fall_Loop로 덮이지만 노드가 비지 않게 기존 DB를 둔다) ---
row('PSD_Jump_UEFN', mode='InAir')

N = len(rows)


def enum_col(enum, prop, display, key, names):
    vals = []
    for _, c in rows:
        v = c.get(key)
        if v is None:
            vals.append({'valueName': f'{enum}::{names[0]}', 'comparison': 'MatchAny', 'value': 0})
        else:
            vals.append({'valueName': f'{enum}::{v}', 'comparison': 'MatchEqual', 'value': names.index(v)})
    return {'_structType': '/Script/Chooser.EnumColumn',
            'inputValue': {'_structType': '/Script/Chooser.EnumContextProperty',
                           'binding': {'enum': {'refPath': f'/Script/LootNPop.{enum}'}, 'propertyBindingChain': [prop],
                                       'contextIndex': 0, 'isBoundToRoot': False, 'displayName': display}},
            'defaultRowValue': {'valueName': 'None', 'comparison': 'MatchEqual', 'value': 0},
            'rowValues': vals, 'bDisabled': False, 'editorColumnWidth': 200}


def multi_enum_col(enum, prop, display, key):
    return {'_structType': '/Script/Chooser.MultiEnumColumn',
            'inputValue': {'_structType': '/Script/Chooser.EnumContextProperty',
                           'binding': {'enum': {'refPath': f'/Script/LootNPop.{enum}'}, 'propertyBindingChain': [prop],
                                       'contextIndex': 0, 'isBoundToRoot': False, 'displayName': display}},
            'defaultRowValue': {'value': 0},
            'rowValues': [{'value': c.get(key, 0)} for _, c in rows], 'bDisabled': False, 'editorColumnWidth': 200}


def bool_col(prop, display, key):
    vals = []
    for _, c in rows:
        v = c.get(key)
        vals.append('MatchAny' if v is None else ('MatchTrue' if v else 'MatchFalse'))
    return {'_structType': '/Script/Chooser.BoolColumn',
            'inputValue': {'_structType': '/Script/Chooser.BoolContextProperty',
                           'binding': {'propertyBindingChain': [prop], 'contextIndex': 0, 'isBoundToRoot': False, 'displayName': display}},
            'defaultRowValue': 'MatchAny', 'rowValuesWithAny': vals, 'bDisabled': False, 'editorColumnWidth': 200}


def float_col(prop, display, key):
    vals = []
    for _, c in rows:
        lo, hi = c.get(key, (None, None))
        vals.append({'min': lo or 0.0, 'max': hi or 0.0, 'bNoMin': lo is None, 'bNoMax': hi is None})
    return {'_structType': '/Script/Chooser.FloatRangeColumn',
            'inputValue': {'_structType': '/Script/Chooser.FloatContextProperty',
                           'binding': {'propertyBindingChain': [prop], 'contextIndex': 0, 'isBoundToRoot': False, 'displayName': display}},
            'bWrapInput': False, 'minValue': 0.0, 'maxValue': 2000.0,
            'defaultRowValue': {'min': 0.0, 'max': 0.0, 'bNoMin': True, 'bNoMax': True},
            'rowValues': vals, 'bDisabled': False, 'editorColumnWidth': 200}


columns = [
    enum_col('ELNPMovementMode', 'MovementMode', 'Movement Mode', 'mode', ['OnGround', 'InAir']),
    enum_col('ELNPMovementState', 'MovementState', 'Movement State', 'state', ['Idle', 'Moving']),
    enum_col('ELNPGait', 'Gait', 'Gait', 'gait', ['Walk', 'Run', 'Sprint']),
    enum_col('ELNPMovementDirection', 'MovementDirection', 'Movement Direction', 'dir', DIRS),
    multi_enum_col('ELNPMovementDirection', 'MovementDirection_Recent', 'Movement Direction Recent', 'dir_recent'),
    multi_enum_col('ELNPGait', 'Gait_LastFrame', 'Gait Last Frame', 'gait_last'),
    enum_col('ELNPMovementMode', 'MovementMode_LastFrame', 'Movement Mode Last Frame', 'mode_last', ['OnGround', 'InAir']),
    float_col('GroundSpeed', 'Ground Speed', 'speed'),
    bool_col('bIsStarting', 'Is Starting', 'starting'),
    bool_col('bIsPivoting', 'Is Pivoting', 'pivoting'),
    bool_col('bShouldSpinTransition', 'Should Spin Transition', 'spin'),
    bool_col('bShouldTurnInPlace', 'Should Turn In Place', 'tip'),
    bool_col('bJustLanded_Light', 'Just Landed Light', 'land_l'),
    bool_col('bJustLanded_Heavy', 'Just Landed Heavy', 'land_h'),
]

out = sys.argv[1] if len(sys.argv) > 1 else '.'
with open(os.path.join(out, 'chooser_2-4_results.txt'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(MM + db for db, _ in rows) + '\n')
with open(os.path.join(out, 'chooser_2-4_columns.json'), 'w', encoding='utf-8') as f:
    json.dump({'columnsStructs': columns}, f)
with open(os.path.join(out, 'chooser_2-4_rows.md'), 'w', encoding='utf-8') as f:
    for i, (db, c) in enumerate(rows):
        f.write(f'{i:2d} {db} {c}\n')
print(N, 'rows')
