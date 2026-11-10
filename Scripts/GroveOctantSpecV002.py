"""Grove V002 대표 구역. 단위 cm, H는 구 중심 쪽으로 증가한다."""
import math

VERSION="V018"
RADIUS=30000.0
SEAM_MARGIN=1500.0
SEAM_BAND=4000.0
GROUND=-1000.0
SEGMENTS=32
CLEARINGS=[
    {"id":"ClearingA","xy":[-12500,-2500],"h":-1000},
    {"id":"ClearingB","xy":[1000,-12000],"h":-1000},
    {"id":"ClearingC","xy":[4500,8500],"h":-1400},
    {"id":"ClearingE","xy":[-12500,14500],"h":-1000},
    {"id":"BossClearing","xy":[10500,2000],"h":-1200}]

def point(x,y,h):
    raw=(RADIUS/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2),RADIUS/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2),RADIUS/math.sqrt(3)+2*x/math.sqrt(6))
    scale=(RADIUS-h)/math.sqrt(sum(v*v for v in raw))
    return tuple(v*scale for v in raw)

def ground(x,y):
    envelope=max(0,min(1,(min(point(x,y,0))-5500)/3500))
    waves=450*math.sin(x/4200+.7)*math.cos(y/5000-.5)+320*math.sin((x+.5*y)/2800+1)+180*math.cos((x-y)/3500)
    h=GROUND+waves*envelope*envelope*(3-2*envelope)
    basin=math.exp(-((x+9000)/4200)**2-((y-4000)/4200)**2)
    h+=( -2600-h)*basin
    # 지하 방의 지붕과 지각이 만나는 전체 패치는 같은 높이를 공유한다.
    dx=max(-11800-x,0,x+4800)
    dy=max(-10800-y,0,y+5200)
    t=max(0,min(1,math.hypot(dx,dy)/1800))
    h=GROUND+(h-GROUND)*t*t*(3-2*t)
    for clearing in CLEARINGS:
        xx,yy=clearing["xy"]
        t=max(0,min(1,(math.hypot(x-xx,y-yy)-1400)/1800))
        h=clearing["h"]+(h-clearing["h"])*t*t*(3-2*t)
    return h

TREES=[
    {"id":"T1_West","xy":[-7000,-7500],"radius":2300,"height":11000,"roots":[[20,3500],[135,2800],[250,2600]],"fork":25},
    {"id":"T2_Central","xy":[0,0],"radius":3000,"height":14000,"roots":[[0,4800],[120,4000],[240,4000]],"fork":105},
    {"id":"T4_Northwest","xy":[7500,-8000],"radius":2200,"height":12500,"roots":[[10,3000],[145,3600],[265,3000]],"fork":155},
    {"id":"T5_BossLandmark","xy":[17500,0],"radius":3200,"height":17000,"roots":[[20,3200],[140,3600],[260,3200]],"fork":65},
    {"id":"T3_East","xy":[-5000,11000],"radius":2100,"height":12000,"roots":[[20,3800],[145,3000],[265,3000]],"fork":35},
    {"id":"T6_Southwest","xy":[-13000,-15000],"radius":1700,"height":10000,"roots":[[0,4000],[90,4000],[230,800]],"fork":125},
    {"id":"T7_Southeast","xy":[-13700,7500],"radius":1900,"height":10500,"roots":[[0,3800],[95,2000],[260,2300]],"fork":205},
]
ROOT_WALLS=[
    {"id":"Wall_T1","from":[-7000,-7500],"to":[-5000,-1500],"height":1100,"ramp":2600},
    {"id":"Wall_T2","from":[0,0],"to":[-1000,7000],"height":1300,"ramp":1650,"style":"Switchback"},
    {"id":"Wall_T4","from":[7500,-8000],"to":[8500,-1000],"height":1100,"ramp":2600},
    {"id":"Wall_T5","from":[17500,0],"to":[13500,6200],"height":1400,"ramp":2800},
    {"id":"Wall_T3","from":[-5000,11000],"to":[-11000,13500],"height":1100,"ramp":1650,"style":"Switchback"},
    {"id":"Wall_T6","from":[-13000,-15000],"to":[-6000,-14500],"height":1000,"ramp":2600},
    {"id":"Wall_T7","from":[-13700,7500],"to":[-7000,6500],"height":1100,"ramp":1650,"style":"Switchback"}]
