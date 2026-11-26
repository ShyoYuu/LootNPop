"""U5a-6 국소 최소 보정 가능성을 오프라인 사본에서 조사한다. UE 에셋은 변경하지 않는다."""
import json
from pathlib import Path
import sys
import numpy as np
from mathutils.bvhtree import BVHTree

sys.path.insert(0, str(Path(__file__).parent))
import ReviewRidgeBodyIntersections as review


def crossings(v, t):
    tree = BVHTree.FromPolygons(v.tolist(), t.tolist(), all_triangles=True)
    raw = [(a, b) for a, b in tree.overlap(tree) if a < b]
    return [(a, b) for a, b in raw if review.classify(v[t[a]], v[t[b]])['category'] in ('proper_crossing', 'coplanar_overlap')]


def main():
    classification = review.read(review.OUT / 'CandidateClassification.json')
    results = []
    for body in classification['body_results'][:3]:
        v, t = review.binary(review.ROOT / body['binary'])
        initial = crossings(v, t)
        neighbors = [set() for _ in v]
        for a, b, c in t:
            neighbors[a].update((b, c)); neighbors[b].update((a, c)); neighbors[c].update((a, b))
        seeds = set(int(i) for a, b in initial for face in (a, b) for i in t[face])
        active = set(seeds)
        for _ in range(2):
            active.update(n for i in list(active) for n in neighbors[i])
        current = v.copy()
        trials = []
        remaining = initial
        for iteration in range(1, 81):
            active_now = set(int(i) for a, b in remaining for f in (a, b) for i in t[f])
            active_now.update(n for i in list(active_now) for n in neighbors[i])
            ids = np.asarray(sorted(active_now), int)
            delta = np.asarray([current[list(neighbors[i])].mean(axis=0) - current[i] for i in ids])
            current[ids] += .2 * delta
            # 채택 바깥 경계의 축별 극값을 그대로 둔다. 전역 수축이나 캡 재구성은 하지 않는다.
            for axis in range(3):
                extreme = (v[:,axis] == v[:,axis].min()) | (v[:,axis] == v[:,axis].max())
                current[extreme,axis] = v[extreme,axis]
            if iteration % 2 == 0:
                remaining = crossings(current, t)
                displacement = np.linalg.norm(current - v, axis=1)
                trial = {'iterations': iteration, 'remaining_crossings': len(remaining),
                         'changed_vertices': int((displacement > 1e-8).sum()),
                         'max_displacement_cm': float(displacement.max())}
                trials.append(trial)
                print('LOCAL_REPAIR ' + json.dumps(dict(name=body['name'], **trial)), flush=True)
                if not remaining:
                    np.savez(review.OUT / (body['name'] + '_ConstrainedProposal.npz'), vertices=current, triangles=t)
                    break
        results.append({'name': body['name'], 'initial_including_shared': len(initial), 'seed_vertices': len(seeds),
                        'active_vertices': len(active), 'trials': trials})
    review.write('ConstrainedRepairExploration.json', {'method': 'current crossing faces plus one ring; stop moving resolved areas; axis extrema and all other vertices fixed',
                   'source_modified': False, 'results': results})


if __name__ == '__main__':
    main()
