"""기존 뿌리/벽을 폐기한 새 Grove 뿌리 덩어리. cm, H는 구 중심 쪽이다."""
import math

VERSION="V042"
GEOMETRY_VERSION="V040"
ANCHOR_BASE_VERSION="V041"
BASELINE_VERSION="V021"
RADIUS=30000.0
SEAM_MARGIN=1500.0
SEAM_BAND=4000.0
SEGMENTS=32
SUPPORT_HEIGHT=3200
SUPPORT_MIN_PATCH_AREA_CM2=100
PAD_GRID_SPACING=200
TREES=[
    {"id":"T1","xy":[-7000,-7500],"radius":2300,"height":11000,"fork":25},
    {"id":"T2","xy":[0,0],"radius":3000,"height":14000,"fork":105},
    {"id":"T4","xy":[7500,-8000],"radius":2200,"height":12500,"fork":155},
    {"id":"T5","xy":[17500,0],"radius":3200,"height":17000,"fork":65},
    {"id":"T3","xy":[-5000,11000],"radius":2100,"height":12000,"fork":35},
    {"id":"T6","xy":[-13000,-15000],"radius":1700,"height":10000,"fork":125},
    {"id":"T7","xy":[-13700,7500],"radius":1900,"height":10500,"fork":205}]
CLEARINGS=[
    {"id":"ClearingA","xy":[-12500,-2500],"h":-1000},
    {"id":"ClearingB","xy":[1000,-12000],"h":-1000},
    {"id":"ClearingC","xy":[4500,8500],"h":-1400},
    {"id":"ClearingE","xy":[-12500,14500],"h":-1000},
    {"id":"BossClearing","xy":[10500,4000],"h":-1200}]

def point(x,y,h):
    raw=(RADIUS/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2),RADIUS/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2),RADIUS/math.sqrt(3)+2*x/math.sqrt(6))
    scale=(RADIUS-h)/math.sqrt(sum(v*v for v in raw))
    return tuple(v*scale for v in raw)

def smooth(t):
    t=max(0,min(1,t))
    return t*t*(3-2*t)

def raw_ground(x,y):
    envelope=smooth((min(point(x,y,0))-5500)/3500)
    waves=450*math.sin(x/4200+.7)*math.cos(y/5000-.5)+320*math.sin((x+.5*y)/2800+1)+180*math.cos((x-y)/3500)
    h=-1000+waves*envelope
    basin=math.exp(-((x+9000)/4200)**2-((y-4000)/4200)**2)
    return h+(-2600-h)*basin

VAULT_XY=[-13700+(6700*.28),7500-(15000*.28)]
GROUND_PADS=CLEARINGS+[{"id":"RootVault","xy":VAULT_XY,"h":raw_ground(*VAULT_XY)}]

def ground(x,y):
    h=raw_ground(x,y)
    for pad in GROUND_PADS:
        t=smooth((math.hypot(x-pad["xy"][0],y-pad["xy"][1])-1400)/1800)
        h=pad["h"]+(h-pad["h"])*t
    return h

ROOTS=[
    {"id":"R1","pair":["T6","T1"],"height":1500,"base_width":3200,"top_width":3200,"lean":0,"feature":"ZRamp"},
    {"id":"R2","pair":["T1","T2"],"height":2000,"base_width":2600,"top_width":1600,"lean":150,"feature":"Tunnel","tunnels":[[.45,1800,900]]},
    {"id":"R3","pair":["T2","T4"],"height":1800,"base_width":7000,"top_width":1400,"lean":0,"feature":"CrossRamp","tunnels":[[.65,1600,800]]},
    {"id":"R4","pair":["T4","T5"],"height":2200,"base_width":2800,"top_width":1800,"lean":-150,"feature":"Hook"},
    {"id":"R5","pair":["T2","T5"],"height":2500,"base_width":2000,"top_width":2000,"lean":0,"feature":"Crown","tunnels":[[.52,2400,1000]]},
    {"id":"R6","pair":["T2","T3"],"height":1800,"base_width":2400,"top_width":1400,"lean":-100,"feature":"Notch","notches":[[.56,1600]]},
    {"id":"R7","pair":["T3","T7"],"height":1500,"base_width":2400,"top_width":1600,"lean":100,"feature":"ChippedArch","tunnels":[[.55,1400,700]]},
    {"id":"R8","pair":["T7","T1"],"height":2100,"base_width":2800,"top_width":1800,"lean":0,"feature":"VaultBreach","tunnels":[[.28,1600,800]],"notches":[[.49,1800]]}]
