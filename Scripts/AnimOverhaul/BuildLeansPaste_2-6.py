# AnimationOverhaul 2-6 — GASP AdditiveLeans 레이어 Ctrl+C 텍스트를 ABP_Main_UEFN AnimGraph 붙여넣기용으로 바꾼다.
#
# 입력: Saved/GASPRef/leans.txt (0-2에서 사용자가 GASP SandboxCharacter_Mover_ABP의 AdditiveLeans 레이어를 Ctrl+C)
# 출력: Saved/AnimOverhaul/lnp_2-6_leans_paste.txt (에디터 AnimGraph에 붙여넣는다)
#
# 바꾸는 것:
# - 레이어 입·출력(Root, LinkedInputPose)을 뺀다. 입력 캐시 PreLeanPose와 출력은 붙여넣은 뒤 MCP로 잇는다.
# - 이동 모드 Enum 블렌드를 뺀다(LootNPop에는 Sliding이 없다).
# - 이동 상태 Enum 블렌드를 Bool 블렌드로 바꾸고 C++ ShouldApplyAdditiveLeans에 바인딩한다(AN-003).
# - 머리 Bool 블렌드를 EnableAO 대신 C++ ShouldApplyHeadLean에 바인딩한다. 참·거짓 뜻이 반대라 두 캐시 참조를 맞바꾼다.
# - 몸통 Walk↔Run α 바인딩을 Speed2D(double)에서 GroundSpeed(float, 접평면 속력)로 바꾼다. LeanAmount 바인딩은 이름이 같아 그대로 둔다.
# - ExportPath와 GASP 영문 주석 노드를 빼고 한국어 주석 노드 하나를 둔다.
#
# 사용: python -I Scripts/AnimOverhaul/BuildLeansPaste_2-6.py  (프로젝트 루트에서)

import re
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "Saved/GASPRef/leans.txt"
DST = ROOT / "Saved/AnimOverhaul/lnp_2-6_leans_paste.txt"

DROP = {"AnimGraphNode_Root_0", "AnimGraphNode_LinkedInputPose_0", "AnimGraphNode_BlendListByEnum_1",
        "AnimGraphNode_UseCachedPose_1", "EdGraphNode_Comment_44"}


def new_guid():
    return uuid.uuid4().hex.upper()


def split_blocks(text):
    blocks = re.split(r"\n(?=Begin Object Class=)", text.strip("﻿\r\n"))
    out = {}
    order = []
    for b in blocks:
        name = re.match(r'Begin Object Class=\S+ Name="([^"]+)"', b).group(1)
        out[name] = b.rstrip("\r\n")
        order.append(name)
    return out, order


def pin_id(block, pin_name):
    return re.search(r'CustomProperties Pin \(PinId=([0-9A-F]+),PinName="%s"' % re.escape(pin_name), block).group(1)


def set_linked(block, pin_name, links):
    """pin_name 핀의 LinkedTo를 links([(노드, 핀ID)])로 바꾼다. 빈 목록이면 LinkedTo를 지운다."""
    lines = block.split("\n")
    for i, line in enumerate(lines):
        if re.search(r'PinName="%s"' % re.escape(pin_name), line) and "CustomProperties Pin" in line:
            line = re.sub(r"LinkedTo=\([^)]*\),", "", line)
            if links:
                linked = "LinkedTo=(" + "".join("%s %s," % l for l in links) + "),"
                line = line.replace("PersistentGuid=", linked + "PersistentGuid=", 1)
            lines[i] = line
            return "\n".join(lines)
    raise KeyError(pin_name)


def rebind(block, old_text, old_path, new_text, new_path):
    block = re.sub(r'(PathAsText=NSLOCTEXT\("", "[0-9A-F]+", )"%s"' % re.escape(old_text), r'\1"%s"' % new_text, block)
    return block.replace('PropertyPath=("%s")' % old_path, 'PropertyPath=("%s")' % new_path)


