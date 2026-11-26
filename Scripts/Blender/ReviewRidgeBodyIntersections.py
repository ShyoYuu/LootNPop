"""U5a-6 채택 V006의 보존 파일과 삼각형 교차를 읽기 전용으로 분류한다.

Blender --background --factory-startup --python <파일>
면 번호는 저장된 V005 bin의 0 기반 삼각형 번호다. 기존 생성기는 실행하지 않는다.
"""
import collections
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
AUTHOR = ROOT / 'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
SOURCE = ROOT / 'Saved/Profiling/Graphics/Phase01/U5a_5_V001'
OUT = ROOT / 'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
EPS = 1e-7  # cm, float64 판정 오차와 UE 반입 오차는 구별한다.


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')


def preservation():
    frozen = read(AUTHOR / 'FrozenInputs_V006.json')['files']
    handoff = read(SOURCE / 'AcceptedHandoffHashes_V001.json')
    assert len(frozen) == 412 and len(handoff) == 43
    merged = {}
    for row in frozen + handoff:
        key = row['path'].replace('\\', '/').casefold()
        if key in merged:
            assert merged[key]['sha256'] == row['sha256'], row['path']
        else:
            merged[key] = {'path': row['path'], 'sha256': row['sha256']}
    assert len(merged) == 417
    issues = []
    for row in merged.values():
        p = ROOT / row['path']
        if not p.is_file() or sha(p) != row['sha256']:
            issues.append(row['path'])
    backup_issues = [r['backup'] for r in handoff
                     if not (ROOT / r['backup']).is_file() or sha(ROOT / r['backup']) != r['sha256']]
    report = {'frozen': 412, 'handoff': 43, 'unique': 417, 'issues': issues,
              'handoff_backup_issues': backup_issues, 'pass': not issues and not backup_issues}
    write('PreservationAudit.json', report)
    assert report['pass'], report
    write('PreservationHashes.json', sorted(merged.values(), key=lambda r: r['path'].casefold()))
    print('PRESERVATION ' + json.dumps(report), flush=True)


def binary(path):
    data = path.read_bytes()
    nv, nt = struct.unpack_from('<ii', data)
    v = np.frombuffer(data, '<f4', nv * 3, 8).reshape(-1, 3).astype(np.float64)
    t = np.frombuffer(data, '<i4', nt * 3, 8 + nv * 12).reshape(-1, 3)
    assert len(data) == 8 + nv * 12 + nt * 12
    return v, t


def section(triangle, distances):
    points = [p for p, d in zip(triangle, distances) if abs(d) <= EPS]
    for i, j in ((0, 1), (1, 2), (2, 0)):
        if distances[i] * distances[j] < 0:
            points.append(triangle[i] + (triangle[j] - triangle[i]) * distances[i] / (distances[i] - distances[j]))
    return np.asarray(points)


def barycentric(p, tri):
    e, f = tri[1] - tri[0], tri[2] - tri[0]
    q = p - tri[0]
    ee, ef, ff = np.dot(e, e), np.dot(e, f), np.dot(f, f)
    denom = ee * ff - ef * ef
    u = (ff * np.dot(q, e) - ef * np.dot(q, f)) / denom
    w = (ee * np.dot(q, f) - ef * np.dot(q, e)) / denom
    return np.asarray([1 - u - w, u, w])


def coplanar_area(a, b, normal):
    axis = int(np.argmax(np.abs(normal)))
    polygon = np.delete(a, axis, axis=1)
    clip = np.delete(b, axis, axis=1)
    cross = lambda x, y: float(x[0] * y[1] - x[1] * y[0])
    sign = 1 if cross(clip[1] - clip[0], clip[2] - clip[0]) >= 0 else -1
    for i in range(3):
        x, y = clip[i], clip[(i + 1) % 3]
        result = []
        for p, q in zip(polygon, np.roll(polygon, -1, axis=0)):
            dp = sign * cross(y - x, p - x)
            dq = sign * cross(y - x, q - x)
            if dp >= -EPS:
                result.append(p)
            if (dp > EPS and dq < -EPS) or (dp < -EPS and dq > EPS):
                result.append(p + (q - p) * dp / (dp - dq))
        polygon = np.asarray(result)
        if len(polygon) < 3:
            return 0.0
    area = abs(sum(cross(p, q) for p, q in zip(polygon, np.roll(polygon, -1, axis=0)))) / 2
    return area / abs(normal[axis])