TREE_BY_ID={t["id"]:t for t in TREES}
ROOT_BY_ID={r["id"]:r for r in ROOTS}

def basis(root):
    a,b=(TREE_BY_ID[i]["xy"] for i in root["pair"])
    dx,dy=b[0]-a[0],b[1]-a[1]
    length=math.hypot(dx,dy)
    return a,b,(-dy/length,dx/length),length

def root_xy(root,t,lateral=0):
    a,b,side,_=basis(root)
    return [a[i]+(b[i]-a[i])*t+side[i]*lateral for i in range(2)]

def root_top(root,t):
    h=ground(*root_xy(root,t))+root["height"]
    if root["id"] in ("R4","R5"):
        center=ground(*root_xy(root,.5))+root["height"]
        return center+(h-center)*smooth((abs(t-.5)*basis(root)[3]-700)/1000)
    return h

def cut_height(root,t,lateral,fraction):
    g=ground(*root_xy(root,t,lateral))
    return g+(root_top(root,t)-g)*fraction+10*(1-fraction)

def root_outline(root,inset=0):
    _,_,_,length=basis(root)
    e=inset/length
    half=root["base_width"]/2-inset
    return [root_xy(root,t,l) for t,l in ((e,-half),(1-e,-half),(1-e,half),(e,half))]

def tree_outline(tree,scale=.995):
    return [[tree["xy"][0]+tree["radius"]*scale*math.cos(i*math.tau/SEGMENTS),tree["xy"][1]+tree["radius"]*scale*math.sin(i*math.tau/SEGMENTS)] for i in range(SEGMENTS)]

def crossing(root_id,t,sign=1):
    root=ROOT_BY_ID[root_id]
    reach=root["base_width"]/2+900
    return [root_xy(root,t,sign*reach),root_xy(root,t),root_xy(root,t,-sign*reach)]

# 승인된 지상 구성 위에 더하는 세로 동선. 높이는 모두 구면 투영 전 cm이다.
UNDERGROUND={"xy":[-9000,1000],"floor":-10000,"radius":5500,"shell_radius":6100,"headroom":5000,
    "entrances":[{"id":"West","tree":"T1","xy":[-10500,-6500],"type":"Shaft","floor":-8800,"path_step":100,
                  "points":[[-10500,-6500],[-12000,-3500],[-9000,1000]]},
                 {"id":"Central","tree":"T2","xy":[4000,2500],"type":"Ramp",
                  "points":[[4000,2500],[6000,-3000],[3500,-8000],[-2500,-8000],[-7500,-5000],[-9000,1000]]},
                 {"id":"North","tree":"T3","xy":[-1800,11000],"type":"Shaft","floor":-8800}]}
SHAFT_FLAT_LENGTH=1200
SHAFT_JOIN_MARGIN=600

def cave_path(points,step):
    result=[]
    for i in range(len(points)-1):
        a,b,c,d=points[max(0,i-1)],points[i],points[i+1],points[min(len(points)-1,i+2)]
        count=max(1,math.ceil(math.dist(b,c)/step))
        for j in range(count):
            t=j/count
            result.append([.5*(2*b[k]+(-a[k]+c[k])*t+(2*a[k]-5*b[k]+4*c[k]-d[k])*t*t+(-a[k]+3*b[k]-3*c[k]+d[k])*t*t*t) for k in range(2)])
    return result+[points[-1]]

