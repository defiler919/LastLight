"""Create the SightWeave M1 lab through Unreal Editor asset APIs."""

import math

import unreal


MAP_DIRECTORY = "/SightWeave/Maps"
MAP_PATH = f"{MAP_DIRECTORY}/L_SightWeave_Lab"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"
CONE_PATH = "/Engine/BasicShapes/Cone.Cone"


def spawn_mesh(name, mesh, location, scale, yaw=0.0):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*location),
        unreal.Rotator(0.0, yaw, 0.0),
    )
    if actor is None:
        raise RuntimeError(f"Could not spawn fixture actor: {name}")
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def spawn_text(name, text, location, world_size=62.0, color=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.TextRenderActor,
        unreal.Vector(*location),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if actor is None:
        raise RuntimeError(f"Could not spawn label actor: {name}")
    actor.set_actor_label(name)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    if component is None:
        raise RuntimeError(f"TextRenderComponent missing on: {name}")
    component.set_editor_property("text", unreal.Text(text))
    component.set_editor_property("world_size", world_size)
    component.set_editor_property("text_render_color", color or unreal.Color(235, 235, 235, 255))
    return actor


def wall(name, cube, origin, offset, length=1100.0, yaw=0.0, height=300.0):
    x = origin[0] + offset[0]
    y = origin[1] + offset[1]
    return spawn_mesh(
        name,
        cube,
        (x, y, height * 0.5),
        (length / 100.0, 0.4, height / 100.0),
        yaw,
    )


def wedge(name, cube, cylinder, origin, local_offset, yaw):
    x = origin[0] + local_offset[0]
    y = origin[1] + local_offset[1]
    spawn_mesh(f"{name}_Origin", cylinder, (x, y, 16.0), (0.6, 0.6, 0.16))
    for angle in (-24.0, 24.0):
        radians = math.radians(yaw + angle)
        center_x = x + math.cos(radians) * 500.0
        center_y = y + math.sin(radians) * 500.0
        spawn_mesh(
            f"{name}_Boundary_{int(angle)}",
            cube,
            (center_x, center_y, 24.0),
            (10.0, 0.12, 0.12),
            yaw + angle,
        )


def create_lab():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    unreal_editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if level_editor is None or unreal_editor is None or editor_actors is None:
        raise RuntimeError("Required Unreal editor subsystems are unavailable")

    unreal.EditorAssetLibrary.make_directory(MAP_DIRECTORY)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not level_editor.load_level(MAP_PATH):
            raise RuntimeError(f"Could not load existing lab map: {MAP_PATH}")
        existing_actors = editor_actors.get_all_level_actors()
        if existing_actors and not editor_actors.destroy_actors(existing_actors):
            raise RuntimeError(f"Could not clear existing lab fixtures: {MAP_PATH}")
    elif not level_editor.new_level(MAP_PATH):
        raise RuntimeError(f"Could not create level: {MAP_PATH}")

    cube = unreal.load_asset(CUBE_PATH)
    cylinder = unreal.load_asset(CYLINDER_PATH)
    cone = unreal.load_asset(CONE_PATH)
    if not cube or not cylinder or not cone:
        raise RuntimeError("Required Engine basic-shape assets are unavailable")

    world = unreal_editor.get_editor_world()
    if world is None:
        raise RuntimeError("Editor world is unavailable after creating the lab")
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", unreal.GameModeBase)

    origins = []
    for row in range(3):
        for column in range(4):
            origins.append((-4800.0 + column * 3200.0, -3200.0 + row * 3200.0))

    labels = (
        "01 STRAIGHT WALL",
        "02 DIAGONAL WALL",
        "03 RIGHT-ANGLE CORNER",
        "04 CLOSED ROOM",
        "05 DOOR OPENING",
        "06 ROTATING DOOR PLACEHOLDER",
        "07 HEIGHT BANDS",
        "08 OVERLAPPING FLOOR IDS",
        "09 VISION + LEGAL LIGHT",
        "10 VISIBLE / INFRARED ISOLATION",
        "11 BODY-CIRCLE BYPASS",
        "12 TWO REMOTE CONES",
    )

    for index, origin in enumerate(origins):
        spawn_mesh(
            f"SW_Zone_{index + 1:02d}_Base",
            cube,
            (origin[0], origin[1], -18.0),
            (28.0, 28.0, 0.18),
        )
        spawn_text(
            f"SW_Zone_{index + 1:02d}_Label",
            labels[index],
            (origin[0] - 1300.0, origin[1] - 1200.0, 120.0),
            52.0,
            unreal.Color(150, 220, 255, 255),
        )

    # 01: stable straight-wall fixture.
    wall("SW_01_StraightWall", cube, origins[0], (0.0, 150.0), length=1900.0)

    # 02: diagonal-wall fixture.
    wall("SW_02_DiagonalWall", cube, origins[1], (0.0, 150.0), length=1900.0, yaw=35.0)

    # 03: right-angle corner.
    wall("SW_03_Corner_A", cube, origins[2], (-450.0, 150.0), length=1000.0)
    wall("SW_03_Corner_B", cube, origins[2], (50.0, 650.0), length=1000.0, yaw=90.0)

    # 04: closed room.
    for suffix, offset, length, yaw in (
        ("North", (0.0, 850.0), 1700.0, 0.0),
        ("South", (0.0, -150.0), 1700.0, 0.0),
        ("East", (850.0, 350.0), 1000.0, 90.0),
        ("West", (-850.0, 350.0), 1000.0, 90.0),
    ):
        wall(f"SW_04_Room_{suffix}", cube, origins[3], offset, length=length, yaw=yaw)

    # 05: doorway with a deliberate 400 cm opening.
    wall("SW_05_Doorway_Left", cube, origins[4], (-650.0, 250.0), length=900.0)
    wall("SW_05_Doorway_Right", cube, origins[4], (650.0, 250.0), length=900.0)
    spawn_text("SW_05_Opening_Label", "400 cm OPENING", (origins[4][0] - 200.0, origins[4][1] + 400.0, 120.0), 42.0)

    # 06: rotating-door placeholder at an inspectable angle.
    wall("SW_06_Frame_Left", cube, origins[5], (-850.0, 250.0), length=500.0)
    wall("SW_06_Frame_Right", cube, origins[5], (850.0, 250.0), length=500.0)
    wall("SW_06_RotatingDoor", cube, origins[5], (0.0, 250.0), length=1000.0, yaw=32.0, height=260.0)
    spawn_text("SW_06_NoAlgorithm_Label", "M1 FIXTURE - NO DOOR SOLVE", (origins[5][0] - 500.0, origins[5][1] + 850.0, 120.0), 38.0)

    # 07: three height ranges.
    for suffix, x_offset, height in (("Low", -700.0, 100.0), ("Mid", 0.0, 300.0), ("High", 700.0, 650.0)):
        spawn_mesh(
            f"SW_07_{suffix}Height",
            cube,
            (origins[6][0] + x_offset, origins[6][1] + 250.0, height * 0.5),
            (4.0, 4.0, height / 100.0),
        )
        spawn_text(
            f"SW_07_{suffix}Label",
            f"{int(height)} cm",
            (origins[6][0] + x_offset - 160.0, origins[6][1] + 520.0, height + 40.0),
            34.0,
        )

    # 08: same XY footprint, separate vertical floor fixtures.
    spawn_mesh("SW_08_Floor_A", cube, (origins[7][0], origins[7][1] + 250.0, 40.0), (16.0, 16.0, 0.4))
    spawn_mesh("SW_08_Floor_B", cube, (origins[7][0], origins[7][1] + 250.0, 540.0), (12.0, 12.0, 0.4))
    spawn_text("SW_08_Floor_A_Label", "FloorId A", (origins[7][0] - 500.0, origins[7][1] + 850.0, 150.0), 42.0)
    spawn_text("SW_08_Floor_B_Label", "FloorId B (same XY)", (origins[7][0] - 650.0, origins[7][1] + 850.0, 650.0), 42.0)

    # 09: overlapping source markers, explicitly not a solved result.
    spawn_mesh("SW_09_VisionMarker", cylinder, (origins[8][0] - 300.0, origins[8][1] + 250.0, 20.0), (8.0, 8.0, 0.2))
    spawn_mesh("SW_09_LegalIlluminationMarker", cylinder, (origins[8][0] + 300.0, origins[8][1] + 250.0, 24.0), (8.0, 8.0, 0.24))
    spawn_text("SW_09_Vision_Label", "VISION", (origins[8][0] - 700.0, origins[8][1] + 850.0, 110.0), 42.0)
    spawn_text("SW_09_Light_Label", "LEGAL ILLUMINATION", (origins[8][0] + 50.0, origins[8][1] + 850.0, 110.0), 42.0)

    # 10: visible/infrared compatibility lanes stay visibly separate.
    spawn_mesh("SW_10_VisibleOnly", cone, (origins[9][0] - 500.0, origins[9][1] + 250.0, 100.0), (4.0, 4.0, 2.0))
    spawn_mesh("SW_10_InfraredOnly", cone, (origins[9][0] + 500.0, origins[9][1] + 250.0, 100.0), (4.0, 4.0, 2.0))
    spawn_text("SW_10_Visible_Label", "VISIBLE ONLY", (origins[9][0] - 900.0, origins[9][1] + 850.0, 120.0), 38.0, unreal.Color(255, 220, 100, 255))
    spawn_text("SW_10_Infrared_Label", "INFRARED ONLY", (origins[9][0] + 150.0, origins[9][1] + 850.0, 120.0), 38.0, unreal.Color(255, 100, 100, 255))

    # 11: permanent close body-circle bypass fixture.
    spawn_mesh("SW_11_BodyCircle", cylinder, (origins[10][0], origins[10][1] + 250.0, 18.0), (9.0, 9.0, 0.18))
    spawn_text("SW_11_Bypass_Label", "BYPASS LEGAL ILLUMINATION", (origins[10][0] - 850.0, origins[10][1] + 850.0, 120.0), 38.0)

    # 12: two explicitly separate remote cone fixtures.
    wedge("SW_12_RemoteCone_A", cube, cylinder, origins[11], (-650.0, 150.0), 20.0)
    wedge("SW_12_RemoteCone_B", cube, cylinder, origins[11], (650.0, 150.0), 160.0)
    spawn_text("SW_12_Remote_Label", "REMOTE A + REMOTE B", (origins[11][0] - 600.0, origins[11][1] + 1000.0, 120.0), 40.0)

    spawn_text(
        "SW_Lab_Title",
        "SIGHTWEAVE M1 LAB",
        (-1700.0, -5200.0, 260.0),
        110.0,
        unreal.Color(110, 210, 255, 255),
    )
    spawn_text(
        "SW_Lab_Disclaimer",
        "STABLE TEST FIXTURES ONLY - NO FINAL VISIBILITY / FOG EFFECT",
        (-3000.0, -4900.0, 160.0),
        54.0,
        unreal.Color(255, 170, 80, 255),
    )

    directional_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 3000.0),
        unreal.Rotator(-45.0, -35.0, 0.0),
    )
    directional_light.set_actor_label("SW_Lab_NeutralDirectionalLight")
    sky_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 1000.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    sky_light.set_actor_label("SW_Lab_NeutralSkyLight")
    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(0.0, -5600.0, 120.0),
        unreal.Rotator(0.0, 90.0, 0.0),
    )
    player_start.set_actor_label("SW_Lab_PlayerStart")

    if not level_editor.save_current_level():
        raise RuntimeError(f"Could not save level: {MAP_PATH}")
    if not unreal.EditorAssetLibrary.save_asset(MAP_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not persist level asset: {MAP_PATH}")
    unreal.log(f"SightWeave created and saved {MAP_PATH}")


if __name__ == "__main__":
    create_lab()
