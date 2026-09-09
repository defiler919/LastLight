"""Create the independent map using Unreal asset APIs; never replace authored maps."""
import unreal
path = '/Game/Maps/L_SightWeaveApartmentLab'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Apartment map already exists; refusing replacement')
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.new_level(path)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.DarkwellVisionIntegrationGameMode)
fixture = actors.spawn_actor_from_class(unreal.DarkwellApartmentLab, unreal.Vector())
fixture.set_actor_label('SIGHTWEAVE APARTMENT | 12x10m | native manual fixture')
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0,-350,100), unreal.Rotator(yaw=90))
assert levels.save_current_level()
unreal.log('APARTMENT_MAP_CREATED')
unreal.SystemLibrary.quit_editor()