for entrance in UNDERGROUND["entrances"]:
    entrance["h"]=ground(*entrance["xy"])+20
    entrance["path"]=cave_path(entrance.get("points",[entrance["xy"],UNDERGROUND["xy"]]),entrance.get("path_step",400))
    distances=[0]
    for a,b in zip(entrance["path"],entrance["path"][1:]):distances.append(distances[-1]+math.dist(a,b))
    entrance["distances"]=distances;entrance["length"]=distances[-1]
    if entrance["type"]=="Shaft":
        tree=TREE_BY_ID[entrance["tree"]];dx,dy=(a-b for a,b in zip(entrance["xy"],tree["xy"]));length=math.hypot(dx,dy)
        entrance["axis"]=[dx/length,dy/length]

def underground_floor(entrance,t):
    if entrance["type"]=="Shaft":
        start=entrance.get("floor",UNDERGROUND["floor"])
        end=entrance["length"]-UNDERGROUND["radius"]-SHAFT_JOIN_MARGIN
        return start-2+(UNDERGROUND["floor"]-start)*max(0,min(1,(t*entrance["length"]-SHAFT_FLAT_LENGTH)/(end-SHAFT_FLAT_LENGTH)))
    end=entrance["length"]-UNDERGROUND["radius"]-800
    return entrance["h"]+(UNDERGROUND["floor"]-2-entrance["h"])*min(1,t*entrance["length"]/end)

def underground_roof(entrance,t):
    if entrance["type"]=="Shaft":return underground_floor(entrance,t)+3000
    end=entrance["length"]-UNDERGROUND["radius"]-800
    return underground_floor(entrance,t)+1200+1800*smooth((t*entrance["length"]-end+2400)/2400)

def underground_xy(entrance,t):
    distance=t*entrance["length"]
    for i,(a,b) in enumerate(zip(entrance["distances"],entrance["distances"][1:])):
        if distance<=b:
            u=(distance-a)/(b-a)
            return [p+(q-p)*u for p,q in zip(entrance["path"][i],entrance["path"][i+1])]
    return list(entrance["path"][-1])

def cave_nearest(entrance,x,y):
    best=None
    for i,(a,b) in enumerate(zip(entrance["path"],entrance["path"][1:])):
        dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
        u=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/length**2))
        distance=math.hypot(x-a[0]-dx*u,y-a[1]-dy*u)
        if best is None or distance<best[0]:best=(distance,(entrance["distances"][i]+length*u)/entrance["length"])
    return best

def cave_outline(entrance,half=1000,extend=500):
    path=[list(p) for p in entrance["path"]]
    dx,dy=path[1][0]-path[0][0],path[1][1]-path[0][1];length=math.hypot(dx,dy)
    path[0]=[path[0][0]-dx/length*extend,path[0][1]-dy/length*extend]
    sides=[[],[]]
    for i,p in enumerate(path):
        a,b=path[max(0,i-1)],path[min(len(path)-1,i+1)];dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
        for side,sign in zip(sides,(-1,1)):side.append([p[0]-dy/length*half*sign,p[1]+dx/length*half*sign])
    return sides[0]+list(reversed(sides[1]))

def shaft_point(entrance,along,h):
    return [entrance["xy"][0]+entrance["axis"][0]*along,entrance["xy"][1]+entrance["axis"][1]*along,h]

def cavern_roof(x,y):
    depth=UNDERGROUND["radius"]-math.dist((x,y),UNDERGROUND["xy"])
    return UNDERGROUND["floor"]+3000+(UNDERGROUND["headroom"]-3000)*smooth(depth/1800)

CAVE_COVER=[
    {"id":"PillarWest","xy":[-11400,1800],"radius":500,"height":5400,"type":"Pillar"},
    {"id":"PillarEast","xy":[-6500,2600],"radius":500,"height":5400,"type":"Pillar"},
    {"id":"RockWest","xy":[-11800,0],"radius":850,"height":600,"type":"Rock"},
    {"id":"RockEast","xy":[-6900,-1000],"radius":650,"height":900,"type":"Rock"},
    {"id":"RockNorth","xy":[-9800,3600],"radius":600,"height":750,"type":"Rock"}]

