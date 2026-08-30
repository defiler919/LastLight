import unreal


ASSET_PATH = "/Game/Darkwell/Vision/ProjectFog"
COVERAGE_NAME = "M_DarkwellFogCoverage"
SURFACE_NAME = "M_DarkwellFogSurface"


def expr(material, klass, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, klass, x, y)


def connect(source, output_name, target, input_name):
    aliases = {"Coordinates": ["UVs"]}
    for candidate in [input_name] + aliases.get(input_name, []):
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output_name, target, candidate
        ):
            return
    available = unreal.MaterialEditingLibrary.get_material_expression_input_names(target)
    if len(available) == 1 and unreal.MaterialEditingLibrary.connect_material_expressions(
        source, output_name, target, ""
    ):
        return
    raise RuntimeError(
        f"Could not connect {source.get_name()}.{output_name} to "
        f"{target.get_name()}.{input_name}; inputs={available}"
    )


def scalar(material, value, x, y):
    node = expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def scalar_parameter(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def vector_parameter(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def binary(material, klass, a, b, x, y):
    node = expr(material, klass, x, y)
    connect(a, "", node, "A")
    connect(b, "", node, "B")
    return node


def mask(material, source, channels, x, y, source_output=""):
    node = expr(material, unreal.MaterialExpressionComponentMask, x, y)
    node.set_editor_property("r", "r" in channels)
    node.set_editor_property("g", "g" in channels)
    node.set_editor_property("b", "b" in channels)
    node.set_editor_property("a", "a" in channels)
    connect(source, source_output, node, "")
    return node


def saturate(material, source, x, y):
    node = expr(material, unreal.MaterialExpressionSaturate, x, y)
    connect(source, "", node, "")
    return node


def make_asset(asset_tools, name):
    path = f"{ASSET_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = asset_tools.create_asset(
        name, ASSET_PATH, unreal.Material, unreal.MaterialFactoryNew()
    )
    if not material:
        raise RuntimeError(f"Could not create {path}")
    return material, path


def create_coverage(asset_tools):
    material, path = make_asset(asset_tools, COVERAGE_NAME)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    uv = expr(material, unreal.MaterialExpressionTextureCoordinate, -2200, -100)
    world_min_param = vector_parameter(
        material, "FogWorldMin", unreal.LinearColor(0, 0, 0, 0), -2200, 50
    )
    world_extent_param = vector_parameter(
        material, "FogWorldExtent", unreal.LinearColor(3500, 2500, 0, 0), -2200, 180
    )
    world_min = mask(material, world_min_param, "rg", -1950, 50)
    world_extent = mask(material, world_extent_param, "rg", -1950, 180)
    scaled_uv = binary(
        material, unreal.MaterialExpressionMultiply, uv, world_extent, -1700, -50
    )
    world = binary(
        material, unreal.MaterialExpressionAdd, world_min, scaled_uv, -1450, -50
    )

    body_center_param = vector_parameter(
        material, "BodyCenter", unreal.LinearColor(0, 0, 0, 0), -2200, -650
    )
    body_center = mask(material, body_center_param, "rg", -1950, -650)
    body_relative = binary(
        material, unreal.MaterialExpressionSubtract, world, body_center, -1200, -600
    )
    body_distance = expr(material, unreal.MaterialExpressionLength, -950, -600)
    connect(body_relative, "", body_distance, "")
    body_radius = scalar_parameter(material, "BodyRadius", 120.0, -1200, -750)
    body_signed = binary(
        material, unreal.MaterialExpressionSubtract, body_radius, body_distance, -700, -650
    )

    cone_origin_param = vector_parameter(
        material, "ConeOrigin", unreal.LinearColor(0, 0, 0, 0), -2200, 450
    )
    cone_forward_param = vector_parameter(
        material, "ConeForward", unreal.LinearColor(1, 0, 0, 0), -2200, 580
    )
    cone_origin = mask(material, cone_origin_param, "rg", -1950, 450)
    cone_forward = mask(material, cone_forward_param, "rg", -1950, 580)
    cone_relative = binary(
        material, unreal.MaterialExpressionSubtract, world, cone_origin, -1200, 350
    )
    along = expr(material, unreal.MaterialExpressionDotProduct, -950, 300)
    connect(cone_relative, "", along, "A")
    connect(cone_forward, "", along, "B")
    rel_x = mask(material, cone_relative, "r", -950, 450)
    rel_y = mask(material, cone_relative, "g", -950, 520)
    forward_x = mask(material, cone_forward, "r", -950, 590)
    forward_y = mask(material, cone_forward, "g", -950, 660)
    cross_a = binary(
        material, unreal.MaterialExpressionMultiply, rel_x, forward_y, -700, 450
    )
    cross_b = binary(
        material, unreal.MaterialExpressionMultiply, rel_y, forward_x, -700, 580
    )
    cross_signed = binary(
        material, unreal.MaterialExpressionSubtract, cross_a, cross_b, -450, 500
    )
    cross_abs = expr(material, unreal.MaterialExpressionAbs, -200, 500)
    connect(cross_signed, "", cross_abs, "")
    sin_half = scalar_parameter(material, "ConeSinHalfAngle", 0.7071, -700, 750)
    cos_half = scalar_parameter(material, "ConeCosHalfAngle", 0.7071, -700, 850)
    along_sin = binary(
        material, unreal.MaterialExpressionMultiply, along, sin_half, -450, 300
    )
    cross_cos = binary(
        material, unreal.MaterialExpressionMultiply, cross_abs, cos_half, 50, 500
    )
    side_signed = binary(
        material, unreal.MaterialExpressionSubtract, along_sin, cross_cos, 300, 350
    )
    cone_distance = expr(material, unreal.MaterialExpressionLength, -700, 100)
    connect(cone_relative, "", cone_distance, "")
    cone_range = scalar_parameter(material, "ConeRange", 1250.0, -700, 0)
    radial_signed = binary(
        material, unreal.MaterialExpressionSubtract, cone_range, cone_distance, -450, 100
    )
    cone_signed = binary(
        material, unreal.MaterialExpressionMin, side_signed, radial_signed, 550, 250
    )

    width = scalar_parameter(material, "CoverageWidth", 2.5, -450, -350)
    half = scalar(material, 0.5, -450, -250)
    body_normalized = binary(
        material, unreal.MaterialExpressionDivide, body_signed, width, -200, -650
    )
    body_shifted = binary(
        material, unreal.MaterialExpressionAdd, half, body_normalized, 50, -650
    )
    body_coverage = saturate(material, body_shifted, 300, -650)
    cone_normalized = binary(
        material, unreal.MaterialExpressionDivide, cone_signed, width, 800, 250
    )
    cone_shifted = binary(
        material, unreal.MaterialExpressionAdd, half, cone_normalized, 1050, 250
    )
    cone_coverage_raw = saturate(material, cone_shifted, 1300, 250)
    cone_enabled = scalar_parameter(material, "ConeEnabled", 1.0, 1050, 400)
    cone_coverage = binary(
        material,
        unreal.MaterialExpressionMultiply,
        cone_coverage_raw,
        cone_enabled,
        1550,
        300,
    )
    live_coverage = binary(
        material, unreal.MaterialExpressionMax, body_coverage, cone_coverage, 1800, -150
    )
    if not unreal.MaterialEditingLibrary.connect_material_property(
        live_coverage, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError("Could not connect analytic coverage output")
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return material


def create_surface(asset_tools):
    material, path = make_asset(asset_tools, SURFACE_NAME)
    base_texture = unreal.load_asset(
        "/Engine/Engine_MI_Shaders/T_Base_Tile_Diffuse.T_Base_Tile_Diffuse"
    )
    fallback_texture = unreal.load_asset(
        "/Engine/EngineMaterials/DefaultBloomKernel.DefaultBloomKernel"
    )
    if not base_texture or not fallback_texture:
        raise RuntimeError("Required Engine textures are unavailable")

    texcoord = expr(material, unreal.MaterialExpressionTextureCoordinate, -2200, -750)
    uv_scale = scalar_parameter(material, "OriginalUVScale", 18.0, -2200, -620)
    base_uv = binary(
        material, unreal.MaterialExpressionMultiply, texcoord, uv_scale, -1950, -700
    )
    base_sample = expr(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -1700, -750
    )
    base_sample.set_editor_property("parameter_name", "OriginalBaseColorTexture")
    base_sample.set_editor_property("texture", base_texture)
    base_sample.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    )
    connect(base_uv, "", base_sample, "Coordinates")
    tint = vector_parameter(
        material,
        "OriginalBaseColorTint",
        unreal.LinearColor(0.62, 0.72, 0.78, 1.0),
        -1700,
        -580,
    )
    original = binary(
        material, unreal.MaterialExpressionMultiply, base_sample, tint, -1400, -700
    )

    world_position = expr(material, unreal.MaterialExpressionWorldPosition, -2200, -50)
    world_xy = mask(material, world_position, "rg", -1950, -50)
    world_min_param = vector_parameter(
        material, "FogWorldMin", unreal.LinearColor(0, 0, 0, 0), -2200, 100
    )
    world_inv_param = vector_parameter(
        material,
        "FogWorldInvExtent",
        unreal.LinearColor(1.0 / 3500.0, 1.0 / 2500.0, 0, 0),
        -2200,
        230,
    )
    world_min = mask(material, world_min_param, "rg", -1950, 100)
    world_inv = mask(material, world_inv_param, "rg", -1950, 230)
    local_lwc = binary(
        material, unreal.MaterialExpressionSubtract, world_xy, world_min, -1700, 20
    )
    local_float = expr(material, unreal.MaterialExpressionTruncateLWC, -1450, 20)
    connect(local_lwc, "", local_float, "")
    coverage_uv = binary(
        material, unreal.MaterialExpressionMultiply, local_float, world_inv, -1200, 20
    )
    coverage_sample = expr(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -950, 0
    )
    coverage_sample.set_editor_property(
        "parameter_name", "DarkwellLiveCoverageTexture"
    )
    coverage_sample.set_editor_property("texture", fallback_texture)
    coverage_sample.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    )
    # Deliberately leave mip_value_mode at the implicit derivative default.
    connect(coverage_uv, "", coverage_sample, "Coordinates")
    coverage_r = mask(
        material, coverage_sample, "r", -700, 0, source_output="RGBA"
    )
    coverage = saturate(material, coverage_r, -450, 0)
    one_minus_coverage = binary(
        material,
        unreal.MaterialExpressionSubtract,
        scalar(material, 1.0, -450, 150),
        coverage,
        -200,
        100,
    )

    luminance_weights = vector_parameter(
        material,
        "RememberedLuminanceWeights",
        unreal.LinearColor(0.299, 0.587, 0.114, 0),
        -1150,
        -500,
    )
    luminance = expr(material, unreal.MaterialExpressionDotProduct, -900, -500)
    connect(original, "", luminance, "A")
    connect(luminance_weights, "", luminance, "B")
    saturation = scalar_parameter(material, "RememberedSaturation", 0.16, -900, -380)
    remembered_color = expr(material, unreal.MaterialExpressionLinearInterpolate, -650, -500)
    connect(luminance, "", remembered_color, "A")
    connect(original, "", remembered_color, "B")
    connect(saturation, "", remembered_color, "Alpha")
    remembered_brightness = scalar_parameter(
        material, "RememberedBrightness", 0.48, -650, -350
    )
    remembered_filtered = binary(
        material,
        unreal.MaterialExpressionMultiply,
        remembered_color,
        remembered_brightness,
        -400,
        -500,
    )

    live_base = binary(
        material, unreal.MaterialExpressionMultiply, original, coverage, 50, -650
    )
    memory_emissive = binary(
        material,
        unreal.MaterialExpressionMultiply,
        remembered_filtered,
        one_minus_coverage,
        50,
        -420,
    )
    diagnostic = scalar_parameter(material, "DiagnosticRawCoverageView", 0.0, 50, 150)
    diagnostic_inverse = binary(
        material,
        unreal.MaterialExpressionSubtract,
        scalar(material, 1.0, 50, 250),
        diagnostic,
        300,
        200,
    )
    final_base = binary(
        material, unreal.MaterialExpressionMultiply, live_base, diagnostic_inverse, 550, -650
    )
    normal_emissive = binary(
        material,
        unreal.MaterialExpressionMultiply,
        memory_emissive,
        diagnostic_inverse,
        300,
        -420,
    )
    diagnostic_emissive = binary(
        material, unreal.MaterialExpressionMultiply, coverage, diagnostic, 300, -220
    )
    final_emissive = binary(
        material,
        unreal.MaterialExpressionAdd,
        normal_emissive,
        diagnostic_emissive,
        550,
        -320,
    )

    for source, prop in [
        (final_base, unreal.MaterialProperty.MP_BASE_COLOR),
        (final_emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
    ]:
        if not unreal.MaterialEditingLibrary.connect_material_property(source, "", prop):
            raise RuntimeError(f"Could not connect material property {prop}")
    roughness = scalar_parameter(material, "OriginalRoughness", 0.72, 550, -50)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return material


def main():
    unreal.EditorAssetLibrary.make_directory(ASSET_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    create_coverage(tools)
    create_surface(tools)
    unreal.log("DARKWELL_PROJECT_FOG_MATERIALS_CREATED")


if __name__ == "__main__":
    main()
