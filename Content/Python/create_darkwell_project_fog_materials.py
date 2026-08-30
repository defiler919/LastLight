import unreal


ASSET_PATH = "/Game/Darkwell/Vision/ProjectFog"
COVERAGE_NAME = "M_DarkwellFogCoverage"
SURFACE_NAME = "M_DarkwellFogSurface"


def expr(material, klass, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, klass, x, y)


def connect(source, output_name, target, input_name):
    aliases = {
        "Coordinates": ["UVs"],
        "LightValue": ["Light Value", "LightValueInput"],
        "Alpha": ["AlphaInput"],
        "StaticIndirect": ["Static Indirect"],
        "DynamicIndirect": ["Dynamic Indirect"],
    }
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


def custom_expression(material, code, named_inputs, x, y, description):
    node = expr(material, unreal.MaterialExpressionCustom, x, y)
    inputs = []
    for item in named_inputs:
        name = item[0]
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    node.set_editor_property("inputs", inputs)
    node.set_editor_property("code", code)
    node.set_editor_property("description", description)
    for item in named_inputs:
        name, source = item[0], item[1]
        source_output = item[2] if len(item) > 2 else ""
        connect(source, source_output, node, name)
    return node


def sample_live_coverage(
    material, world_point, world_min, world_inv, fallback_texture, x, y
):
    local = binary(
        material, unreal.MaterialExpressionSubtract, world_point, world_min, x, y
    )
    uv = binary(
        material, unreal.MaterialExpressionMultiply, local, world_inv, x + 220, y
    )
    sample = expr(
        material, unreal.MaterialExpressionTextureSampleParameter2D, x + 440, y
    )
    sample.set_editor_property("parameter_name", "DarkwellLiveCoverageTexture")
    sample.set_editor_property("texture", fallback_texture)
    sample.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    )
    connect(uv, "", sample, "Coordinates")
    red = mask(material, sample, "r", x + 660, y, source_output="RGBA")
    return saturate(material, red, x + 820, y)


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
    segment_count = scalar_parameter(
        material, "OccluderSegmentCount", 0.0, 1050, 650
    )
    segment_parameters = []
    for index in range(16):
        segment_parameters.append(
            vector_parameter(
                material,
                f"OccluderSegment{index}",
                unreal.LinearColor(0, 0, 0, 0),
                800 + (index % 4) * 220,
                850 + (index // 4) * 120,
            )
        )
    segment_array = ", ".join(f"OccluderSegment{i}" for i in range(16))
    occlusion_code = f"""
float2 Ray = WorldXY - SourceXY;
float RayLengthSquared = dot(Ray, Ray);
if (RayLengthSquared < 1.0e-6)
{{
    return 1.0;
}}
float4 Segments[16] = {{ {segment_array} }};
float Visibility = 1.0;
[unroll]
for (int Index = 0; Index < 16; ++Index)
{{
    float Enabled = step((float)Index + 0.5, OccluderSegmentCount);
    float2 A = Segments[Index].xy;
    float2 Edge = Segments[Index].zw - A;
    float2 RelativeA = A - SourceXY;
    float Denominator = Ray.x * Edge.y - Ray.y * Edge.x;
    float ValidDenominator = step(1.0e-5, abs(Denominator));
    float SafeDenominator = Denominator >= 0.0
        ? max(Denominator, 1.0e-5)
        : min(Denominator, -1.0e-5);
    float T = (RelativeA.x * Edge.y - RelativeA.y * Edge.x) / SafeDenominator;
    float U = (RelativeA.x * Ray.y - RelativeA.y * Ray.x) / SafeDenominator;
    float IntersectionMargin = min(min(T, 1.0 - T), min(U, 1.0 - U));
    float AntialiasWidth = max(fwidth(IntersectionMargin), 1.0e-4);
    float Blocked = smoothstep(
        -AntialiasWidth,
        AntialiasWidth,
        IntersectionMargin) * ValidDenominator * Enabled;
    Visibility *= 1.0 - Blocked;
}}
return saturate(Visibility);
"""
    shared_occlusion_inputs = [("OccluderSegmentCount", segment_count)] + [
        (f"OccluderSegment{index}", parameter, "RGBA")
        for index, parameter in enumerate(segment_parameters)
    ]
    body_visibility = custom_expression(
        material,
        occlusion_code,
        [("WorldXY", world), ("SourceXY", body_center)]
        + shared_occlusion_inputs,
        1550,
        -650,
        "Continuous body line-segment visibility",
    )
    cone_visibility = custom_expression(
        material,
        occlusion_code,
        [("WorldXY", world), ("SourceXY", cone_origin)]
        + shared_occlusion_inputs,
        1800,
        400,
        "Continuous cone line-segment visibility",
    )
    visible_body = binary(
        material,
        unreal.MaterialExpressionMultiply,
        body_coverage,
        body_visibility,
        2050,
        -550,
    )
    visible_cone = binary(
        material,
        unreal.MaterialExpressionMultiply,
        cone_coverage,
        cone_visibility,
        2050,
        250,
    )
    live_coverage = binary(
        material, unreal.MaterialExpressionMax, visible_body, visible_cone, 2300, -150
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
    world_float = expr(material, unreal.MaterialExpressionTruncateLWC, -1700, -50)
    connect(world_xy, "", world_float, "")
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
    # Every sample deliberately uses the implicit derivative/mip path.
    ground_coverage = sample_live_coverage(
        material, world_float, world_min, world_inv, fallback_texture, -1500, 0
    )

    surface_origin_param = vector_parameter(
        material, "SurfaceOrigin", unreal.LinearColor(0, 0, 0, 0), -2200, 420
    )
    surface_normal_param = vector_parameter(
        material, "SurfaceNormal", unreal.LinearColor(1, 0, 0, 0), -2200, 540
    )
    surface_tangent_param = vector_parameter(
        material, "SurfaceTangent", unreal.LinearColor(0, 1, 0, 0), -2200, 660
    )
    surface_origin = mask(material, surface_origin_param, "rg", -1950, 420)
    surface_normal = mask(material, surface_normal_param, "rg", -1950, 540)
    surface_tangent = mask(material, surface_tangent_param, "rg", -1950, 660)
    surface_relative = binary(
        material,
        unreal.MaterialExpressionSubtract,
        world_float,
        surface_origin,
        -1700,
        520,
    )
    along_tangent = expr(material, unreal.MaterialExpressionDotProduct, -1450, 520)
    connect(surface_relative, "", along_tangent, "A")
    connect(surface_tangent, "", along_tangent, "B")
    tangent_offset = binary(
        material,
        unreal.MaterialExpressionMultiply,
        surface_tangent,
        along_tangent,
        -1200,
        520,
    )
    centerline_point = binary(
        material,
        unreal.MaterialExpressionAdd,
        surface_origin,
        tangent_offset,
        -950,
        520,
    )
    wall_sample_distance = scalar_parameter(
        material, "WallSampleDistance", 27.5, -1200, 680
    )
    normal_offset = binary(
        material,
        unreal.MaterialExpressionMultiply,
        surface_normal,
        wall_sample_distance,
        -950,
        680,
    )
    wall_side_a = binary(
        material,
        unreal.MaterialExpressionAdd,
        centerline_point,
        normal_offset,
        -700,
        500,
    )
    wall_side_b = binary(
        material,
        unreal.MaterialExpressionSubtract,
        centerline_point,
        normal_offset,
        -700,
        660,
    )
    wall_coverage_a = sample_live_coverage(
        material, wall_side_a, world_min, world_inv, fallback_texture, -450, 460
    )
    wall_coverage_b = sample_live_coverage(
        material, wall_side_b, world_min, world_inv, fallback_texture, -450, 700
    )
    wall_coverage = binary(
        material,
        unreal.MaterialExpressionMax,
        wall_coverage_a,
        wall_coverage_b,
        500,
        580,
    )

    box_sample_names = (
        "BoxSamplePositiveX",
        "BoxSampleNegativeX",
        "BoxSamplePositiveY",
        "BoxSampleNegativeY",
    )
    box_coverages = []
    for index, name in enumerate(box_sample_names):
        parameter = vector_parameter(
            material,
            name,
            unreal.LinearColor(0, 0, 0, 0),
            -2200,
            900 + index * 110,
        )
        point = mask(material, parameter, "rg", -1950, 900 + index * 110)
        box_coverages.append(
            sample_live_coverage(
                material,
                point,
                world_min,
                world_inv,
                fallback_texture,
                -1700,
                880 + index * 220,
            )
        )
    box_max_x = binary(
        material,
        unreal.MaterialExpressionMax,
        box_coverages[0],
        box_coverages[1],
        -600,
        1020,
    )
    box_max_y = binary(
        material,
        unreal.MaterialExpressionMax,
        box_coverages[2],
        box_coverages[3],
        -600,
        1320,
    )
    box_coverage = binary(
        material, unreal.MaterialExpressionMax, box_max_x, box_max_y, -350, 1170
    )

    ground_weight = scalar_parameter(
        material, "GroundCoverageWeight", 1.0, 700, 0
    )
    wall_weight = scalar_parameter(
        material, "WallCoverageWeight", 0.0, 700, 120
    )
    box_weight = scalar_parameter(material, "BoxCoverageWeight", 0.0, 700, 240)
    weighted_ground = binary(
        material,
        unreal.MaterialExpressionMultiply,
        ground_coverage,
        ground_weight,
        950,
        0,
    )
    weighted_wall = binary(
        material,
        unreal.MaterialExpressionMultiply,
        wall_coverage,
        wall_weight,
        950,
        120,
    )
    weighted_box = binary(
        material,
        unreal.MaterialExpressionMultiply,
        box_coverage,
        box_weight,
        950,
        240,
    )
    surface_sum = binary(
        material,
        unreal.MaterialExpressionAdd,
        weighted_ground,
        weighted_wall,
        1200,
        80,
    )
    surface_coverage = saturate(
        material,
        binary(
            material,
            unreal.MaterialExpressionAdd,
            surface_sum,
            weighted_box,
            1450,
            140,
        ),
        1650,
        140,
    )
    force_remembered = scalar_parameter(
        material, "ForceRemembered", 0.0, 1200, 320
    )
    allow_live = binary(
        material,
        unreal.MaterialExpressionSubtract,
        scalar(material, 1.0, 1200, 420),
        force_remembered,
        1450,
        360,
    )
    effective_coverage = binary(
        material,
        unreal.MaterialExpressionMultiply,
        surface_coverage,
        allow_live,
        1800,
        220,
    )
    one_minus_coverage = binary(
        material,
        unreal.MaterialExpressionSubtract,
        scalar(material, 1.0, 1800, 420),
        effective_coverage,
        2050,
        320,
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
        material, unreal.MaterialExpressionMultiply, original, effective_coverage, 50, -650
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
    # Surface emissive is display-only. EyeAdaptationInverse keeps a fully
    # Remembered surface at a fixed display brightness while the player moves a
    # Torch or switches tools. GIReplace supplies zero to both static and
    # dynamic indirect-light captures, so this presentation value cannot become
    # a Lumen/Lightmass light source.
    exposure_stable_memory = expr(
        material, unreal.MaterialExpressionEyeAdaptationInverse, 520, -470
    )
    connect(normal_emissive, "", exposure_stable_memory, "LightValue")
    connect(scalar(material, 1.0, 300, -540), "", exposure_stable_memory, "Alpha")
    display_only_memory = expr(
        material, unreal.MaterialExpressionGIReplace, 760, -430
    )
    connect(exposure_stable_memory, "", display_only_memory, "Default")
    zero_indirect = scalar(material, 0.0, 520, -290)
    connect(zero_indirect, "", display_only_memory, "StaticIndirect")
    connect(zero_indirect, "", display_only_memory, "DynamicIndirect")
    diagnostic_emissive = binary(
        material, unreal.MaterialExpressionMultiply, effective_coverage, diagnostic, 300, -220
    )
    final_emissive = binary(
        material,
        unreal.MaterialExpressionAdd,
        display_only_memory,
        diagnostic_emissive,
        1000,
        -320,
    )

    for source, prop in [
        (final_base, unreal.MaterialProperty.MP_BASE_COLOR),
        (final_emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
    ]:
        if not unreal.MaterialEditingLibrary.connect_material_property(source, "", prop):
            raise RuntimeError(f"Could not connect material property {prop}")
    original_roughness = scalar_parameter(
        material, "OriginalRoughness", 0.72, 550, -50
    )
    roughness = expr(material, unreal.MaterialExpressionLinearInterpolate, 800, -50)
    connect(scalar(material, 1.0, 550, 20), "", roughness, "A")
    connect(original_roughness, "", roughness, "B")
    connect(effective_coverage, "", roughness, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    original_specular = scalar_parameter(
        material, "OriginalSpecular", 0.5, 550, 90
    )
    live_specular = binary(
        material,
        unreal.MaterialExpressionMultiply,
        original_specular,
        effective_coverage,
        800,
        90,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        live_specular, "", unreal.MaterialProperty.MP_SPECULAR
    )
    # Gray has no lit AO channel; Live retains the normal material response.
    unreal.MaterialEditingLibrary.connect_material_property(
        effective_coverage, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
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
