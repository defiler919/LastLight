"""Idempotently create/update only /Game/Maps/L_SightWeaveGrayPolicyLab.

Run through Unreal Editor Python after the DarkwellEditor target is built.
The six rooms, controls, Chinese UI, fixtures, and stress content are generated
at runtime by the native Director so the binary map remains deliberately small.
"""

import unreal


MAP = "/Game/Maps/L_SightWeaveGrayPolicyLab"


def _actors_of_exact_class(actor_subsystem, actor_class):
    return [
        actor
        for actor in actor_subsystem.get_all_level_actors()
        if actor.get_class() == actor_class
    ]


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets = unreal.EditorAssetLibrary

    if assets.does_asset_exist(MAP):
        if not levels.load_level(MAP):
            raise RuntimeError("Unable to load existing gray policy lab map")
    else:
        if not levels.new_level(MAP):
            raise RuntimeError("Unable to create gray policy lab map")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_outermost().get_name() != MAP:
        raise RuntimeError("Editor is not on the requested target map")

    world.get_world_settings().set_editor_property(
        "default_game_mode", unreal.DarkwellVisionIntegrationGameMode
    )

    directors = _actors_of_exact_class(
        actors, unreal.DarkwellSightWeaveGrayPolicyLabDirector.static_class()
    )
    if not directors:
        director = actors.spawn_actor_from_class(
            unreal.DarkwellSightWeaveGrayPolicyLabDirector,
            unreal.Vector(0, 0, 0),
            unreal.Rotator(0, 0, 0),
        )
        director.set_actor_label("GRAY POLICY LAB V2 - native runtime director")
    else:
        director = directors[0]
        for duplicate in directors[1:]:
            actors.destroy_actor(duplicate)
    director.set_actor_location(unreal.Vector(0, 0, 0), False, False)

    starts = _actors_of_exact_class(actors, unreal.PlayerStart.static_class())
    if not starts:
        start = actors.spawn_actor_from_class(
            unreal.PlayerStart,
            unreal.Vector(0, -500, 92),
            unreal.Rotator(0, 90, 0),
        )
    else:
        start = starts[0]
        start.set_actor_location(unreal.Vector(0, -500, 92), False, False)
        start.set_actor_rotation(unreal.Rotator(0, 90, 0), False)
        for duplicate in starts[1:]:
            actors.destroy_actor(duplicate)
    start.set_actor_label("大厅玩家起点（PIE 默认停止）")

    if not levels.save_current_level():
        raise RuntimeError("Failed to save gray policy lab map")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    unreal.log(
        "GRAY_POLICY_LAB_V2_READY map={} director=1 player_starts=1".format(MAP)
    )


if __name__ == "__main__":
    main()
