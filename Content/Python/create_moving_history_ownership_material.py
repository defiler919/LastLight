"""Project-side moving Lab derivative; leaves all accepted manual materials intact."""
import unreal
import create_darkwell_project_fog_materials as m

lib = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
root = '/Game/Darkwell/Vision/PropLab/'
assert not assets.does_asset_exist(root + 'M_MovingAccumulatedMemory'), 'Do not overwrite an existing asset'
material = assets.duplicate_asset(root + 'M_ManualAccumulatedMemory', root + 'M_MovingAccumulatedMemory')
opacity = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY)
assert isinstance(opacity, unreal.MaterialExpressionCustom)
assert opacity.get_editor_property('code') == 'return Ready > 0.5 ? saturate(State.b) : 0.0;'
state = next(n for n in lib.get_inputs_for_material_expression(material, opacity)
             if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D))
uv = lib.get_inputs_for_material_expression(material, state)[0]
ownership = m.expr(material, unreal.MaterialExpressionTextureObjectParameter, -1100, 4700)
ownership.set_editor_property('parameter_name', 'SpatialStateTexture')
ownership.set_editor_property('texture', state.get_editor_property('texture'))
ownership.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
inputs = list(opacity.get_editor_property('inputs'))
for name in ('Ownership', 'UV'):
    item = unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
opacity.set_editor_property('inputs', inputs)
m.connect(ownership, '', opacity, 'Ownership')
m.connect(uv, '', opacity, 'UV')
opacity.set_editor_property('code', '''uint Width, Height;
Ownership.GetDimensions(Width, Height);
int2 Pixel = clamp(int2(floor(UV * float2(Width, Height))), int2(0,0), int2(Width,Height)-1);
float HardOwnership = Ownership.Load(int3(Pixel,0)).a;
return Ready > 0.5 ? saturate(State.b) * (HardOwnership > 0.5 ? 1.0 : 0.0) : 0.0;''')
opacity.set_editor_property('description', 'FINAL unfiltered ownership gate after frozen bilinear history; no D/V/R writes')
assert material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
assert lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET) is None
lib.recompile_material(material)
assert assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log('MOVING_HISTORY_MATERIAL_SAVED final_gate=Load_A smooth=bilinear_B frozen_manual_assets=untouched')