def cave_lights():
    lights=[]
    def add(name,local,radius,lumens):
        lights.append({"id":name,"local":local,"radius":radius,"lumens":lumens})
    cx,cy=UNDERGROUND["xy"]
    add("Hub",[cx,cy,UNDERGROUND["floor"]+1800],3800,90000)
    for i in range(8):
        a=i*math.tau/8
        add("Room"+str(i),[cx+3500*math.cos(a),cy+3500*math.sin(a),UNDERGROUND["floor"]+1600],3400,70000)
    for entrance in UNDERGROUND["entrances"]:
        count=math.ceil(entrance["length"]/1500)
        for i in range(count+1):
            t=i/count;x,y=underground_xy(entrance,t)
            if math.dist((x,y),UNDERGROUND["xy"])<UNDERGROUND["radius"]-600:continue
            add(entrance["id"]+"Passage"+str(i),[x,y,underground_floor(entrance,t)+550],2100,25000)
        if entrance["type"]=="Shaft":
            for i,h in enumerate((-3300,-5700,-7900)):
                add(entrance["id"]+"Shaft"+str(i),[*entrance["xy"],h],3000,40000)
    for i,(dx,dy) in enumerate(((-2400,-2400),(2400,-2400),(2400,2400),(-2400,2400))):
        add("RoomFill"+str(i),[cx+dx,cy+dy,UNDERGROUND["floor"]+2800],4200,45000)
    add("RoomEdgeEast",[-4300,1050,UNDERGROUND["floor"]+1000],2400,40000)
    add("RoomEdgeNorth",[-10600,5400,UNDERGROUND["floor"]+1000],2400,40000)
    return lights

def crown_spaces(tree):
    x,y=tree["xy"];r=tree["radius"];base=ground(x,y)
    # 주 가지와 같은 방향으로 붙여, 줄기 뒤를 관통하는 훅 경로를 만들지 않는다.
    a=math.radians(tree["fork"])
    spaces=[]
    for name,fraction,reach,radius in (("LowerBranch",.34,2.8,1800),("UpperBranch",.66,2.6,1600),("Summit",1,1.8,1800)):
        angle=a+(math.radians(65) if name=="UpperBranch" else 0)
        direction=(math.cos(angle),math.sin(angle))
        xy=[x+direction[0]*r*reach,y+direction[1]*r*reach]
        h=base+tree["height"]*fraction
        if tree["id"]=="T4" and name=="LowerBranch":
            # T1 낮은 가지와 겹치는 가장자리는 기본 점프로 양방향 횡단하도록 낮춘다.
            other=TREE_BY_ID["T1"]
            h=ground(*other["xy"])+other["height"]*.34+80
        spaces.append({"id":tree["id"]+name,"tree":tree["id"],"local":[*xy,h],
                       "radius":radius,"direction":list(direction)})
    return spaces

CROWN_SPACES=[s for t in TREES for s in crown_spaces(t)]
BRANCH_RAMPS=[]
for name in ("T2UpperBranch","T5UpperBranch","T1UpperBranch","T4UpperBranch","T3UpperBranch","T6UpperBranch","T7UpperBranch"):
    space=next(s for s in CROWN_SPACES if s["id"]==name)
    tree=TREE_BY_ID[space["tree"]];ux,uy=space["direction"];x,y=tree["xy"]
    cap=ground(x,y)+tree["height"]*.6
    start=[x+ux*tree["radius"]*.25,y+uy*tree["radius"]*.25,cap-100]
    end=[space["local"][0]-ux*space["radius"]*.5,space["local"][1]-uy*space["radius"]*.5,space["local"][2]]
    crest=1-space["radius"]*.65/math.dist(start[:2],end[:2])
    BRANCH_RAMPS.append({"id":name,"tree":tree["id"],"start":start,"end":end,"crest":crest,"width":1300,"cap":cap})

