import unreal


ASSET_PATH = "/Game/UI/Fog"
ASSET_NAME = "M_FogMemoryComposite"


def make_expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def connect(source, output_name, target, input_name):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, output_name, target, input_name
    ):
        raise RuntimeError(
            f"Could not connect {source.get_class().get_name()}.{output_name} "
            f"to {target.get_class().get_name()}.{input_name}"
        )


def create_material():
    full_path = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        ASSET_NAME,
        ASSET_PATH,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        raise RuntimeError("Could not create fog-memory post-process material")

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    preferred_locations = (
        "BL_SCENE_COLOR_BEFORE_DOF",
        "BL_SCENE_COLOR_BEFORE_BLOOM",
        "BL_SCENE_COLOR_AFTER_DOF",
    )
    for location_name in preferred_locations:
        location = getattr(unreal.BlendableLocation, location_name, None)
        if location is not None:
            material.set_editor_property("blendable_location", location)
            unreal.log(f"DARKWELL fog blendable location: {location_name}")
            break

    fog_mask = make_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -350
    )
    fog_mask.set_editor_property("parameter_name", "FogMaskTexture")
    fog_mask.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    )
    default_mask = unreal.load_asset("/Engine/EngineResources/Black")
    if default_mask:
        fog_mask.set_editor_property("texture", default_mask)

    scene_color = make_expression(
        material, unreal.MaterialExpressionSceneTexture, -1000, -650
    )
    scene_color.set_editor_property(
        "scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0
    )
    scene_rgb = make_expression(
        material, unreal.MaterialExpressionComponentMask, -750, -650
    )
    scene_rgb.set_editor_property("r", True)
    scene_rgb.set_editor_property("g", True)
    scene_rgb.set_editor_property("b", True)
    scene_rgb.set_editor_property("a", False)
    connect(scene_color, "Color", scene_rgb, "")
    base_color = make_expression(
        material, unreal.MaterialExpressionSceneTexture, -1000, 100
    )
    base_color.set_editor_property(
        "scene_texture_id", unreal.SceneTextureId.PPI_BASE_COLOR
    )
    base_rgb = make_expression(
        material, unreal.MaterialExpressionComponentMask, -750, 100
    )
    base_rgb.set_editor_property("r", True)
    base_rgb.set_editor_property("g", True)
    base_rgb.set_editor_property("b", True)
    base_rgb.set_editor_property("a", False)
    connect(base_color, "Color", base_rgb, "")

    # Reconstruct stable geometry cues without using live illumination. Base
    # colour alone makes a white pillar disappear into a white floor, so use
    # the GBuffer normal and depth discontinuities as a fixed presentation
    # light and a narrow silhouette line.
    world_normal = make_expression(
        material, unreal.MaterialExpressionSceneTexture, -1000, 400
    )
    world_normal.set_editor_property(
        "scene_texture_id", unreal.SceneTextureId.PPI_WORLD_NORMAL
    )
    normal_z = make_expression(
        material, unreal.MaterialExpressionComponentMask, -750, 400
    )
    normal_z.set_editor_property("r", False)
    normal_z.set_editor_property("g", False)
    normal_z.set_editor_property("b", True)
    normal_z.set_editor_property("a", False)
    connect(world_normal, "Color", normal_z, "")
    normal_abs = make_expression(material, unreal.MaterialExpressionAbs, -500, 400)
    connect(normal_z, "", normal_abs, "")
    normal_scale = make_expression(
        material, unreal.MaterialExpressionMultiply, -250, 400
    )
    normal_scale.set_editor_property("const_b", 0.35)
    connect(normal_abs, "", normal_scale, "A")
    stable_geometry_light = make_expression(
        material, unreal.MaterialExpressionAdd, 0, 400
    )
    stable_geometry_light.set_editor_property("const_a", 0.65)
    connect(normal_scale, "", stable_geometry_light, "B")

    scene_depth = make_expression(
        material, unreal.MaterialExpressionSceneTexture, -1000, 650
    )
    scene_depth.set_editor_property(
        "scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH
    )
    depth_value = make_expression(
        material, unreal.MaterialExpressionComponentMask, -750, 650
    )
    depth_value.set_editor_property("r", True)
    depth_value.set_editor_property("g", False)
    depth_value.set_editor_property("b", False)
    depth_value.set_editor_property("a", False)
    connect(scene_depth, "Color", depth_value, "")
    depth_ddx = make_expression(material, unreal.MaterialExpressionDDX, -500, 600)
    depth_ddy = make_expression(material, unreal.MaterialExpressionDDY, -500, 700)
    connect(depth_value, "", depth_ddx, "")
    connect(depth_value, "", depth_ddy, "")
    depth_ddx_abs = make_expression(material, unreal.MaterialExpressionAbs, -250, 600)
    depth_ddy_abs = make_expression(material, unreal.MaterialExpressionAbs, -250, 700)
    connect(depth_ddx, "", depth_ddx_abs, "")
    connect(depth_ddy, "", depth_ddy_abs, "")
    depth_gradient = make_expression(material, unreal.MaterialExpressionAdd, 0, 650)
    connect(depth_ddx_abs, "", depth_gradient, "A")
    connect(depth_ddy_abs, "", depth_gradient, "B")
    depth_edge_scale = make_expression(
        material, unreal.MaterialExpressionMultiply, 250, 650
    )
    depth_edge_scale.set_editor_property("const_b", 0.02)
    connect(depth_gradient, "", depth_edge_scale, "A")
    depth_edge = make_expression(material, unreal.MaterialExpressionSaturate, 500, 650)
    connect(depth_edge_scale, "", depth_edge, "")
    depth_edge_strength = make_expression(
        material, unreal.MaterialExpressionMultiply, 750, 650
    )
    depth_edge_strength.set_editor_property("const_b", 0.45)
    connect(depth_edge, "", depth_edge_strength, "A")
    geometry_subtract_edge = make_expression(
        material, unreal.MaterialExpressionSubtract, 500, 400
    )
    connect(stable_geometry_light, "", geometry_subtract_edge, "A")
    connect(depth_edge_strength, "", geometry_subtract_edge, "B")
    stable_geometry_factor = make_expression(
        material, unreal.MaterialExpressionSaturate, 750, 400
    )
    connect(geometry_subtract_edge, "", stable_geometry_factor, "")

    current_color = make_expression(material, unreal.MaterialExpressionMultiply, -500, -600)
    connect(scene_rgb, "", current_color, "A")
    connect(fog_mask, "R", current_color, "B")

    luminance_weights = make_expression(
        material, unreal.MaterialExpressionConstant3Vector, -750, 100
    )
    luminance_weights.set_editor_property(
        "constant", unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0)
    )
    base_luminance = make_expression(
        material, unreal.MaterialExpressionDotProduct, -500, 100
    )
    connect(base_rgb, "", base_luminance, "A")
    connect(luminance_weights, "", base_luminance, "B")
    luminance_scale = make_expression(
        material, unreal.MaterialExpressionMultiply, -250, 100
    )
    luminance_scale.set_editor_property("const_b", 0.10)
    connect(base_luminance, "", luminance_scale, "A")

    memory_base = make_expression(
        material, unreal.MaterialExpressionConstant3Vector, -250, 250
    )
    memory_base.set_editor_property(
        "constant", unreal.LinearColor(0.018, 0.022, 0.026, 1.0)
    )
    memory_add_luminance = make_expression(material, unreal.MaterialExpressionAdd, 0, 150)
    connect(memory_base, "", memory_add_luminance, "A")
    connect(luminance_scale, "", memory_add_luminance, "B")
    memory_with_geometry = make_expression(
        material, unreal.MaterialExpressionMultiply, 250, 150
    )
    connect(memory_add_luminance, "", memory_with_geometry, "A")
    connect(stable_geometry_factor, "", memory_with_geometry, "B")

    inverse_current = make_expression(material, unreal.MaterialExpressionSubtract, -500, -250)
    inverse_current.set_editor_property("const_a", 1.0)
    connect(fog_mask, "R", inverse_current, "B")
    memory_only = make_expression(material, unreal.MaterialExpressionMultiply, -250, -250)
    connect(fog_mask, "G", memory_only, "A")
    connect(inverse_current, "", memory_only, "B")
    remembered_color = make_expression(material, unreal.MaterialExpressionMultiply, 500, 0)
    connect(memory_with_geometry, "", remembered_color, "A")
    connect(memory_only, "", remembered_color, "B")

    final_color = make_expression(material, unreal.MaterialExpressionAdd, 750, -300)
    connect(current_color, "", final_color, "A")
    connect(remembered_color, "", final_color, "B")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        final_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError("Could not connect fog composite to emissive output")

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(full_path, only_if_is_dirty=False)
    unreal.log(f"DARKWELL created {full_path}")


if __name__ == "__main__":
    create_material()
