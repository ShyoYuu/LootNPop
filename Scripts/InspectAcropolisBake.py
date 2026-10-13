"""베이크된 지각 seam의 반지름 양자화와 Valid 비트를 원본과 비교한다."""

import json
import struct
from pathlib import Path

import unreal


def spawn_anchors(path):
    payload=bytes(unreal.load_asset(path).get_editor_property("spawn_payload"))
    version,reserved,count,random_count=struct.unpack_from("<HHII",payload)
    assert version==3
    offset,result=12,[]
    for _ in range(count):
        guid=struct.unpack_from("<IIII",payload,offset)
        offset+=16
        length=struct.unpack_from("<i",payload,offset)[0]
        offset+=4+length
        location=struct.unpack_from("<fff",payload,offset)
        offset+=28
        layer,reserved,edge,capsule,headroom=struct.unpack_from("<HHfff",payload,offset)
        offset+=16
        formations=struct.unpack_from("<I",payload,offset)[0]
        offset+=4
        for _ in range(formations):
            length=struct.unpack_from("<i",payload,offset)[0]
            offset+=4+length+16
        result.append({"guid":"".join(f"{v:08X}" for v in guid),"location":location,"headroom":headroom,"layer":layer})
    return result


def seams(path):
    asset = unreal.load_asset(path)
    payload = bytes(asset.get_editor_property("support_payload"))
    version, layers, sources, reserved, step = struct.unpack_from("<HHHHd", payload)
    offset = 76
    for layer in range(layers):
        source, reserved, base, n1, j01, rows = struct.unpack_from("<HHdiii", payload, offset)
        offset += 24
        table = []
        count1 = 0
        for j in range(rows):
            start, length = struct.unpack_from("<ii", payload, offset)
            table.append((start, count1))
            count1 += length
            offset += 8
        if layer == 0:
            row_table, count, n, j0 = table, count1, n1, j01
    def sample(i, j):
        index = row_table[j-j0][1] + i - row_table[j-j0][0]
        radius = struct.unpack_from("<h", payload, offset + 2*index)[0]
        valid = payload[offset + count*6 + index] & 1
        return radius, valid
    return [[sample(i, 0) for i in range(n+1)], [sample(0, j) for j in range(n+1)],
            [sample(n-j, j) for j in range(n+1)]]


def run():
    original = seams("/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00")
    current = seams("/Game/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00")
    differences = [{"edge":e,"step":i,"reference":a,"actual":b}
                   for e in range(3) for i,(a,b) in enumerate(zip(original[e], current[e])) if a != b]
    root = Path(unreal.Paths.project_dir()).resolve()
    (root / "Saved/SliceOctant/Acropolis_00/V016/seam-differences.json").write_text(json.dumps(differences, indent=2))
    unreal.log("ACROPOLIS_SEAM_DIFF " + json.dumps({"count":len(differences),"first":differences[:12]}))
    return differences


if __name__ == "__main__":
    run()
