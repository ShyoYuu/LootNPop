"""면 교차 검토용 오프라인 투영 화면 6개를 만든다. UE 캡처로 사용하지 않는다."""
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import ReviewRidgeBodyIntersections as review


def main():
    data=review.read(review.OUT/'PairReviewData.json')['data']
    scene=bpy.context.scene
    scene.render.engine='BLENDER_WORKBENCH'
    scene.render.resolution_x=600;scene.render.resolution_y=600;scene.render.resolution_percentage=100
    shading=scene.display.shading
    shading.light='STUDIO';shading.color_type='MATERIAL';shading.show_shadows=False
    shading.show_cavity=True;shading.show_specular_highlight=False
    shading.background_type='WORLD';scene.world.color=(.04,.05,.07)
    camera_data=bpy.data.cameras.new('ReviewCamera');camera_data.type='ORTHO'
    camera=bpy.data.objects.new('ReviewCamera',camera_data);scene.collection.objects.link(camera);scene.camera=camera
    materials=[]
    for name,color in [('FaceA',(.9,.12,.13,1)),('FaceB',(.08,.4,.95,1))]:
        material=bpy.data.materials.new(name);material.diffuse_color=color;materials.append(material)
    records=[]
    for body in dict.fromkeys(row['body'] for row in data):
        row=max((r for r in data if r['body']==body and not r['shared']),key=lambda r:r['length_cm'])
        points=np.asarray(row['before']).reshape(-1,3);center=points.mean(0)
        tri=np.asarray(row['before'][0]);n=np.cross(tri[1]-tri[0],tri[2]-tri[0]);n/=np.linalg.norm(n)
        axis=tri[1]-tri[0];axis/=np.linalg.norm(axis)
        view=n*.8+axis*.6;view/=np.linalg.norm(view)
        camera.location=Vector(view*100)
        camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
        camera_data.ortho_scale=float(np.ptp(np.asarray(row['before']+row['after']).reshape(-1,3),axis=0).max()*1.8)
        for phase in ('before','after'):
            for obj in list(scene.objects):
                if obj.type=='MESH':bpy.data.objects.remove(obj,do_unlink=True)
            for i,triangle in enumerate(row[phase]):
                mesh=bpy.data.meshes.new('ReviewFace');mesh.from_pydata((np.asarray(triangle)-center).tolist(),[],[(0,1,2)])
                mesh.materials.append(materials[i]);obj=bpy.data.objects.new('ReviewFace',mesh);scene.collection.objects.link(obj)
            label=body.split('_')[-2]
            path=review.OUT/f'Pair_{label}_{row["faces"][0]}_{row["faces"][1]}_{phase}.png'
            scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
            records.append({'body':body,'faces':row['faces'],'phase':phase,'file':path.relative_to(review.ROOT).as_posix(),
                            'camera':'same orthographic camera and scale for each before/after pair','type':'offline triangle diagnostic; not Unreal viewport'})
    review.write('PairRenderMetadata.json',records)


if __name__=='__main__':main()
