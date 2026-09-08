"""Create only the independent manual map, through Unreal's asset APIs."""
import unreal
path = "/Game/Maps/L_BlackRegionLab"
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError("Map already exists; do not replace an authored map")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.new_level(path)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.DarkwellVisionIntegrationGameMode)
fixture = actors.spawn_actor_from_class(unreal.DarkwellCleanBlackRegionLab, unreal.Vector())
fixture.set_actor_label("BLACK REGION - static manual fixture")
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-200,-130,100), unreal.Rotator(pitch=0,yaw=-90,roll=0))
levels.save_current_level()
unreal.log("CLEAN_BLACK_LAB_MAP_CREATED")
unreal.SystemLibrary.quit_editor()
