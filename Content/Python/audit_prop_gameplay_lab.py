import unreal

MAP = "/Game/Maps/L_ProjectFogPropGameplayLab"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(MAP)
furniture = []
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.DarkwellStalkerCharacter):
        a.set_editor_property("persistent_id", "Lab.Stalker.NeverRemember")
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
levels.save_current_level()
unreal.log("LAB_ASSET_AUDIT_PASS uniqueIds=25 row8=8 row4=4")
