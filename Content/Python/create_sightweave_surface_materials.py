import unreal


ASSET_PATH = "/Game/Darkwell/Vision/Materials"
FUNCTION_NAME = "MF_DarkwellSightWeaveSurface"
MASTER_NAME = "M_DarkwellSightWeaveSurface"


def expr(owner, expression_class, x, y, function=False):
    create = (
        unreal.MaterialEditingLibrary.create_material_expression_in_function
        if function
        else unreal.MaterialEditingLibrary.create_material_expression
    )
    return create(owner, expression_class, x, y)


def connect(source, output_name, target, input_name):
    candidates = [input_name]
    candidates.extend(
        {
            "AGreaterThanB": ["A > B", "A>B"],
            "AEqualsB": ["A == B", "A = B", "A==B"],
            "ALessThanB": ["A < B", "A<B"],
            "Coordinates": ["UVs"],
        }.get(input_name, [])
    )
    for candidate in candidates:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output_name, target, candidate
        ):
            return
    else:
        available = unreal.MaterialEditingLibrary.get_material_expression_input_names(
            target
        )
        if len(available) == 1 and unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output_name, target, ""
        ):
            return
        raise RuntimeError(
            f"Could not connect {source.get_name()}.{output_name} "
            f"to {target.get_name()}.{input_name}; inputs={available}"
        )


def scalar(owner, value, x, y, function=False):
    node = expr(owner, unreal.MaterialExpressionConstant, x, y, function)
    node.set_editor_property("r", value)
    return node


def function_input(function, name, input_type, x, y):
    node = expr(
        function, unreal.MaterialExpressionFunctionInput, x, y, function=True
    )
    node.set_editor_property("input_name", name)
    node.set_editor_property("input_type", input_type)
    return node


def function_output(function, name, source, output_name, x, y):
    node = expr(
        function, unreal.MaterialExpressionFunctionOutput, x, y, function=True
    )
    node.set_editor_property("output_name", name)
    connect(source, output_name, node, "")
    return node


def make_binary(owner, klass, a, b, x, y, function=False):
    node = expr(owner, klass, x, y, function)
    connect(a, "", node, "A")
    connect(b, "", node, "B")
    return node


def make_mask(owner, source, channels, x, y, function=False):
    node = expr(owner, unreal.MaterialExpressionComponentMask, x, y, function)
    node.set_editor_property("r", "r" in channels)
    node.set_editor_property("g", "g" in channels)
    node.set_editor_property("b", "b" in channels)
    node.set_editor_property("a", "a" in channels)
    connect(source, "", node, "")
    return node


