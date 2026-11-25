"""U5a-4 실제 보존·반입 보행 정점·두 재질을 읽기 전용으로 검사한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeStoneRefinement.py" verify V005
배치·저장·충돌·베이크를 실행하지 않는다.
"""
import itertools
import math
from pathlib import Path
import struct
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[2] / 'Scripts')]
import RidgeClosedBodyTrial as core
import AssembleRidgeOctant as assembly


def verify():
    # 에디터 Python 모듈 캐시에 남은 이전 판의 CONFIG를 사용하지 않는다.
    core.CONFIG = HERE / 'ClosedBodyTrial_V001/Generation_V005.json'
    config, _ = core.inputs()
    before = set(core.OUT.glob('Verify_*.json'))
    core.verify()
    created = set(core.OUT.glob('Verify_*.json')) - before
    assert len(created) == 1, 'Full verification did not finish; retry after level load'
    report_path = created.pop()
    report = core.read(report_path)
    assert report['preservation_pass'] and not report['dirty'], report['issues']

    row = config['new_outputs'][0]
    mesh = unreal.load_asset(row['asset'])
    assert mesh and mesh.get_num_triangles(0) == 6288
    materials = {}
    for slot, expected in row['material_assignments'].items():
        index = mesh.get_material_index(slot)
        assert index >= 0, slot
        actual = core.layout.test.package(mesh.get_material(index))
        assert actual == expected, (slot, actual)
        materials[slot] = actual

    _, positions = assembly.native_mesh(mesh)
    cell = .05
    lookup = {}
    for p in positions:
        point = (p.x, p.y, p.z)
        key = tuple(math.floor(v / cell) for v in point)
        lookup.setdefault(key, []).append(point)
    source = core.ROOT / 'Saved/Profiling/Graphics/Phase01/U5a_3_V001/Geometry_V011/SM_Ridge_StairRamp3_Connection_V011.bin'
    binary = source.read_bytes()
    count, _ = struct.unpack_from('<ii', binary)
    vertices = list(struct.iter_unpack('<fff', binary[8:8 + count * 12]))
    assert count == 286 * 7
    errors = []
    for start in range(0, count, 7):
        for index in (1, 2, 3):
            point = vertices[start + index]
            key = tuple(math.floor(v / cell) for v in point)
            candidates = [candidate for delta in itertools.product((-1, 0, 1), repeat=3)
                          for candidate in lookup.get(tuple(a + b for a, b in zip(key, delta)), [])]
            assert candidates, ('Missing walking vertex', start, index)
            error = min(math.dist(point, candidate) for candidate in candidates)
            assert error < cell, ('Walking vertex moved', start, index, error)
            errors.append(error)

    bounds = [[f(getattr(p, axis) for p in positions) for axis in ('x', 'y', 'z')]
              for f in (min, max)]
    bounds_error = max(abs(a - b) for actual, expected in zip(bounds, row['bounds_cm'])
                       for a, b in zip(actual, expected))
    assert bounds_error < .05, bounds_error
    assert not core.layout.test.dirty()
    core.evidence('StoneVerify', {
        'preservation_pass': True, 'level': config['target_level'],
        'full_verify': report_path.relative_to(core.ROOT).as_posix(),
        'walking_surface_vertex_count': len(errors), 'walking_surface_max_error_cm': max(errors),
        'walking_surface_tolerance_cm': cell, 'native_vertex_count': len(positions),
        'bounds_cm': bounds, 'bounds_error_cm': bounds_error, 'materials': materials,
        'triangles': mesh.get_num_triangles(0), 'dirty': core.layout.test.dirty(),
        'protected_pass': False, 'collision_or_PIE_pass': False, 'accepted': False,
        'status': 'PRESERVATION_AND_IMPORTED_WALK_PASS; visual review pending; legacy ramp overlap REVIEW'})
    unreal.log('RIDGE_STONE_VERIFY PRESERVATION_AND_IMPORTED_WALK_PASS')


if __name__ == '__main__':
    assert sys.argv[1:] == ['verify', 'V005'], 'Use verify V005 only'
    verify()