BRANCH={"id":"CentralBranch","outline":[[-600,-1700],[-1900,-1800],[-4600,-4600],[-4800,-6200],[-3500,-7200],[-1900,-6400],[-1700,-4300]],"floor":2600,"bottom":1700,"pod":[-3200,-5500,2600]}
CAVE={"id":"RootVault","outer":[-11800,-4800,-10800,-5200,-5100,GROUND],"chamber":[-11100,-5400,-10100,-5900,-4700,-1500],"shaft":[-11300,-9400,-9900,-7700,-4700,500],"pod":[-7200,-8000,-4700]}
NODES=[{"id":"ClearingA","xy":[-12500,-2500]},{"id":"ClearingB","xy":[1000,-12000]},
    {"id":"WestFlank","xy":[-11000,-13000]},{"id":"EastFlank","xy":[-2000,-7500]},
    {"id":"RootCrossing","xy":[-3500,-6200]},{"id":"Recovery","xy":[-9000,4000]},
    {"id":"VaultRim","xy":[-10400,-11300]},{"id":"BossApproach","xy":[10500,2000]}]
ROUTES=[
    {"id":"WestFlank","xy":[[-12500,-2500],[-14500,-6000],[-12500,-10500],[-10500,-11000],[1000,-12000]],"half_width":700},
    {"id":"EastFlank","xy":[[-12500,-2500],[-10500,500],[-4500,-3000],[-2000,-7500],[1000,-12000]],"half_width":700},
    {"id":"RecoveryReturn","xy":[[-12500,-2500],[-13500,3000],[-9000,4000],[-4000,5500]],"half_width":800},
    {"id":"BToBoss","xy":[[1000,-12000],[1000,-5000],[6000,-3000],[10500,2000]],"half_width":700},
    {"id":"BossToC","xy":[[10500,2000],[4500,8500]],"half_width":800},
    {"id":"CToE","xy":[[4500,8500],[0,18500],[-7000,20000],[-12500,14500]],"half_width":800},
    {"id":"EToA","xy":[[-12500,14500],[-17500,15500],[-18500,6500],[-16500,0],[-12500,-2500]],"half_width":700}]
# 전투 공터의 폭과 별도로 연결부의 보행 중심 띠는 4m로 검사한다.
for route in ROUTES:
    route["half_width"]=200

def connecting_walls():
    trees={t["id"].split("_")[0]:t for t in TREES}
    pairs=[("T1","T2"),("T2","T4"),("T1","T4"),("T2","T7"),("T1","T7"),
           ("T4","T6"),("T1","T6"),("T6","T7"),("T4","T5"),("T2","T5"),
           ("T5","T3"),("T2","T3"),("T3","T7")]
    result=[]
    def cross(a,b):
        return a[0]*b[1]-a[1]*b[0]
    for index,(first,last) in enumerate(pairs):
        a,b=trees[first]["xy"],trees[last]["xy"]
        d=[b[i]-a[i] for i in range(2)]
        gates=[]
        for route in ROUTES:
            for c,e in zip(route["xy"],route["xy"][1:]):
                r=[e[i]-c[i] for i in range(2)]
                den=cross(d,r)
                if abs(den)<1:
                    continue
                for offset in (-route["half_width"],0,route["half_width"]):
                    delta=[c[0]-a[0]-offset*r[1]/math.hypot(*r),c[1]-a[1]+offset*r[0]/math.hypot(*r)]
                    t,u=cross(delta,r)/den,cross(delta,d)/den
                    end_margin=(route["half_width"]+100)/math.hypot(*r)
                    if 0<t<1 and -end_margin<=u<=1+end_margin:
                        half=100*math.hypot(*r)/abs(den)
                        gates.append([max(0,t-half),min(1,t+half)])
        merged=[]
        for left,right in sorted(gates):
            if merged and left<=merged[-1][1]+.01:
                merged[-1][1]=max(right,merged[-1][1])
            else:
                merged.append([left,right])
        style="Arch" if index%2==0 else "Notch"
        result.append({"id":"Link_"+first+"_"+last,"from":a,"to":b,"height":300 if style=="Arch" else 250,
                       "half_width":50,"gates":merged,"style":style})
    return result

