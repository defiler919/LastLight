"""Idempotently create the component-backed SightWeave M2/M3 lab through Unreal asset APIs."""

import math
import os
import unreal

MAP_DIRECTORY = "/SightWeave/Maps"
MAP_PATH = f"{MAP_DIRECTORY}/L_SightWeave_Lab"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"
CONE_PATH = "/Engine/BasicShapes/Cone.Cone"
GROUND = "Ground"
LOCAL_OWNER = "Local"
M3P4_ACTIVE_CAMERA = os.environ.get("SIGHTWEAVE_M3P4_LAB_CAMERA", "Overview")
M3P4_DYNAMIC_DOOR_YAW = float(os.environ.get("SIGHTWEAVE_M3P4_DOOR_YAW", "28.0"))
M3P5_ACTIVE_CAMERA = os.environ.get("SIGHTWEAVE_M3P5_LAB_CAMERA", "Overview")


def floor_id(value):
    result = unreal.SightWeaveFloorId()
    result.set_editor_property("value", unreal.Name(value))
    return result


def owner_id(value=LOCAL_OWNER):
    result = unreal.SightWeaveKnowledgeOwnerId()
    result.set_editor_property("value", unreal.Name(value))
    return result


def height_range(z_min=0.0, z_max=300.0):
    result = unreal.SightWeaveHeightRange()
    result.set_editor_property("z_min", z_min)
    result.set_editor_property("z_max", z_max)
    return result


def spawn_actor(actor_class, name, location, yaw=0.0):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
    )
    if actor is None:
        raise RuntimeError(f"Could not spawn actor: {name}")
    actor.set_actor_label(name)
    actor.tags = [unreal.Name("SightWeaveM2Lab")]
    return actor


def spawn_mesh(name, mesh, location, scale, yaw=0.0):
    actor = spawn_actor(unreal.StaticMeshActor, name, location, yaw)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def spawn_text(name, text, location, world_size=48.0, color=None):
    actor = spawn_actor(unreal.TextRenderActor, name, location)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_editor_property("text", unreal.Text(text))
    component.set_editor_property("world_size", world_size)
    component.set_editor_property("text_render_color", color or unreal.Color(235, 235, 235, 255))
    return actor


def spawn_camera(name, location, yaw=0.0, ortho_width=10000.0, auto_activate=False):
    actor = spawn_actor(unreal.CameraActor, name, location, yaw)
    actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw), False)
    component = actor.get_component_by_class(unreal.CameraComponent)
    component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    component.set_editor_property("ortho_width", ortho_width)
    if auto_activate:
        actor.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    return actor


def add_floor(name, location, name_id, bounds_min, bounds_max, z_min, z_max, active):
    actor = spawn_actor(unreal.SightWeaveFloorActor, name, location)
    component = actor.get_component_by_class(unreal.SightWeaveFloorComponent)
    definition = component.get_editor_property("definition")
    definition.set_editor_property("floor_id", floor_id(name_id))
    definition.set_editor_property("bounds_min", unreal.Vector2D(*bounds_min))
    definition.set_editor_property("bounds_max", unreal.Vector2D(*bounds_max))
    definition.set_editor_property("height_range", height_range(z_min, z_max))
    definition.set_editor_property("enabled", True)
    definition.set_editor_property("active_for_queries", active)
    component.set_editor_property("definition", definition)
    if not component.refresh_floor_registration():
        raise RuntimeError(f"Could not register floor {name}")
    return actor


def add_occluder(name, location, points, yaw=0.0, z_min=0.0, z_max=300.0,
                  dynamic=False, closed=False, merge=True, name_id=GROUND):
    actor = spawn_actor(unreal.SightWeaveOccluderActor, name, location, yaw)
    component = actor.get_component_by_class(unreal.SightWeaveOccluderComponent)
    component.set_editor_property("local_points", [unreal.Vector2D(*point) for point in points])
    component.set_editor_property("closed_contour", closed)
    component.set_editor_property("floor_id", floor_id(name_id))
    component.set_editor_property("local_height_range", height_range(z_min, z_max))
    component.set_editor_property("enabled", True)
    component.set_editor_property("dynamic", dynamic)
    component.set_editor_property("merge_safe_collinear_segments", merge)
    if not component.refresh_occluder_registration():
        raise RuntimeError(f"Could not register occluder {name}: {component.get_last_validation_error()}")
    return actor