def classify(a, b):
    # 원점 이동 후 float64로 판정해 월드 좌표에 대한 단정도 BVH 오차를 분리한다.
    origin = a[0].copy()
    a, b = a - origin, b - origin
    normals = [np.cross(x[1] - x[0], x[2] - x[0]) for x in (a, b)]
    areas = [np.linalg.norm(n) / 2 for n in normals]
    altitudes = [2 * area / max(np.linalg.norm(x[(i + 1) % 3] - x[i]) for i in range(3))
                 for x, area in zip((a, b), areas)]
    metrics = {'areas_cm2': areas, 'min_altitudes_cm': altitudes}
    if min(altitudes) <= EPS:
        return dict(metrics, category='degenerate', cause='zero area or altitude within numerical tolerance')
    na, nb = [n / np.linalg.norm(n) for n in normals]
    da, db = (a - b[0]) @ nb, (b - a[0]) @ na
    metrics['plane_distances_cm'] = [da.tolist(), db.tolist()]
    if any(np.min(d) > EPS or np.max(d) < -EPS for d in (da, db)):
        return dict(metrics, category='separated', cause='float64 supporting planes separate triangles; BVH candidate only')
    direction = np.cross(na, nb)
    length = np.linalg.norm(direction)
    if length < 1e-10:
        area = coplanar_area(a, b, na)
        return dict(metrics, category='coplanar_overlap' if area > EPS else 'coplanar_contact',
                    overlap_area_cm2=area, cause='coplanar triangle clipping')
    direction /= length
    sa, sb = section(a, da), section(b, db)
    if not len(sa) or not len(sb):
        return dict(metrics, category='separated', cause='no common plane section')
    ia, ib = sa @ direction, sb @ direction
    lo, hi = max(min(ia), min(ib)), min(max(ia), max(ib))
    metrics['intersection_length_cm'] = max(0.0, float(hi - lo))
    if hi < lo - EPS:
        return dict(metrics, category='separated', cause='disjoint plane intersection intervals')
    p = sa[0] + direction * ((lo + hi) / 2 - np.dot(sa[0], direction))
    bary = [barycentric(p, tri) for tri in (a, b)]
    strict = hi - lo > EPS and min(float(x.min()) for x in bary) > 1e-9
    return dict(metrics, category='proper_crossing' if strict else 'tangent_contact',
                barycentric_midpoint=[x.tolist() for x in bary], intersection_world_cm=(p + origin).tolist(),
                cause='positive segment through both triangle interiors' if strict else 'vertex or edge tangency; no interior crossing')


def independent_edge_hits(a, b):
    """Moller 방식 선분/삼각형 검사를 양방향 적용해 평면 단면 판정을 교차 확인한다."""
    hits = []
    for source, target in ((a, b), (b, a)):
        e1, e2 = target[1] - target[0], target[2] - target[0]
        for i in range(3):
            origin = source[i]
            direction = source[(i + 1) % 3] - origin
            h = np.cross(direction, e2)
            determinant = np.dot(e1, h)
            if abs(determinant) < 1e-12:
                continue
            s = origin - target[0]
            u = np.dot(s, h) / determinant
            q = np.cross(s, e1)
            w = np.dot(direction, q) / determinant
            along = np.dot(e2, q) / determinant
            if min(u, w, 1-u-w, along, 1-along) >= -1e-9:
                hits.append((origin + along * direction).tolist())
    return hits


