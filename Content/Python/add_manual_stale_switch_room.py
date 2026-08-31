"""Add only the manual room actor to the existing independent Lab; never recreate it."""
import unreal

MAP='/Game/Maps/L_ProjectFogPropGameplayLab'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(MAP)
before=actors.get_all_level_actors()
rooms=[a for a in before if isinstance(a,unreal.DarkwellManualStaleRoom)]
assert len(rooms)<=1
if not rooms:
    room=actors.spawn_actor_from_class(unreal.DarkwellManualStaleRoom,unreal.Vector(4000,0,0))
    room.set_actor_label('MANUAL STALE - two rooms and pressure switch')
else:room=rooms[0]
# Explicitly update only this new room's native bindings if a prior preview was
# saved. The accepted camera maps +X to screen-left; the corridor is at -X.
centers=[(-25,0,-15),(-25,800,125),(-25,-800,125),(950,0,125),(125,0,125),(-700,-710,125),(-700,0,125),(-700,710,125),(-1000,0,125)]
positions={f'RoomStructure{i}':p for i,p in enumerate(centers)}
positions.update(PressureSwitch=(500,-450,3),EditorCabinetPreview=(500,500,95))
for part in room.get_components_by_class(unreal.StaticMeshComponent):
    if part.get_name() in positions:
        part.set_relative_location(unreal.Vector(*positions[part.get_name()]),False,False)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(4000,-1100,2000),unreal.Rotator(pitch=-65,yaw=90,roll=0))
assert len([a for a in actors.get_all_level_actors() if isinstance(a,unreal.DarkwellPropLabFurniture)])==25
assert len([a for a in actors.get_all_level_actors() if isinstance(a,unreal.DarkwellStalkerCharacter)])==0
assert levels.save_current_level()
unreal.log('MANUAL_ROOM_ASSET_PASS originalFurniture=25 addedRoom=1 map='+MAP)