def add_vision(name, location, shape, range_cm, half_angle=45.0, yaw=0.0,
               bypass=False, accepted=("Visible",), near_radius=0.0,
               owner=LOCAL_OWNER, name_id=GROUND, z_min=0.0, z_max=300.0):
    actor = spawn_actor(unreal.SightWeaveVisionSourceActor, name, location, yaw)
    component = actor.get_component_by_class(unreal.SightWeaveVisionSourceComponent)
    description = component.get_editor_property("description")
    description.set_editor_property("knowledge_owner_id", owner_id(owner))
    description.set_editor_property("floor_id", floor_id(name_id))
    description.set_editor_property("height_range", height_range(z_min, z_max))
    description.set_editor_property("shape", shape)
    description.set_editor_property("range", range_cm)
    description.set_editor_property("half_angle_degrees", half_angle)
    description.set_editor_property("near_awareness_radius", near_radius)
    description.set_editor_property("active", True)
    description.set_editor_property(
        "illumination_policy",
        unreal.SightWeaveIlluminationPolicy.BYPASS_LEGAL_ILLUMINATION
        if bypass else unreal.SightWeaveIlluminationPolicy.REQUIRES_LEGAL_ILLUMINATION,
    )
    compatibility = unreal.SightWeaveIlluminationCompatibilityProfile()
    compatibility.set_editor_property(
        "accepted_capabilities", [] if bypass else [unreal.Name(value) for value in accepted]
    )
    description.set_editor_property("compatibility", compatibility)
    component.set_editor_property("description", description)
    if not component.refresh_vision_source_registration():
        raise RuntimeError(f"Could not register vision source {name}")
    return actor


def add_light(name, location, capabilities, shape=None, range_cm=900.0,
              half_angle=180.0, yaw=0.0, owner=LOCAL_OWNER,
              name_id=GROUND, z_min=0.0, z_max=300.0):
    actor = spawn_actor(unreal.SightWeaveIlluminationSourceActor, name, location, yaw)
    component = actor.get_component_by_class(unreal.SightWeaveIlluminationSourceComponent)
    description = component.get_editor_property("description")
    description.set_editor_property("knowledge_owner_id", owner_id(owner))
    description.set_editor_property("floor_id", floor_id(name_id))
    description.set_editor_property("height_range", height_range(z_min, z_max))
    description.set_editor_property("shape", shape or unreal.SightWeaveSourceShape.RADIAL)
    description.set_editor_property("range", range_cm)
    description.set_editor_property("half_angle_degrees", half_angle)
    description.set_editor_property("active", True)
    description.set_editor_property("emitted_capabilities", [unreal.Name(value) for value in capabilities])
    component.set_editor_property("description", description)
    if not component.refresh_illumination_source_registration():
        raise RuntimeError(f"Could not register legal illumination {name}")
    return actor


def add_suppression(name, location, radius):
    actor = spawn_actor(unreal.SightWeaveHardSuppressionActor, name, location)
    component = actor.get_component_by_class(unreal.SightWeaveHardSuppressionComponent)
    description = component.get_editor_property("description")
    description.set_editor_property("floor_id", floor_id(GROUND))
    description.set_editor_property("height_range", height_range())
    description.set_editor_property("center", unreal.Vector2D(0.0, 0.0))
    description.set_editor_property("radius", radius)
    description.set_editor_property("enabled", True)
    component.set_editor_property("description", description)
    if not component.refresh_hard_suppression_registration():
        raise RuntimeError(f"Could not register suppression {name}")
    return actor


def add_debug_query(name, location, name_id=GROUND, owner=LOCAL_OWNER):
    actor = spawn_actor(unreal.SightWeaveDebugQueryActor, name, location)
    component = actor.get_component_by_class(unreal.SightWeaveDebugQueryComponent)
    component.set_editor_property("knowledge_owner_id", owner_id(owner))
    component.set_editor_property("floor_id", floor_id(name_id))
    component.set_editor_property("draw_at_begin_play", True)
    return actor


def add_static_environment(name, location, footprint, yaw=0.0, intensity=112,
                           owner=LOCAL_OWNER, name_id=GROUND):
    actor = spawn_actor(unreal.SightWeaveStaticEnvironmentActor, name, location, yaw)
    component = actor.get_component_by_class(unreal.SightWeaveStaticEnvironmentComponent)
    component.set_editor_property("knowledge_owner_id", owner_id(owner))
    component.set_editor_property("floor_id", floor_id(name_id))
    component.set_editor_property("local_height_range", height_range())
    component.set_editor_property(
        "local_footprint", [unreal.Vector2D(*point) for point in footprint]
    )
    component.set_editor_property("neutral_intensity", intensity)
    component.set_editor_property("explicitly_immutable", True)
    component.set_editor_property("enabled", True)
    if not component.refresh_static_environment_registration():
        raise RuntimeError(f"Could not register static environment {name}")
    return actor


def add_memory_modifier(name, location, operation, shape, radius=100.0,
                        half_extents=(100.0, 100.0), rotation=0.0,
                        polygon=(), owner=LOCAL_OWNER, name_id=GROUND):
    actor = spawn_actor(unreal.SightWeaveMemoryModifierActor, name, location)
    component = actor.get_component_by_class(unreal.SightWeaveMemoryModifierComponent)
    component.set_editor_property("knowledge_owner_id", owner_id(owner))
    component.set_editor_property("floor_id", floor_id(name_id))
    component.set_editor_property("local_height_range", height_range())
    component.set_editor_property("operation", operation)
    component.set_editor_property("shape", shape)
    component.set_editor_property("radius", radius)
    component.set_editor_property("half_extents", unreal.Vector2D(*half_extents))
    component.set_editor_property("rotation_degrees", rotation)
    component.set_editor_property(
        "local_polygon_vertices", [unreal.Vector2D(*point) for point in polygon]
    )
    component.set_editor_property("enabled", True)
    # Registration intentionally occurs in PIE after exploration memory has an
    # exact world-lifetime scope. The authored component remains deterministic.
    return actor