def main():
    blocks, order = split_blocks(SRC.read_text(encoding="utf-8"))

    head = "AnimGraphNode_BlendListByBool_0"
    state = "AnimGraphNode_BlendListByBool_1"
    curve = "AnimGraphNode_TwoWayBlend_0"
    pre_lean_for_state = "AnimGraphNode_UseCachedPose_0"

    # 이동 상태 Bool 블렌드 = 머리 Bool 블렌드 사본(새 이름·새 핀 GUID). 위치는 원래 Enum 블렌드 자리
    enum_state = blocks["AnimGraphNode_BlendListByEnum_2"]
    state_block = blocks[head].replace('Name="%s"' % head, 'Name="%s"' % state)
    for old in re.findall(r"PinId=([0-9A-F]{32})", state_block) + re.findall(r"NodeGuid=([0-9A-F]{32})", state_block):
        state_block = state_block.replace(old, new_guid())
    pos_x = re.search(r"NodePosX=(-?\d+)", enum_state).group(1)
    pos_y = re.search(r"NodePosY=(-?\d+)", enum_state)
    state_block = re.sub(r"NodePosX=-?\d+", "NodePosX=" + pos_x, state_block)
    state_block = re.sub(r"NodePosY=-?\d+", "NodePosY=" + (pos_y.group(1) if pos_y else "0"), state_block)
    state_block = rebind(state_block, "Enable AO", "EnableAO", "Should Apply Additive Leans", "ShouldApplyAdditiveLeans")
    blocks[state] = state_block
    order[order.index("AnimGraphNode_BlendListByEnum_2")] = state
    DROP.add("AnimGraphNode_BlendListByEnum_2")

    # 연결: 참(0) ← 머리 블렌드, 거짓(1) ← PreLeanPose, 출력 → 곡선 TwoWayBlend A
    blocks[state] = set_linked(blocks[state], "BlendPose_0", [(head, pin_id(blocks[head], "Pose"))])
    blocks[state] = set_linked(blocks[state], "BlendPose_1", [(pre_lean_for_state, pin_id(blocks[pre_lean_for_state], "Pose"))])
    blocks[state] = set_linked(blocks[state], "Pose", [(curve, pin_id(blocks[curve], "A"))])
    blocks[head] = set_linked(blocks[head], "Pose", [(state, pin_id(blocks[state], "BlendPose_0"))])
    blocks[pre_lean_for_state] = set_linked(blocks[pre_lean_for_state], "Pose", [(state, pin_id(blocks[state], "BlendPose_1"))])
    blocks[curve] = set_linked(blocks[curve], "A", [(state, pin_id(blocks[state], "Pose"))])

    # 레이어 입·출력 연결 끊기(MCP로 다시 잇는다)
    blocks["AnimGraphNode_SaveCachedPose_0"] = set_linked(blocks["AnimGraphNode_SaveCachedPose_0"], "Pose", [])
    blocks[curve] = set_linked(blocks[curve], "Pose", [])

    # 머리 블렌드: 참 = 머리 기울기 적용. 참 핀(UseCachedPose_7)이 Body+HeadLeans, 거짓 핀(UseCachedPose_4)이 BodyLeans를 보게 맞바꾼다
    blocks[head] = rebind(blocks[head], "Enable AO", "EnableAO", "Should Apply Head Lean", "ShouldApplyHeadLean")
    for use, cache in (("AnimGraphNode_UseCachedPose_7", "AnimGraphNode_SaveCachedPose_2"),
                       ("AnimGraphNode_UseCachedPose_4", "AnimGraphNode_SaveCachedPose_1")):
        blocks[use] = re.sub(r"(SaveCachedPoseNode=\"[^']*')[^']*('\")", r"\g<1>%s\2" % cache, blocks[use])

    # 몸통 α: Speed2D(double) → GroundSpeed(float). 핀과 같은 형식이라 승격 표시를 뺀다
    walk_run = "AnimGraphNode_TwoWayBlend_1"
    b = rebind(blocks[walk_run], "Speed 2D", "Speed2D", "Ground Speed", "GroundSpeed")
    b = b.replace('PinType=(PinCategory="real",PinSubCategory="double"),PromotedPinType=(PinCategory="real",PinSubCategory="double")',
                  'PinType=(PinCategory="real",PinSubCategory="float"),PromotedPinType=(PinCategory="real",PinSubCategory="float")')
    blocks[walk_run] = b.replace(",bIsPromotion=True", "")

    # 주석
    comment = "EdGraphNode_Comment_43"
    blocks[comment] = re.sub(r'NodeComment="[^"]*"',
                             'NodeComment="Additive Leans (AnimationOverhaul 2-6, GASP AdditiveLeans). 몸통 기울기 = 측면 가속(LeanAmount), 머리 기울기는 FreeAim·락온이 아닐 때만(AO와 겹치면 머리 과회전)."',
                             blocks[comment])

    out = []
    for name in order:
        if name in DROP:
            continue
        b = re.sub(r' ExportPath="[^"]*"', "", blocks[name])
        for dropped in DROP:
            b = re.sub(r"%s [0-9A-F]{32}," % re.escape(dropped), "", b)
        out.append(b)
    text = "\n".join(out) + "\n"
    text = text.replace("LinkedTo=(),", "")

    # 노드 이름에 접미사를 붙여 ABP에 이미 있는 같은 이름(SaveCachedPose_0 등)과 캐시 참조가 섞이지 않게 한다
    for name in order:
        if name not in DROP:
            text = re.sub(r"\b%s\b" % re.escape(name), name + "_Lean", text)

    DST.parent.mkdir(parents=True, exist_ok=True)
    DST.write_text(text, encoding="utf-8")
    print("wrote", DST, "nodes", len(out))


if __name__ == "__main__":
    main()