def create_function(asset_tools):
    full_path = f"{ASSET_PATH}/{FUNCTION_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
    function = asset_tools.create_asset(
        FUNCTION_NAME,
        ASSET_PATH,
        unreal.MaterialFunction,
        unreal.MaterialFunctionFactoryNew(),
    )
    if not function:
        raise RuntimeError("Could not create SightWeave material function")

    attrs = function_input(
        function,
        "OriginalMaterialAttributes",
        unreal.FunctionInputType.FUNCTION_INPUT_MATERIAL_ATTRIBUTES,
        -1800,
        -600,
    )
    state_center = function_input(
        function, "StateCenter", unreal.FunctionInputType.FUNCTION_INPUT_VECTOR4, -1800, -350
    )
    state_positive = function_input(
        function, "StatePositive", unreal.FunctionInputType.FUNCTION_INPUT_VECTOR4, -1800, -200
    )
    state_negative = function_input(
        function, "StateNegative", unreal.FunctionInputType.FUNCTION_INPUT_VECTOR4, -1800, -50
    )
    category = function_input(
        function, "SurfaceCategory", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -1800, 150
    )
    memory_saturation = function_input(
        function, "MemorySaturation", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -1800, 350
    )
    memory_brightness = function_input(
        function, "MemoryBrightness", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -1800, 500
    )
    memory_contrast = function_input(
        function, "MemoryContrast", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -1800, 650
    )

    state_max_a = make_binary(
        function, unreal.MaterialExpressionMax, state_center, state_positive, -1500, -200, True
    )
    state_max = make_binary(
        function, unreal.MaterialExpressionMax, state_max_a, state_negative, -1300, -150, True
    )
    category_saturate = expr(
        function, unreal.MaterialExpressionSaturate, -1500, 100, function=True
    )
    connect(category, "", category_saturate, "")
    conservative_state = expr(
        function, unreal.MaterialExpressionLinearInterpolate, -1050, -150, function=True
    )
    connect(state_center, "", conservative_state, "A")
    connect(state_max, "", conservative_state, "B")
    connect(category_saturate, "", conservative_state, "Alpha")

    state_r = make_mask(function, conservative_state, "r", -800, -250, True)
    feather_g = make_mask(function, conservative_state, "g", -800, -100, True)
    memory_b = make_mask(function, conservative_state, "b", -800, 50, True)
    valid_a = make_mask(function, conservative_state, "a", -800, 200, True)

    threshold_live = scalar(function, 0.75, -800, -400, True)
    one = scalar(function, 1.0, -800, 350, True)
    zero = scalar(function, 0.0, -800, 450, True)
    live_threshold_delta = make_binary(
        function, unreal.MaterialExpressionSubtract, state_r, threshold_live, -550, -350, True
    )
    live_threshold_scale = scalar(function, 10000.0, -550, -450, True)
    live_threshold_ramp = make_binary(
        function,
        unreal.MaterialExpressionMultiply,
        live_threshold_delta,
        live_threshold_scale,
        -300,
        -350,
        True,
    )
    live_if = expr(function, unreal.MaterialExpressionSaturate, -100, -350, function=True)
    connect(live_threshold_ramp, "", live_if, "")
    live_max = make_binary(
        function, unreal.MaterialExpressionMax, live_if, feather_g, -300, -200, True
    )
    live_weight = make_binary(
        function, unreal.MaterialExpressionMultiply, live_max, valid_a, -50, -200, True
    )

    category_limit = scalar(function, 2.5, -800, 600, True)
    memory_category_delta = make_binary(
        function, unreal.MaterialExpressionSubtract, category_limit, category, -550, 500, True
    )
    memory_category_scale = scalar(function, 2.0, -550, 650, True)
    memory_category_ramp = make_binary(
        function,
        unreal.MaterialExpressionMultiply,
        memory_category_delta,
        memory_category_scale,
        -300,
        500,
        True,
    )
    memory_eligible = expr(function, unreal.MaterialExpressionSaturate, -100, 500, function=True)
    connect(memory_category_ramp, "", memory_eligible, "")
    memory_valid = make_binary(
        function, unreal.MaterialExpressionMultiply, memory_b, memory_eligible, -300, 300, True
    )
    inverse_live = make_binary(
        function, unreal.MaterialExpressionSubtract, one, live_weight, -50, 100, True
    )
    memory_not_live = make_binary(
        function, unreal.MaterialExpressionMultiply, memory_valid, inverse_live, 200, 250, True
    )
    memory_weight = make_binary(
        function, unreal.MaterialExpressionMultiply, memory_not_live, valid_a, 450, 250, True
    )

    break_attrs = expr(
        function, unreal.MaterialExpressionBreakMaterialAttributes, -1050, -650, function=True
    )
    connect(attrs, "", break_attrs, "")
    weights = expr(
        function, unreal.MaterialExpressionConstant3Vector, -500, -650, function=True
    )
    weights.set_editor_property(
        "constant", unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0)
    )
    luminance = expr(function, unreal.MaterialExpressionDotProduct, -250, -650, function=True)
    connect(break_attrs, "BaseColor", luminance, "A")
    connect(weights, "", luminance, "B")
    memory_color = expr(
        function, unreal.MaterialExpressionLinearInterpolate, 0, -600, function=True
    )
    connect(luminance, "", memory_color, "A")
    connect(break_attrs, "BaseColor", memory_color, "B")
    connect(memory_saturation, "", memory_color, "Alpha")
    half = scalar(function, 0.5, 0, -450, True)
    contrast_sub = make_binary(
        function, unreal.MaterialExpressionSubtract, memory_color, half, 250, -600, True
    )
    contrast_mul = make_binary(
        function, unreal.MaterialExpressionMultiply, contrast_sub, memory_contrast, 500, -600, True
    )
    contrast_add = make_binary(
        function, unreal.MaterialExpressionAdd, contrast_mul, half, 750, -600, True
    )
    contrast_sat = expr(function, unreal.MaterialExpressionSaturate, 1000, -600, function=True)
    connect(contrast_add, "", contrast_sat, "")
    filtered_memory = make_binary(
        function, unreal.MaterialExpressionMultiply, contrast_sat, memory_brightness, 1250, -600, True
    )

    live_base = expr(function, unreal.MaterialExpressionMultiply, 250, -850, function=True)
    connect(break_attrs, "BaseColor", live_base, "A")
    connect(live_weight, "", live_base, "B")
    memory_emissive = make_binary(
        function, unreal.MaterialExpressionMultiply, filtered_memory, memory_weight, 1500, -500, True
    )
    live_emissive = expr(function, unreal.MaterialExpressionMultiply, 500, -750, function=True)
    connect(break_attrs, "EmissiveColor", live_emissive, "A")
    connect(live_weight, "", live_emissive, "B")
    final_emissive = make_binary(
        function, unreal.MaterialExpressionAdd, live_emissive, memory_emissive, 1750, -600, True
    )
    live_metallic = expr(function, unreal.MaterialExpressionMultiply, 500, -100, function=True)
    connect(break_attrs, "Metallic", live_metallic, "A")
    connect(live_weight, "", live_metallic, "B")
    live_specular = expr(function, unreal.MaterialExpressionMultiply, 500, 0, function=True)
    connect(break_attrs, "Specular", live_specular, "A")
    connect(live_weight, "", live_specular, "B")
    final_roughness = expr(
        function, unreal.MaterialExpressionLinearInterpolate, 500, 100, function=True
    )
    connect(one, "", final_roughness, "A")
    connect(break_attrs, "Roughness", final_roughness, "B")
    connect(live_weight, "", final_roughness, "Alpha")

    make_attrs = expr(
        function, unreal.MaterialExpressionMakeMaterialAttributes, 2050, -500, function=True
    )
    connect(live_base, "", make_attrs, "BaseColor")
    connect(live_metallic, "", make_attrs, "Metallic")
    connect(live_specular, "", make_attrs, "Specular")
    connect(final_roughness, "", make_attrs, "Roughness")
    connect(final_emissive, "", make_attrs, "EmissiveColor")
    connect(break_attrs, "Normal", make_attrs, "Normal")
    connect(break_attrs, "AmbientOcclusion", make_attrs, "AmbientOcclusion")
    function_output(function, "MaterialAttributes", make_attrs, "", 2300, -500)

    unreal.MaterialEditingLibrary.update_material_function(function, None)
    unreal.EditorAssetLibrary.save_asset(full_path, only_if_is_dirty=False)
    return function