def wall(name, cube, origin, offset, length=1100.0, yaw=0.0,
         wall_height=300.0, z_min=0.0, dynamic=False,
         static_memory=False, memory_intensity=160):
    x, y = origin[0] + offset[0], origin[1] + offset[1]
    visual = spawn_mesh(f"{name}_Visual", cube, (x, y, z_min + wall_height * 0.5),
                        (length / 100.0, 0.35, wall_height / 100.0), yaw)
    if dynamic:
        visual.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    occluder = add_occluder(
        name, (x, y, z_min), ((-length * 0.5, 0.0), (length * 0.5, 0.0)),
        yaw=yaw, z_min=0.0, z_max=wall_height, dynamic=dynamic
    )
    if static_memory:
        add_static_environment(
            f"{name}_Memory", (x, y, z_min),
            ((-length * 0.5, -28.0), (length * 0.5, -28.0),
             (length * 0.5, 28.0), (-length * 0.5, 28.0)),
            yaw=yaw, intensity=memory_intensity
        )
    return occluder


def create_lab():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    unreal_editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_editor or not unreal_editor or not editor_actors:
        raise RuntimeError("Required Unreal editor subsystems are unavailable")

    unreal.EditorAssetLibrary.make_directory(MAP_DIRECTORY)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not level_editor.load_level(MAP_PATH):
            raise RuntimeError(f"Could not load {MAP_PATH}")
        actors = editor_actors.get_all_level_actors()
        if actors and not editor_actors.destroy_actors(actors):
            raise RuntimeError("Could not clear existing lab actors")
    elif not level_editor.new_level(MAP_PATH):
        raise RuntimeError(f"Could not create {MAP_PATH}")

    cube, cylinder, cone = (unreal.load_asset(path) for path in (CUBE_PATH, CYLINDER_PATH, CONE_PATH))
    if not cube or not cylinder or not cone:
        raise RuntimeError("Engine basic shapes are unavailable")
    world = unreal_editor.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)

    add_floor("SW_M2_GroundFloor", (0.0, 0.0, 0.0), GROUND,
              (-8500.0, -6500.0), (154500.0, 12500.0), 0.0, 300.0, True)
    add_floor("SW_M2_UpperFloor", (0.0, 0.0, 500.0), "Upper",
              (-8500.0, -6500.0), (8500.0, 6500.0), 0.0, 300.0, False)

    origins = [(-6000.0 + col * 3000.0, -4500.0 + row * 3000.0)
               for row in range(4) for col in range(5)]
    labels = (
        "01 LONG WALL + LATERAL SOURCE", "02 ROTATING CONE + DIAGONAL",
        "03 L CORNER", "04 T CORNER", "05 COLLINEAR + DUPLICATE",
        "06 NARROW DOORWAY", "07 SOURCE ON / NEAR EDGE", "08 DYNAMIC ROTATING DOOR",
        "09 FLOOR SEPARATION", "10 HEIGHT BELOW / THROUGH / ABOVE",
        "11 TWO VISION SOURCE UNION", "12 EIGHT SOURCE STRESS", "13 VISIBLE + VISIBLE",
        "14 VISIBLE / INFRARED REJECT", "15 VISIBLE+IR MULTI-CHANNEL",
        "16 LEGAL ILLUMINATION ONLY", "17 CIRCLE BYPASS NO LIGHT",
        "18 SUPPRESS LIVE VISION", "19 TWO REMOTE OFF-SCREEN CONES",
        "20 ROOM + DEBUG QUERY MARKER",
    )
    for index, origin in enumerate(origins):
        spawn_mesh(f"SW_M2_Zone_{index + 1:02d}_Base", cube,
                   (origin[0], origin[1], -16.0), (26.0, 26.0, 0.16))
        spawn_text(f"SW_M2_Zone_{index + 1:02d}_Label", labels[index],
                   (origin[0] - 1200.0, origin[1] - 1100.0, 100.0), 42.0,
                   unreal.Color(145, 220, 255, 255))

    wall("SW_M2_01_LongWall", cube, origins[0], (0.0, 200.0), 2100.0)
    add_vision("SW_M2_01_LateralVision", (origins[0][0] - 700.0, origins[0][1] - 300.0, 100.0),
               unreal.SightWeaveSourceShape.CAMERA_CONE, 1500.0, 55.0, 35.0, True)

    wall("SW_M2_02_DiagonalWall", cube, origins[1], (150.0, 300.0), 1900.0, 35.0)
    add_vision("SW_M2_02_RotatingVision", (origins[1][0] - 650.0, origins[1][1] - 300.0, 100.0),
               unreal.SightWeaveSourceShape.DIRECTIONAL_CONE, 1500.0, 35.0, 20.0, True)

    wall("SW_M2_03_L_A", cube, origins[2], (-400.0, 250.0), 1000.0)
    wall("SW_M2_03_L_B", cube, origins[2], (100.0, 750.0), 1000.0, 90.0)
    add_vision("SW_M2_03_Vision", (origins[2][0] - 700.0, origins[2][1] - 350.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1600.0, bypass=True)

    wall("SW_M2_04_T_Top", cube, origins[3], (0.0, 450.0), 1900.0)
    wall("SW_M2_04_T_Stem", cube, origins[3], (0.0, -50.0), 1000.0, 90.0)
    add_vision("SW_M2_04_Vision", (origins[3][0] - 700.0, origins[3][1] - 450.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1600.0, bypass=True)

    add_occluder("SW_M2_05_Collinear", (origins[4][0], origins[4][1] + 250.0, 0.0),
                 ((-900.0, 0.0), (-300.0, 0.0), (300.0, 0.0), (900.0, 0.0)), merge=True)
    add_occluder("SW_M2_05_Duplicate", (origins[4][0], origins[4][1] + 250.0, 0.0),
                 ((-300.0, 0.0), (300.0, 0.0), (-300.0, 0.0)), merge=False)
    spawn_mesh("SW_M2_05_WallVisual", cube, (origins[4][0], origins[4][1] + 250.0, 150.0), (18.0, 0.35, 3.0))
    add_vision("SW_M2_05_Vision", (origins[4][0], origins[4][1] - 500.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1500.0, bypass=True)

    wall("SW_M2_06_DoorwayLeft", cube, origins[5], (-650.0, 250.0), 1020.0)
    wall("SW_M2_06_DoorwayRight", cube, origins[5], (650.0, 250.0), 1020.0)
    add_vision("SW_M2_06_Vision", (origins[5][0], origins[5][1] - 650.0, 100.0),
               unreal.SightWeaveSourceShape.DIRECTIONAL_CONE, 1800.0, 45.0, 90.0, True)

    wall("SW_M2_07_Edge", cube, origins[6], (0.0, 250.0), 1900.0)
    add_vision("SW_M2_07_OnEdge", (origins[6][0] - 350.0, origins[6][1] + 250.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 900.0, bypass=True)
    add_vision("SW_M2_07_NearEdge", (origins[6][0] + 350.0, origins[6][1] + 249.95, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 900.0, bypass=True)

    wall("SW_M2_08_FrameLeft", cube, origins[7], (-850.0, 250.0), 500.0)
    wall("SW_M2_08_FrameRight", cube, origins[7], (850.0, 250.0), 500.0)
    wall("SW_M2_08_DynamicDoor", cube, origins[7], (0.0, 250.0), 1000.0, 32.0, 260.0, dynamic=True)
    add_vision("SW_M2_08_Vision", (origins[7][0], origins[7][1] - 650.0, 100.0),
               unreal.SightWeaveSourceShape.DIRECTIONAL_CONE, 1800.0, 45.0, 90.0, True)

    spawn_mesh("SW_M2_09_UpperDeck", cube, (origins[8][0], origins[8][1] + 200.0, 500.0), (18.0, 18.0, 0.35))
    add_vision("SW_M2_09_GroundVision", (origins[8][0] - 400.0, origins[8][1] + 200.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 900.0, bypass=True)
    add_vision("SW_M2_09_UpperVision", (origins[8][0] + 400.0, origins[8][1] + 200.0, 600.0),
               unreal.SightWeaveSourceShape.RADIAL, 900.0, bypass=True,
               name_id="Upper", z_min=500.0, z_max=800.0)

    for suffix, x_offset, z_min, wall_height in (
        ("Below", -700.0, -200.0, 100.0), ("Through", 0.0, 0.0, 300.0), ("Above", 700.0, 400.0, 180.0)
    ):
        wall(f"SW_M2_10_{suffix}", cube, origins[9], (x_offset, 250.0), 500.0,
             wall_height=wall_height, z_min=z_min)
    add_vision("SW_M2_10_Vision", (origins[9][0], origins[9][1] - 550.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1600.0, bypass=True, z_min=50.0, z_max=150.0)

    for suffix, x_offset in (("A", -550.0), ("B", 550.0)):
        add_vision(f"SW_M2_11_Vision{suffix}", (origins[10][0] + x_offset, origins[10][1] + 200.0, 100.0),
                   unreal.SightWeaveSourceShape.RADIAL, 850.0, bypass=True)

    for index in range(8):
        angle = 2.0 * math.pi * index / 8.0
        add_vision(f"SW_M2_12_Stress_{index + 1}",
                   (origins[11][0] + math.cos(angle) * 650.0,
                    origins[11][1] + 200.0 + math.sin(angle) * 650.0, 100.0),
                   unreal.SightWeaveSourceShape.DIRECTIONAL_CONE, 1000.0, 35.0,
                   math.degrees(angle) + 180.0, True)

    add_vision("SW_M2_13_VisibleVision", (origins[12][0] - 350.0, origins[12][1] + 150.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1000.0, accepted=("Visible",))
    add_light("SW_M2_13_VisibleLight", (origins[12][0] + 350.0, origins[12][1] + 150.0, 100.0),
              ("Visible",), range_cm=1000.0)

    add_vision("SW_M2_14_VisibleVision", (origins[13][0] - 350.0, origins[13][1] + 150.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1000.0, accepted=("Visible",))
    add_light("SW_M2_14_InfraredLight", (origins[13][0] + 350.0, origins[13][1] + 150.0, 100.0),
              ("Infrared",), range_cm=1000.0)

    add_vision("SW_M2_15_MultiVision", (origins[14][0] - 350.0, origins[14][1] + 150.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1000.0, accepted=("Visible", "Infrared"))
    add_light("SW_M2_15_InfraredLight", (origins[14][0] + 350.0, origins[14][1] + 150.0, 100.0),
              ("Infrared",), range_cm=1000.0)

    add_light("SW_M2_16_LegalLightOnly", (origins[15][0], origins[15][1] + 200.0, 100.0),
              ("Visible",), range_cm=1000.0)
    add_vision("SW_M2_17_CircleBypass", (origins[16][0], origins[16][1] + 200.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 750.0, bypass=True)
    spawn_mesh("SW_M2_17_BypassMarker", cylinder, (origins[16][0], origins[16][1] + 200.0, 18.0), (7.5, 7.5, 0.18))

    add_vision("SW_M2_18_Bypass", (origins[17][0], origins[17][1] + 200.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 900.0, bypass=True)
    add_suppression("SW_M2_18_SuppressLiveVision", (origins[17][0] + 250.0, origins[17][1] + 200.0, 0.0), 300.0)

    for suffix, x_offset, yaw in (("A", -650.0, 25.0), ("B", 650.0, 155.0)):
        add_vision(f"SW_M2_19_Remote{suffix}", (origins[18][0] + x_offset, origins[18][1] + 150.0, 100.0),
                   unreal.SightWeaveSourceShape.CAMERA_CONE, 1600.0, 28.0, yaw, True)

    for suffix, offset, length, yaw in (
        ("North", (0.0, 850.0), 1700.0, 0.0), ("South", (0.0, -150.0), 1700.0, 0.0),
        ("East", (850.0, 350.0), 1000.0, 90.0), ("West", (-850.0, 350.0), 1000.0, 90.0),
    ):
        wall(f"SW_M2_20_Room_{suffix}", cube, origins[19], offset, length, yaw)
    add_vision("SW_M2_20_DebugVision", (origins[19][0], origins[19][1] + 350.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 1200.0, bypass=True)
    spawn_mesh("SW_M2_20_DebugQueryMarker", cone,
               (origins[19][0] + 350.0, origins[19][1] + 350.0, 70.0), (1.0, 1.0, 1.4))
    add_debug_query("SW_M2_20_AuthorityQuery", (origins[19][0] + 350.0, origins[19][1] + 350.0, 100.0))

    # M3.3 presentation-only display area. The active Ground floor keeps Local as the
    # explicit default presentation owner; these fixtures do not enter host gameplay.
    m3_origin = (0.0, 8500.0)
    spawn_mesh("SW_M3P3_Presentation_Base", cube,
               (m3_origin[0], m3_origin[1], -16.0), (130.0, 45.0, 0.16))
    spawn_text("SW_M3P3_Presentation_Label",
               "M3.3 POST-TONEMAP HARD MASK / LIVE SCENE COLOR / STRICT BLACK",
               (-6100.0, 6700.0, 130.0), 58.0, unreal.Color(120, 255, 165, 255))
    wall("SW_M3P3_StraightWall", cube, (-4700.0, 8500.0), (0.0, 250.0), 1900.0)
    wall("SW_M3P3_L_A", cube, (-1700.0, 8500.0), (-450.0, 150.0), 1000.0)
    wall("SW_M3P3_L_B", cube, (-1700.0, 8500.0), (50.0, 650.0), 1000.0, 90.0)
    wall("SW_M3P3_T_Top", cube, (1500.0, 8500.0), (0.0, 500.0), 1900.0)
    wall("SW_M3P3_T_Stem", cube, (1500.0, 8500.0), (0.0, 0.0), 1000.0, 90.0)
    wall("SW_M3P3_DiagonalWall", cube, (4700.0, 8500.0), (0.0, 300.0), 1900.0, 35.0)
    add_vision("SW_M3P3_PresentationVision", (0.0, 7200.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 7000.0, bypass=True)
    for seam_index, seam_x in enumerate((-3540.0, -1060.0, 1420.0, 3900.0, 6380.0)):
        spawn_mesh(f"SW_M3P3_TileSeam_{seam_index + 1}", cube,
                   (seam_x, 8500.0, 3.0), (0.025, 80.0, 0.03))
    spawn_camera("SW_M3P3_OverviewCamera", (0.0, 8500.0, 10000.0),
                 yaw=0.0, ortho_width=14500.0, auto_activate=False)

    # With Ground.BoundsMin.X == -8500 and Standard span == 2480 cm, this is the
    # logical 63/64 boundary. The narrow cone forces persistent residency across
    # page 0 slot 63 and page 1 slot 0 without covering a broad 2-D tile field.
    page_boundary_x = -8500.0 + 64.0 * 2480.0
    # Keep the narrow 1600 m cone inside one logical tile row. At 11000 cm its
    # finite half width straddled the 10860 cm row boundary, producing a 65x2
    # sparse AABB (130 tiles) and correctly tripping the frozen capacity of 128.
    page_strip_y = 12100.0
    add_vision("SW_M3P3_PageBoundaryVision", (-8000.0, page_strip_y, 100.0),
               unreal.SightWeaveSourceShape.DIRECTIONAL_CONE,
               160000.0, 0.2, 0.0, True)
    spawn_mesh("SW_M3P3_PageBoundary_Base", cube,
               (page_boundary_x, page_strip_y, -16.0), (55.0, 30.0, 0.16))
    wall("SW_M3P3_PageBoundaryWall", cube, (page_boundary_x, page_strip_y),
         (0.0, 0.0), 2000.0, 90.0)
    spawn_text("SW_M3P3_PageBoundary_Label",
               "LOGICAL 63 / 64 - PAGE 0 SLOT 63 / PAGE 1 SLOT 0",
               (page_boundary_x - 2500.0, page_strip_y - 1200.0, 130.0),
               52.0, unreal.Color(255, 205, 90, 255))
    spawn_camera("SW_M3P3_PageBoundaryCamera",
                 (page_boundary_x, page_strip_y, 6000.0),
                 yaw=0.0, ortho_width=7000.0, auto_activate=False)

    # M3.4 is a separate presentation-only fixture strip. It deliberately reuses
    # the Ground/Local authority data model while exposing inward-feather edge,
    # corner, narrow-corridor, suppression, bypass, dirty-door, view, seam, and
    # page-boundary inspection points. No host gameplay or SceneCapture is used.
    m3p4_origin = (31000.0, 8500.0)
    spawn_mesh("SW_M3P4_Feather_Base", cube,
               (m3p4_origin[0], m3p4_origin[1], -16.0), (135.0, 45.0, 0.16))
    spawn_text("SW_M3P4_Feather_Label",
               "M3.4 WORLD-SPACE INWARD FEATHER / HARD POINT GATE REMAINS FINAL",
               (m3p4_origin[0] - 6400.0, 6700.0, 130.0), 58.0,
               unreal.Color(210, 145, 255, 255))

    wall("SW_M3P4_StraightWall", cube, (24500.0, 8500.0), (0.0, 250.0), 1900.0)
    wall("SW_M3P4_L_A", cube, (27500.0, 8500.0), (-450.0, 150.0), 1000.0)
    wall("SW_M3P4_L_B", cube, (27500.0, 8500.0), (50.0, 650.0), 1000.0, 90.0)
    wall("SW_M3P4_T_Top", cube, (30500.0, 8500.0), (0.0, 500.0), 1900.0)
    wall("SW_M3P4_T_Stem", cube, (30500.0, 8500.0), (0.0, 0.0), 1000.0, 90.0)
    wall("SW_M3P4_Corridor_Left", cube, (33500.0, 8500.0), (-330.0, 250.0), 1800.0, 90.0)
    wall("SW_M3P4_Corridor_Right", cube, (33500.0, 8500.0), (330.0, 250.0), 1800.0, 90.0)
    wall("SW_M3P4_DynamicDoor", cube, (36500.0, 8500.0), (0.0, 250.0),
         1100.0, M3P4_DYNAMIC_DOOR_YAW, 260.0, dynamic=True)
    add_vision("SW_M3P4_OverviewBypass", (31000.0, 7200.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 8500.0, bypass=True)
    add_suppression("SW_M3P4_SuppressionCut", (38200.0, 8500.0, 0.0), 420.0)
    spawn_text("SW_M3P4_Bypass_Label", "BYPASS - SAME INWARD VISUAL RULE",
               (23700.0, 10100.0, 110.0), 42.0, unreal.Color(110, 240, 255, 255))
    spawn_text("SW_M3P4_Suppression_Label", "SUPPRESSION CUT - NEVER FEATHER OUTWARD",
               (36800.0, 10100.0, 110.0), 42.0, unreal.Color(255, 125, 125, 255))

    for seam_index, seam_x in enumerate((26220.0, 28700.0, 31180.0, 33660.0, 36140.0)):
        spawn_mesh(f"SW_M3P4_TileSeam_{seam_index + 1}", cube,
                   (seam_x, 8500.0, 4.0), (0.025, 80.0, 0.04))
    spawn_camera("SW_M3P4_OverviewCamera", (31000.0, 8500.0, 10000.0),
                 yaw=0.0, ortho_width=15500.0,
                 auto_activate=M3P4_ACTIVE_CAMERA == "Overview")
    spawn_camera("SW_M3P4_Rotated45Camera", (31000.0, 8500.0, 10000.0),
                 yaw=45.0, ortho_width=15500.0,
                 auto_activate=M3P4_ACTIVE_CAMERA == "Rotated45")
    spawn_camera("SW_M3P4_CloseupCamera", (24500.0, 8500.0, 6000.0),
                 yaw=0.0, ortho_width=4200.0,
                 auto_activate=M3P4_ACTIVE_CAMERA == "Closeup")
    spawn_camera("SW_M3P4_DynamicDoorCamera", (36500.0, 8500.0, 6000.0),
                 yaw=0.0, ortho_width=4200.0,
                 auto_activate=M3P4_ACTIVE_CAMERA == "DynamicDoor")
    spawn_text("SW_M3P4_PageBoundary_Label",
               "M3.4 FEATHER CONTINUITY REUSES LOGICAL 63/64 FIXTURE",
               (page_boundary_x - 2500.0, page_strip_y + 1100.0, 130.0),
               48.0, unreal.Color(210, 145, 255, 255))
    spawn_camera("SW_M3P4_PageBoundaryCamera",
                 (page_boundary_x, page_strip_y, 6000.0),
                 yaw=45.0, ortho_width=7000.0,
                 auto_activate=M3P4_ACTIVE_CAMERA == "PageBoundary")

    # M3.5 static-environment exploration-memory strip. Every remembered cue is
    # explicitly authored through immutable CPU footprints; dynamic sentinels do
    # not receive eligibility and therefore cannot leak their current state.
    m3p5_origin = (52000.0, 8500.0)
    spawn_mesh("SW_M3P5_Memory_Base", cube,
               (m3p5_origin[0], m3p5_origin[1], -16.0), (190.0, 45.0, 0.16))
    add_static_environment(
        "SW_M3P5_StaticGroundMemory", (m3p5_origin[0], m3p5_origin[1], 0.0),
        ((-9500.0, -2250.0), (9500.0, -2250.0),
         (9500.0, 2250.0), (-9500.0, 2250.0)), intensity=68
    )
    spawn_text("SW_M3P5_Memory_Label",
               "M3.5 HARDLIVE > HARDMEMORY + EXPLICIT STATIC ATTRIBUTE > UNKNOWN",
               (43000.0, 6500.0, 130.0), 54.0, unreal.Color(165, 215, 255, 255))
    spawn_text("SW_M3P5_Remembered_Label", "REMEMBERED STATIC GRAY / CLEAR / SUPPRESS",
               (44500.0, 10300.0, 110.0), 38.0, unreal.Color(185, 185, 185, 255))
    spawn_text("SW_M3P5_Live_Label", "CURRENT LIVE SCENE COLOR",
               (55500.0, 10300.0, 110.0), 42.0, unreal.Color(110, 255, 150, 255))
    spawn_text("SW_M3P5_Unknown_Label", "UNKNOWN = STRICT BLACK",
               (59500.0, 10300.0, 110.0), 42.0, unreal.Color(255, 95, 95, 255))

    wall("SW_M3P5_StraightWall", cube, (44500.0, 8500.0), (0.0, 250.0),
         1900.0, static_memory=True, memory_intensity=168)
    wall("SW_M3P5_L_A", cube, (47500.0, 8500.0), (-450.0, 150.0),
         1000.0, static_memory=True, memory_intensity=176)
    wall("SW_M3P5_L_B", cube, (47500.0, 8500.0), (50.0, 650.0),
         1000.0, 90.0, static_memory=True, memory_intensity=176)
    wall("SW_M3P5_T_Top", cube, (50500.0, 8500.0), (0.0, 500.0),
         1900.0, static_memory=True, memory_intensity=184)
    wall("SW_M3P5_T_Stem", cube, (50500.0, 8500.0), (0.0, 0.0),
         1000.0, 90.0, static_memory=True, memory_intensity=184)
    wall("SW_M3P5_DiagonalWall", cube, (53500.0, 8500.0), (0.0, 250.0),
         1800.0, 35.0, static_memory=True, memory_intensity=192)
    for suffix, offset, length, yaw in (
        ("North", (0.0, 900.0), 1900.0, 0.0),
        ("South", (0.0, -100.0), 1900.0, 0.0),
        ("East", (950.0, 400.0), 1000.0, 90.0),
        ("West", (-950.0, 400.0), 1000.0, 90.0),
    ):
        wall(f"SW_M3P5_Room_{suffix}", cube, (56000.0, 8500.0), offset,
             length, yaw, static_memory=True, memory_intensity=158)
    wall("SW_M3P5_DynamicDoor", cube, (58000.0, 8500.0), (0.0, 300.0),
         1100.0, 32.0, 260.0, dynamic=True, static_memory=False)
    moving = spawn_mesh("SW_M3P5_MovingMeshLeakSentinel", cylinder,
                        (59000.0, 8500.0, 120.0), (2.0, 2.0, 2.4))
    moving.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    point_light = spawn_actor(unreal.PointLight, "SW_M3P5_CurrentLightLeakSentinel",
                              (57000.0, 8500.0, 450.0))
    point_light.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    point_light.point_light_component.set_editor_property("intensity", 9000.0)
    emissive = spawn_mesh("SW_M3P5_EmissiveParticleLeakSentinel", cone,
                          (60000.0, 8500.0, 100.0), (1.4, 1.4, 2.0))
    emissive.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn_text("SW_M3P5_DynamicLeak_Label",
               "DYNAMIC DOOR / MOVING MESH / CURRENT LIGHT / VFX MUST DISAPPEAR",
               (56500.0, 6800.0, 110.0), 34.0, unreal.Color(255, 150, 95, 255))

    add_vision("SW_M3P5_RememberOnce", (48000.0, 8500.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 4300.0, bypass=True)
    add_vision("SW_M3P5_LiveNow", (57000.0, 8500.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 2100.0, bypass=True)
    add_vision("SW_M3P5_BlockProbe", (53000.0, 8500.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 700.0, bypass=True)
    add_memory_modifier(
        "SW_M3P5_BlockMemoryWrites", (53000.0, 8500.0, 0.0),
        unreal.SightWeaveMemoryModifierOperation.BLOCK_MEMORY_WRITES,
        unreal.SightWeaveMemoryRegionShape.CIRCLE, radius=850.0
    )
    add_memory_modifier(
        "SW_M3P5_SuppressMemoryPresentation", (49500.0, 8500.0, 0.0),
        unreal.SightWeaveMemoryModifierOperation.SUPPRESS_MEMORY_PRESENTATION,
        unreal.SightWeaveMemoryRegionShape.ROTATED_BOX,
        half_extents=(500.0, 350.0), rotation=25.0
    )
    spawn_text("SW_M3P5_Clear_Label", "CLEAR MEMORY -> UNKNOWN",
               (46500.0, 7600.0, 90.0), 32.0, unreal.Color(255, 90, 90, 255))
    spawn_text("SW_M3P5_Block_Label", "BLOCK WRITES -> STAYS UNKNOWN",
               (52000.0, 7600.0, 90.0), 32.0, unreal.Color(255, 185, 75, 255))
    spawn_text("SW_M3P5_Suppress_Label", "SUPPRESS PRESENTATION / AUTHORITY RETAINED",
               (48700.0, 9300.0, 90.0), 30.0, unreal.Color(205, 130, 255, 255))

    # Explicit off-floor-bounds source exercises negative logical tiles relative
    # to Ground.BoundsMin without changing the frozen page-boundary mapping.
    add_vision("SW_M3P5_NegativeTileRememberOnce", (-12000.0, 8500.0, 100.0),
               unreal.SightWeaveSourceShape.RADIAL, 900.0, bypass=True)
    add_static_environment(
        "SW_M3P5_NegativeTileStaticMemory", (-12000.0, 8500.0, 0.0),
        ((-1000.0, -1000.0), (1000.0, -1000.0),
         (1000.0, 1000.0), (-1000.0, 1000.0)), intensity=124
    )
    add_static_environment(
        "SW_M3P5_PageBoundaryStaticMemory", (page_boundary_x, page_strip_y, 0.0),
        ((-2600.0, -600.0), (2600.0, -600.0),
         (2600.0, 600.0), (-2600.0, 600.0)), intensity=140
    )

    for seam_index, seam_x in enumerate((43500.0, 45980.0, 48460.0, 50940.0,
                                         53420.0, 55900.0, 58380.0, 60860.0)):
        spawn_mesh(f"SW_M3P5_TileSeam_{seam_index + 1}", cube,
                   (seam_x, 8500.0, 4.0), (0.025, 80.0, 0.04))
    spawn_camera("SW_M3P5_OverviewCamera", (52000.0, 8500.0, 12000.0),
                 yaw=90.0, ortho_width=21000.0,
                 auto_activate=M3P5_ACTIVE_CAMERA == "Overview")
    spawn_camera("SW_M3P5_Rotated45Camera", (52000.0, 8500.0, 12000.0),
                 yaw=45.0, ortho_width=21000.0,
                 auto_activate=M3P5_ACTIVE_CAMERA == "Rotated45")
    spawn_camera("SW_M3P5_RememberedCamera", (48000.0, 8500.0, 6500.0),
                 yaw=90.0, ortho_width=6500.0,
                 auto_activate=M3P5_ACTIVE_CAMERA == "Remembered")
    spawn_camera("SW_M3P5_DynamicLeakCamera", (58500.0, 8500.0, 6500.0),
                 yaw=90.0, ortho_width=6000.0,
                 auto_activate=M3P5_ACTIVE_CAMERA == "DynamicLeak")
    spawn_camera("SW_M3P5_PageBoundaryCamera", (page_boundary_x, page_strip_y, 6500.0),
                 yaw=90.0, ortho_width=7500.0,
                 auto_activate=M3P5_ACTIVE_CAMERA == "PageBoundary")

    spawn_text("SW_M2_Lab_Title", "SIGHTWEAVE M2 CPU AUTHORITY LAB",
               (-2400.0, -6200.0, 240.0), 95.0, unreal.Color(105, 210, 255, 255))
    spawn_text("SW_M2_Lab_Disclaimer", "REFERENCE SOLVER / COMPONENT FIXTURES / DEBUG DATA - NO GPU FOG",
               (-3300.0, -5950.0, 150.0), 48.0, unreal.Color(255, 170, 80, 255))
    light = spawn_actor(unreal.DirectionalLight, "SW_M2_Lab_NeutralDirectionalLight", (0.0, 0.0, 3000.0))
    light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-45.0, yaw=-35.0), False)
    spawn_actor(unreal.SkyLight, "SW_M2_Lab_NeutralSkyLight", (0.0, 0.0, 1000.0))
    spawn_actor(unreal.PlayerStart, "SW_M2_Lab_PlayerStart", (0.0, -6200.0, 120.0), 90.0)

    if not level_editor.save_current_level():
        raise RuntimeError(f"Could not save {MAP_PATH}")
    if not unreal.EditorAssetLibrary.save_asset(MAP_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not persist {MAP_PATH}")
    unreal.log(f"SightWeave M2/M3/M3.4/M3.5 created and saved {MAP_PATH}")


if __name__ == "__main__":
    create_lab()
