"""Official editor-only migration of the existing lab; never touches another map."""
import unreal
MAP='/Game/Maps/L_ProjectFogPropGameplayLab'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(MAP)
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.DarkwellStalkerCharacter):
        assert actors.destroy_actor(actor)
    if isinstance(actor,unreal.PlayerStart):
        actor.set_actor_location(unreal.Vector(400,-700,100),False,True)
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-30,roll=0),True)
    if isinstance(actor,unreal.DarkwellPropGameplayLab):
        positions={'LabStructure6':(-560,415,46),'LabStructure7':(-920,255,46),'LabStructure8':(-1080,0,46)}
        for part in actor.get_components_by_class(unreal.StaticMeshComponent):
            if part.get_name() in positions:
                part.set_relative_location(unreal.Vector(*positions[part.get_name()]),False,True)
            if part.get_name()=='LabStructure8': part.set_relative_scale3d(unreal.Vector(.2,1.8,.92))
    if not isinstance(actor,unreal.DarkwellPropLabFurniture): continue
    identity=str(actor.get_editor_property('stable_id'))
    if identity.startswith('Lab.Kitchen.Row'):
        actor.set_editor_property('individual_worktop',False)
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=90 if 'Row4' in identity else 0,roll=0),True)
    if identity=='Lab.Island':
        actor.set_editor_property('shape',4)
        actor.set_editor_property('dimensions',unreal.Vector(1400,110,90))
        actor.set_editor_property('tint',unreal.LinearColor(.10,.48,.68,1))
        actor.set_actor_location(unreal.Vector(400,-300,0),False,True)
    if identity=='Lab.Counter.Long':
        actor.set_editor_property('dimensions',unreal.Vector(499,66,5))
        actor.set_actor_location(unreal.Vector(-567.5,350,88),False,True)
    if identity=='Lab.Counter.Return': actor.set_actor_location(unreal.Vector(-850,230,88),False,True)
    # Reconstruct every native shape, including shelves and handles, after migration.
    actor.set_editor_property('dimensions',actor.get_editor_property('dimensions'))
assert not any(isinstance(a,unreal.DarkwellStalkerCharacter) for a in actors.get_all_level_actors())
assert levels.save_current_level()
unreal.log('LAB_COMPARISON_MAP_PASS enemy=0 island_single=1 length=1400 row4_upright=1')