def vector_parameter(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def scalar_parameter(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def state_sample(material, texture, uv, x, y):
    node = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", "SightWeaveStateTexture")
    node.set_editor_property("texture", texture)
    node.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    )
    connect(uv, "", node, "Coordinates")
    return node


def create_master(asset_tools, function):
    full_path = f"{ASSET_PATH}/{MASTER_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
    material = asset_tools.create_asset(
        MASTER_NAME, ASSET_PATH, unreal.Material, unreal.MaterialFactoryNew()
    )
    if not material:
        raise RuntimeError("Could not create SightWeave surface master")
    material.set_editor_property("use_material_attributes", True)

    base_texture = unreal.load_asset(
        "/Engine/Engine_MI_Shaders/T_Base_Tile_Diffuse.T_Base_Tile_Diffuse"
    )
    if not base_texture:
        raise RuntimeError("Engine tile BaseColor texture is unavailable")
    base_sample = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -1800, -700)
    base_sample.set_editor_property("parameter_name", "OriginalBaseColorTexture")
    base_sample.set_editor_property("texture", base_texture)
    base_sample.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    )
    texcoord = expr(material, unreal.MaterialExpressionTextureCoordinate, -2050, -700)
    uv_scale = scalar_parameter(material, "OriginalUVScale", 8.0, -2050, -550)
    scaled_uv = make_binary(
        material, unreal.MaterialExpressionMultiply, texcoord, uv_scale, -1800, -550
    )
    connect(scaled_uv, "", base_sample, "Coordinates")
    tint = vector_parameter(
        material, "OriginalBaseColorTint", unreal.LinearColor(0.7, 0.7, 0.7, 1.0), -1550, -550
    )
    tinted_base = make_binary(
        material, unreal.MaterialExpressionMultiply, base_sample, tint, -1300, -650
    )
    make_attrs = expr(material, unreal.MaterialExpressionMakeMaterialAttributes, -1000, -650)
    connect(tinted_base, "", make_attrs, "BaseColor")
    connect(scalar_parameter(material, "OriginalMetallic", 0.0, -1300, -450), "", make_attrs, "Metallic")
    connect(scalar_parameter(material, "OriginalSpecular", 0.35, -1300, -350), "", make_attrs, "Specular")
    connect(scalar_parameter(material, "OriginalRoughness", 0.72, -1300, -250), "", make_attrs, "Roughness")
    connect(scalar(material, 1.0, -1300, -150), "", make_attrs, "AmbientOcclusion")

    world_position = expr(material, unreal.MaterialExpressionWorldPosition, -2050, 0)
    world_xy = make_mask(material, world_position, "rg", -1800, 0)
    world_min_parameter = vector_parameter(
        material,
        "SightWeaveWorldMin",
        unreal.LinearColor(0.0, 0.0, 0.0, 0.0),
        -2050,
        180,
    )
    world_inv_extent_parameter = vector_parameter(
        material,
        "SightWeaveWorldInvExtent",
        unreal.LinearColor(0.0001, 0.0001, 0.0, 0.0),
        -2050,
        320,
    )
    world_min = make_mask(material, world_min_parameter, "rg", -1800, 150)
    world_inv_extent = make_mask(
        material, world_inv_extent_parameter, "rg", -1800, 300
    )
    local_world = make_binary(
        material, unreal.MaterialExpressionSubtract, world_xy, world_min, -1550, 50
    )
    center_uv = make_binary(
        material, unreal.MaterialExpressionMultiply, local_world, world_inv_extent, -1300, 50
    )
    normal = expr(material, unreal.MaterialExpressionPixelNormalWS, -2050, 500)
    normal_xy = make_mask(material, normal, "rg", -1800, 500)
    normal_xy_normalized = expr(material, unreal.MaterialExpressionNormalize, -1550, 500)
    connect(normal_xy, "", normal_xy_normalized, "")
    wall_bias = scalar_parameter(material, "SightWeaveWallSampleBiasCm", 7.5, -1550, 650)
    bias_world = make_binary(
        material, unreal.MaterialExpressionMultiply, normal_xy_normalized, wall_bias, -1300, 500
    )
    bias_uv = make_binary(
        material, unreal.MaterialExpressionMultiply, bias_world, world_inv_extent, -1050, 500
    )
    positive_uv = make_binary(
        material, unreal.MaterialExpressionAdd, center_uv, bias_uv, -800, 400
    )
    negative_uv = make_binary(
        material, unreal.MaterialExpressionSubtract, center_uv, bias_uv, -800, 550
    )
    # The default must be a linear/mask texture so the graph compiles against the
    # non-sRGB runtime render target before a MID binds the real state texture.
    empty_state = unreal.load_asset(
        "/Engine/EngineMaterials/DefaultDiffuse_TC_Masks.DefaultDiffuse_TC_Masks"
    )
    center_state = state_sample(material, empty_state, center_uv, -550, 0)
    positive_state = state_sample(material, empty_state, positive_uv, -550, 250)
    negative_state = state_sample(material, empty_state, negative_uv, -550, 500)

    category = scalar_parameter(material, "SightWeaveSurfaceCategory", 0.0, -550, 750)
    category.set_editor_property("use_custom_primitive_data", True)
    category.set_editor_property("primitive_data_index", 0)
    memory_saturation = scalar_parameter(material, "RememberedSaturation", 0.18, -300, 750)
    memory_brightness = scalar_parameter(material, "RememberedBrightness", 0.32, -300, 850)
    memory_contrast = scalar_parameter(material, "RememberedContrast", 0.68, -300, 950)

    call = expr(material, unreal.MaterialExpressionMaterialFunctionCall, 0, 0)
    call.set_material_function(function)
    connect(make_attrs, "", call, "OriginalMaterialAttributes")
    connect(center_state, "RGBA", call, "StateCenter")
    connect(positive_state, "RGBA", call, "StatePositive")
    connect(negative_state, "RGBA", call, "StateNegative")
    connect(category, "", call, "SurfaceCategory")
    connect(memory_saturation, "", call, "MemorySaturation")
    connect(memory_brightness, "", call, "MemoryBrightness")
    connect(memory_contrast, "", call, "MemoryContrast")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        call, "MaterialAttributes", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES
    ):
        raise RuntimeError("Could not connect surface attributes to the material")

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(full_path, only_if_is_dirty=False)
    return material