def branch_ramp_point(ramp,t,lateral=0):
    a,b=ramp["start"],ramp["end"];dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
    return [a[0]+dx*t-dy/length*lateral,a[1]+dy*t+dx/length*lateral,a[2]+(b[2]-a[2])*min(1,t/ramp["crest"])]

def ascent_start(tree):
    # 최종 V024 외피의 지면/시야/복귀 광선에서 고른 실제 저작 출발점이다.
    xy={"T1":[-1564,-8459],"T2":[1667,9454],"T4":[-860,-8000],"T5":[13660,6651],
        "T3":[-4125,15963],"T6":[-12173,-10312],"T7":[-12908,3009]}[tree["id"]]
    return [*xy,ground(*xy)+10]

def support_surface(x,y,h):
    if h<ground(x,y)+SUPPORT_HEIGHT or 2200<h<3000:return True
    for ramp in BRANCH_RAMPS:
        tree=TREE_BY_ID[ramp["tree"]]
        if abs(h-ramp["cap"])<35 and math.dist((x,y),tree["xy"])<tree["radius"]*.9:return True
        a,b=ramp["start"],ramp["end"];dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
        t=((x-a[0])*dx+(y-a[1])*dy)/length**2;side=abs(-(x-a[0])*dy+(y-a[1])*dx)/length
        if -.01<t<1.01 and side<ramp["width"]/2+50 and abs(h-branch_ramp_point(ramp,t)[2])<35:return True
    # 경사 접합 외의 저작된 가지/정상 상면과 목을 공개한다.
    for s in CROWN_SPACES:
        if abs(h-s["local"][2])>=35:continue
        if math.hypot(x-s["local"][0],y-s["local"][1])<s["radius"]+120:return True
        if s["id"].endswith("Summit"):continue
        tree=TREE_BY_ID[s["tree"]];dx,dy=x-tree["xy"][0],y-tree["xy"][1];ux,uy=s["direction"]
        along=dx*ux+dy*uy;across=abs(-dx*uy+dy*ux)
        if tree["radius"]*.25-50<along<math.dist(tree["xy"],s["local"][:2]) and across<700:return True
    return False

ROUTES=[
    {"id":"AtoBWest","xy":[[-12500,-2500],[-17000,-3500],[-18000,-14500],[-16000,-19000],[-6000,-18500],[1000,-12000]]},
    {"id":"AtoBEast","xy":[[-12500,-2500]]+crossing("R8",.49,-1)+crossing("R2",.45,1)+[[1000,-12000]]},
    {"id":"AtoRecovery","xy":[[-12500,-2500]]+crossing("R8",.28,-1)+[[-9000,4000]]},
    {"id":"RecoveryToC","xy":[[-9000,4000]]+crossing("R6",.56,1)+[[4500,8500]]},
    {"id":"BtoBoss","xy":[[1000,-12000]]+crossing("R3",.65,-1)+crossing("R5",.52,-1)+[[10500,4000]]},
    {"id":"BossToC","xy":[[10500,4000],[4500,8500]]},
    {"id":"CtoE","xy":[[4500,8500],[0,18500],[-7000,20000],[-12500,14500]]},
    {"id":"EtoA","xy":[[-12500,14500],[-17500,15500],[-18500,6500],[-16500,0],[-12500,-2500]]}]

