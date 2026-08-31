import unreal

MAP = "/Game/Maps/L_ProjectFogPropGameplayLab"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(MAP)
furniture = []
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.DarkwellStalkerCharacter):
        raise AssertionError('Default lab must not contain a placed Stalker')
    if isinstance(a, unreal.DarkwellPropLabFurniture):
        furniture.append(a)
        unreal.log(f"LAB_ASSET {a.get_editor_property('stable_id')} shape={a.get_editor_property('shape')} dimensions={a.get_editor_property('dimensions')}")
    if isinstance(a, unreal.NavMeshBoundsVolume):
        unreal.log("LAB_NAV " + str(a.get_actor_bounds(False)))
assert len(furniture) == 25
ids = [str(a.get_editor_property("stable_id")) for a in furniture]
assert len(set(ids)) == 25
assert sum("Row8" in i for i in ids) == 8
assert sum("Row4" in i for i in ids) == 4
navdata = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.RecastNavMesh)]
if not navdata:
    navdata = [actors.spawn_actor_from_class(unreal.RecastNavMesh, unreal.Vector())]
for nav in navdata:
    nav.set_editor_property('runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
unreal.log('LAB_RECAST_DATA_COUNT=' + str(len(navdata)))
levels.save_current_level()
unreal.log("LAB_ASSET_AUDIT_PASS uniqueIds=25 row8=8 row4=4")