CONNECTING_WALLS=connecting_walls()
PODS=[{"id":"ClearingA","local":[-12500,-2500,ground(-12500,-2500)]},
    {"id":"ClearingB","local":[1000,-12000,ground(1000,-12000)]},
    {"id":"RootVault","local":CAVE["pod"]},{"id":"CentralBranch","local":BRANCH["pod"]}]
PODS.extend({"id":c["id"],"local":[*c["xy"],c["h"]]} for c in CLEARINGS if c["id"] not in ("ClearingA","ClearingB"))
ANCHORS=[
    {"id":"BranchArrival","local":[-3200,-5500,4100],"landing":[-3200,-5500],"from":"ClearingB","max_distance":12500},
    {"id":"BranchReturn","local":[-1500,-10500,3300],"landing":[-1500,-10500],"from":"CentralBranch","max_distance":10000},
    {"id":"VaultInner","local":[-10400,-8800,-2300],"landing":[-10400,-8800],"from":"RootVault","max_distance":7000},
    {"id":"VaultExit","local":[-10400,-11300,1000],"landing":[-10400,-11300],"from":"VaultInner","max_distance":6500},
    {"id":"VaultDescent","local":[-7800,-8500,-2900],"landing":[-7800,-8500],"from":"VaultInner","max_distance":5500},
    {"id":"BasinReturn","local":[-13500,3000,800],"landing":[-13500,3000],"from":"Recovery","max_distance":7500}]

def footprint(tree,a):
    return tree["radius"]+max(length*max(0,math.cos(a-math.radians(degrees)))**6 for degrees,length in tree["roots"])

def outline(tree,scale=1):
    x,y=tree["xy"]
    return [(x+scale*footprint(tree,a)*math.cos(a),y+scale*footprint(tree,a)*math.sin(a)) for a in (i*math.tau/SEGMENTS for i in range(SEGMENTS))]

def wall_outline(wall,inset=0):
    a,b=wall["from"],wall["to"]
    dx,dy=b[0]-a[0],b[1]-a[1]
    length=math.hypot(dx,dy)
    side=(-dy/length,dx/length)
    forward=(dx/length,dy/length)
    width=wall["ramp"]+250-inset
    return [(p[0]+s*side[0]*width+f*forward[0]*inset,p[1]+s*side[1]*width+f*forward[1]*inset)
            for p,s,f in ((a,-1,1),(b,-1,-1),(b,1,-1),(a,1,1))]

def placement():
    nodes=[{**n,"local":[*n["xy"],ground(*n["xy"])]} for n in NODES]
    nodes.extend({"id":p["id"],"local":p["local"]} for p in PODS if p["id"] not in {n["id"] for n in nodes})
    nodes.extend({"id":a["id"],"local":a["local"]} for a in ANCHORS if a["id"]=="VaultInner")
    for wall in ROOT_WALLS:
        if wall.get("style")=="Switchback":
            a,b=wall["from"],wall["to"]
            dx,dy=b[0]-a[0],b[1]-a[1]
            length=math.hypot(dx,dy)
            x,y=a[0]+dx*.96-dy/length*1300,a[1]+dy*.96+dx/length*1300
            nodes.append({"id":"RootZ"+wall["id"].removeprefix("Wall_"),"local":[x,y,ground(x,y)+10]})
    for wall in CONNECTING_WALLS:
        a,b=wall["from"],wall["to"]
        dx,dy=b[0]-a[0],b[1]-a[1]
        length=math.hypot(dx,dy)
        t=sum(wall["gates"][0])/2 if wall["gates"] else .5
        x,y=a[0]+dx*t-dy/length*500,a[1]+dy*t+dx/length*500
        nodes.append({"id":wall["id"],"local":[x,y,ground(x,y)]})
    return {"nodes":nodes,"pods":PODS,"anchors":ANCHORS,"routes":ROUTES}