def placement():
    nodes=[{"id":c["id"],"local":[*c["xy"],ground(*c["xy"])]} for c in CLEARINGS]
    nodes.extend([{"id":"Recovery","local":[-9000,4000,ground(-9000,4000)]},
                  {"id":"SurfaceJoint","local":[-14896,1765,ground(-14896,1765)+10]},
                  {"id":"RootVault","local":[*VAULT_XY,ground(*VAULT_XY)+10]},
                  {"id":"EastGround","local":[15500,-7500,ground(15500,-7500)]}])
    for name,root_id,t,lateral in (("RootCrown","R5",.5,0),("EastCrown","R4",.5,0),
            ("RootNotchWest","R6",.41,0),("RootBreachWest","R8",.35,0)):
        root=ROOT_BY_ID[root_id]
        nodes.append({"id":name,"local":[*root_xy(root,t,lateral),root_top(root,t)]})
    root=ROOT_BY_ID["R1"]
    xy=root_xy(root,.70,1400)
    nodes.append({"id":"RootZStart","local":[*xy,cut_height(root,.70,1400,0)]})
    root=ROOT_BY_ID["R3"]
    xy=root_xy(root,.55,root["base_width"]/2)
    nodes.append({"id":"RootCrossRamp","local":[*xy,ground(*xy)+10]})
    by_id={n["id"]:n["local"] for n in nodes}
    pods=[{"id":c["id"],"local":by_id[c["id"]]} for c in CLEARINGS]
    pods.extend([{"id":"RootVault","local":by_id["RootVault"],"headroom":600},
                 {"id":"RootCrown","local":by_id["RootCrown"]}])
    anchors=[]
    def anchor(name,source,target,max_distance=12000):
        anchors.append({"id":name,"from":source,"local":target,"max_distance":max_distance})
    r=ROOT_BY_ID["R5"]
    landing=[*root_xy(r,.5,400),root_top(r,.5)]
    anchor("CrownArrival","BossClearing",[landing[0],landing[1],landing[2]+1800])
    anchors[-1]['landing']=landing
    anchors[-1]['landing_root']='R5'
    r=ROOT_BY_ID["R4"]
    landing=[*root_xy(r,.5,-300),root_top(r,.5)]
    anchor("EastArrival","EastGround",[landing[0],landing[1],landing[2]+1600])
    anchors[-1]['landing']=landing
    anchors[-1]['landing_root']='R4'
    for name,source,root_id,t in (("NotchCross","RootNotchWest","R6",.69),("BreachCross","RootBreachWest","R8",.65)):
        r=ROOT_BY_ID[root_id]
        landing=[*root_xy(r,t),root_top(r,t)]
        anchor(name,source,[landing[0],landing[1],landing[2]+1200])
        anchors[-1]['landing']=landing
        anchors[-1]['landing_root']=root_id
    anchor("BasinReturn","Recovery",[*VAULT_XY,ground(*VAULT_XY)+550],7500)
    anchors[-1]['landing']=[*VAULT_XY,ground(*VAULT_XY)+10]
    nodes.append({"id":"UndergroundHub","local":[*UNDERGROUND["xy"],UNDERGROUND["floor"]]})
    pods.append({"id":"UndergroundHub","local":[*UNDERGROUND["xy"],UNDERGROUND["floor"]],"headroom":4000})
    for entrance in UNDERGROUND["entrances"]:
        name="Underground"+entrance["id"]
        if entrance["type"]=="Ramp":
            xy=underground_xy(entrance,.02)
            nodes.append({"id":name,"local":[*xy,underground_floor(entrance,.02)]})
            continue
        surface=shaft_point(entrance,2300,0);surface[2]=ground(*surface[:2])+10
        lip=shaft_point(entrance,1050,0);lip[2]=ground(*lip[:2])+10
        ledge=shaft_point(entrance,-900,-5500)
        bottom=shaft_point(entrance,900,entrance.get("floor",UNDERGROUND["floor"])-2)
        for suffix,local in (("",surface),("Lip",lip),("Ledge",ledge),("Bottom",bottom)):
            nodes.append({"id":name+suffix,"local":local})
        anchor(entrance["id"]+"ShaftLedge",name+"Bottom",[ledge[0],ledge[1],ledge[2]+3000],12000)
        anchors[-1].update({"landing":ledge,"also_from":[name+"Lip"]})
        anchor(entrance["id"]+"ShaftReturn",name+"Ledge",[surface[0],surface[1],surface[2]+2500],12000)
        anchors[-1].update({"landing":surface,"landing_ground":True})
    for tree in TREES:
        spaces=crown_spaces(tree)
        source=tree["id"]+"Ascent"
        nodes.append({"id":source,"local":ascent_start(tree)})
        for space in spaces:
            name=space["id"];local=space["local"]
            nodes.append({"id":name,"local":local})
            target=[local[0],local[1],local[2]+2000]
            anchor(name+"Arrival",source,target,12000 if source.endswith("Ascent") else 10000)
            anchors[-1]["landing"]=local
            previous=next(n["local"] for n in nodes if n["id"]==source)
            dx,dy=previous[0]-local[0],previous[1]-local[1];length=math.hypot(dx,dy)
            departure=[local[0]+dx/length*(space["radius"]-100),local[1]+dy/length*(space["radius"]-100),local[2]]
            nodes.append({"id":name+"Descent","local":departure})
            # 지면 하강은 낙하/활공으로 맡기고, 공중 발판 복귀를 겸하는 앵커만 남긴다.
            if not source.endswith("Ascent"):
                anchor(name+"Return",name+"Descent",[previous[0],previous[1],previous[2]+2000],10000)
                anchors[-1]["landing"]=previous
            source=name
    for ramp in BRANCH_RAMPS:
        nodes.append({"id":ramp["tree"]+"BranchRamp","local":branch_ramp_point(ramp,.25)})
    for name,x,space_id in (("BranchJunctionLow",100,"T1LowerBranch"),("BranchJunctionHigh",900,"T4LowerBranch")):
        space=next(s for s in CROWN_SPACES if s["id"]==space_id)
        nodes.append({"id":name,"local":[x,-4900,space["local"][2]]})
    # 같은 위치의 상승/하강 앵커를 공유하고 유지 대상의 접근점을 남긴다.
    merged=[];by_position={}
    def merge_into(keep,other):
        sources=[keep['from']]+keep.get('also_from',[])+[other['from']]+other.get('also_from',[])
        keep['also_from']=list(dict.fromkeys(sources))[1:]
        keep['merged_ids'].extend(other['merged_ids'])
        keep['max_distance']=max(keep['max_distance'],other['max_distance'])
    for a in anchors:
        a['merged_ids']=[a['id']]
        key=tuple(a['local'])
        if key in by_position:merge_into(by_position[key],a)
        else:by_position[key]=a;merged.append(a)
    # 확정 외피에서 조준·몸체 접근·착지 띠를 통과한 개별 보정값이다.
    adjustments={
        'BasinReturn':(150,0),
        'WestShaftLedge':(1500,0),'WestShaftReturn':(500,0),
        'NorthShaftLedge':(1250,0),'NorthShaftReturn':(500,0),
        'T1LowerBranchArrival':(250,800),'T1UpperBranchArrival':(1000,0),
        'T2LowerBranchArrival':(250,800),
        'T4LowerBranchArrival':(0,800),'T4UpperBranchArrival':(1250,0),
        'T5LowerBranchArrival':(1250,0),'T5UpperBranchArrival':(1000,0),
        'T3LowerBranchArrival':(500,200),'T3UpperBranchArrival':(1500,0),
        'T6LowerBranchArrival':(0,600),'T6UpperBranchArrival':(1000,0),'T6SummitArrival':(0,400),
        'T7LowerBranchArrival':(250,0)}
    by_id={n['id']:n['local'] for n in nodes}
    for a in merged:
        raise_cm,shift=adjustments.get(a['id'],(0,0))
        if shift:
            start=by_id[a['from']];dx,dy=start[0]-a['local'][0],start[1]-a['local'][1];length=math.hypot(dx,dy)
            a['local'][0]+=dx/length*shift;a['local'][1]+=dy/length*shift
        a['local'][2]+=raise_cm
    return {"nodes":nodes,"pods":pods,"anchors":merged,"routes":ROUTES}