def self_check():
    a = np.asarray([[0., 0., 0.], [2., 0., 0.], [0., 2., 0.]])
    cases = [
        (a + [0, 0, 1], 'separated'),
        (a + [.2, .2, 0], 'coplanar_overlap'),
        (a + [2, 0, 0], 'coplanar_contact'),
        (np.asarray([[.5, .5, -1], [.5, .5, 1], [1.5, .5, 0]]), 'proper_crossing'),
        (np.asarray([[0., 0., 0.], [-1, 0, 1], [0, -1, 1]]), 'tangent_contact'),
        (np.asarray([[0., 0., 0.], [1, 0, 0], [2, 0, 0]]), 'degenerate'),
    ]
    for b, expected in cases:
        assert classify(a, b)['category'] == expected, expected
        assert classify(b, a)['category'] == expected, expected


def main():
    self_check()
    preservation()
    build = read(SOURCE / 'Geometry_V005/Build.json')
    results = []
    for row, expected in zip(build['outputs'], (12, 16, 311, 0)):
        path = SOURCE / 'Geometry_V005' / (row['name'] + '.bin')
        v, t = binary(path)
        tree = BVHTree.FromPolygons(v.tolist(), t.tolist(), all_triangles=True)
        raw = [(a, b) for a, b in tree.overlap(tree) if a < b]
        candidates = [(a, b) for a, b in raw if not set(t[a]).intersection(t[b])]
        assert len(candidates) == expected, (row['name'], len(candidates))
        pairs = []
        for a, b in candidates:
            result = classify(v[t[a]], v[t[b]])
            if result['category'] == 'proper_crossing':
                hits = independent_edge_hits(v[t[a]], v[t[b]])
                assert len(hits) >= 2, (row['name'], a, b, hits)
                span=max(np.linalg.norm(np.asarray(p)-q) for p in hits for q in hits)
                assert abs(span-result['intersection_length_cm']) < 1e-4
                result['independent_edge_hits'] = hits
            pairs.append(dict(result, faces=[a, b], vertices=[t[a].tolist(), t[b].tolist()],
                              centroid_world_cm=v[t[[a, b]]].mean(axis=(0, 1)).tolist()))
        summary = dict(collections.Counter(p['category'] for p in pairs))
        shared_pairs = []
        for a, b in raw:
            shared = sorted(set(t[a]).intersection(t[b]))
            if shared:
                result = classify(v[t[a]], v[t[b]])
                if result['category']=='proper_crossing':
                    hits=independent_edge_hits(v[t[a]],v[t[b]])
                    span=max(np.linalg.norm(np.asarray(p)-q) for p in hits for q in hits)
                    assert abs(span-result['intersection_length_cm']) < 1e-4
                    result['independent_edge_hits']=hits
                shared_pairs.append(dict(result, faces=[a, b], shared_vertices=[int(i) for i in shared]))
        result = {'name': row['name'], 'binary': path.relative_to(ROOT).as_posix(), 'binary_sha256': sha(path),
                  'vertices': len(v), 'triangles': len(t), 'bvh_unique_pairs': len(raw),
                  'shared_vertex_excluded': len(raw) - len(candidates), 'candidate_count': len(candidates),
                  'summary': summary, 'pairs': pairs, 'shared_bvh_pairs': shared_pairs,
                  'shared_pair_summary': dict(collections.Counter(p['category'] for p in shared_pairs))}
        results.append(result)
        print('CLASSIFICATION ' + json.dumps({k: result[k] for k in ('name', 'candidate_count', 'summary')}), flush=True)
    write('CandidateClassification.json', {'unit': 'U5a-6', 'source_map': read(AUTHOR / 'Acceptance_V006.json')['level'],
          'numeric_tolerance_cm': EPS, 'face_index': 'zero based stored V005 binary triangle index; not UE render face index',
          'body_results': results, 'total_candidates': sum(r['candidate_count'] for r in results),
          'collision_or_PIE_pass': False, 'source_modified': False})


if __name__ == '__main__':
    main()
