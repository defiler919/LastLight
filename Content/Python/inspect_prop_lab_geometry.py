"""Read-only editor geometry audit, including cross-actor coplanar exterior faces."""
import itertools
import json
import re
from pathlib import Path
import unreal

MAP='/Game/Maps/L_ProjectFogPropGameplayLab'
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
rows=[]
def xyz(v): return [v.x,v.y,v.z]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(actor,(unreal.DarkwellPropLabFurniture,unreal.DarkwellPropGameplayLab)): continue
    identity=str(actor.get_editor_property('stable_id')) if isinstance(actor,unreal.DarkwellPropLabFurniture) else 'STRUCTURE (no furniture StableID)'
    for part in actor.get_components_by_class(unreal.StaticMeshComponent):
        origin,extent,radius=unreal.SystemLibrary.get_component_bounds(part)
        rows.append(dict(actor=actor.get_actor_label(),identity=identity,part=part.get_name(),visible=part.is_visible(),
            location=xyz(part.get_world_location()),scale=xyz(part.get_world_scale()),rotation=str(part.get_world_rotation()),
            bounds_min=xyz(origin-extent),bounds_max=xyz(origin+extent),radius=radius,
            materials=[m.get_path_name() if m else None for m in part.get_materials()]))
pairs=[]
for a,b in itertools.combinations([r for r in rows if r['visible']],2):
    lo=[max(a['bounds_min'][i],b['bounds_min'][i]) for i in range(3)]
    hi=[min(a['bounds_max'][i],b['bounds_max'][i]) for i in range(3)]
    overlap=[max(0,hi[i]-lo[i]) for i in range(3)]
    volume=overlap[0]*overlap[1]*overlap[2]
    coplanar=[]
    for axis in range(3):
        area=overlap[(axis+1)%3]*overlap[(axis+2)%3]
        for side in ('min','max'):
            if area>0.1 and abs(a['bounds_'+side][axis]-b['bounds_'+side][axis])<0.01:
                coplanar.append(dict(axis=axis,side=side,area_cm2=area))
    if volume>0.1 or coplanar:
        pairs.append(dict(a=a['actor']+'/'+a['part'],b=b['actor']+'/'+b['part'],intersection_cm3=volume,coplanar_exterior=coplanar))
label=re.search(r'-PropLabAudit=(\w+)',unreal.SystemLibrary.get_command_line())
label=label[1] if label else 'Geometry'
target=Path(unreal.Paths.project_saved_dir())/'PropGameplayLab'/(label+'.json')
target.write_text(json.dumps(dict(map=MAP,primitives=rows,intersections=pairs),indent=2),encoding='utf-8')
unreal.log('LAB_GEOMETRY_AUDIT '+str(target)+' primitives='+str(len(rows)))
for pair in pairs:
    unreal.log('LAB_GEOMETRY_PAIR '+json.dumps(pair))