def create_instance(asset_tools, name, parent, uv_scale, tint):
    full_path = f"{ASSET_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
    instance = asset_tools.create_asset(
        name,
        ASSET_PATH,
        unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew(),
    )
    instance.set_editor_property("parent", parent)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        instance, "OriginalUVScale", uv_scale
    )
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, "OriginalBaseColorTint", tint
    )
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    unreal.EditorAssetLibrary.save_asset(full_path, only_if_is_dirty=False)
    return instance


def main():
    unreal.EditorAssetLibrary.make_directory(ASSET_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    function = create_function(tools)
    master = create_master(tools, function)
    create_instance(
        tools,
        "MI_DarkwellSightWeaveFloor",
        master,
        18.0,
        unreal.LinearColor(0.62, 0.72, 0.78, 1.0),
    )
    create_instance(
        tools,
        "MI_DarkwellSightWeaveWall",
        master,
        6.0,
        unreal.LinearColor(0.55, 0.58, 0.62, 1.0),
    )
    create_instance(
        tools,
        "MI_DarkwellSightWeaveStatic",
        master,
        3.0,
        unreal.LinearColor(0.78, 0.46, 0.24, 1.0),
    )
    unreal.log("DARKWELL SightWeave surface materials created and saved")


if __name__ == "__main__":
    main()
