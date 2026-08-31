"""One reviewed asset migration, via UE APIs. Never saves a map or changes WPO.

The original fixed mesh uses state R for persistent discovered opacity and G
for local live/gray blending. The EXISTING snapshot proxy uses disjoint B.
The same original mesh's ShadowPassSwitch stays constant one.
"""
import unreal
import create_darkwell_project_fog_materials as m

lib=unreal.MaterialEditingLibrary
assets=unreal.EditorAssetLibrary
root='/Game/Darkwell/Vision/PropLab/'
surface=unreal.load_asset(root+'M_ManualFixedReveal')
assert surface

def reachable(material):
    pending=[lib.get_material_property_input_node(material,p) for p in
             (unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_EMISSIVE_COLOR,
              unreal.MaterialProperty.MP_OPACITY_MASK,unreal.MaterialProperty.MP_OPACITY)]
    seen=[]
    while pending:
        n=pending.pop()
        if not n or n in seen:continue
        seen.append(n)
        pending.extend(lib.get_inputs_for_material_expression(material,n))
    return seen

def state_texture(material):
    world=m.expr(material,unreal.MaterialExpressionWorldPosition,-2000,4000)
    world.set_editor_property('world_position_shader_offset',unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    xy=m.mask(material,world,'rg',-1800,4000)
    domain=m.vector_parameter(material,'SpatialMinInv',unreal.LinearColor(0,0,0,0),-2000,4200)
    minimum=m.mask(material,domain,'rg',-1800,4200)
    inverse=m.mask(material,domain,'ba',-1800,4400,source_output='RGBA')
    uv=m.binary(material,unreal.MaterialExpressionMultiply,
                m.binary(material,unreal.MaterialExpressionSubtract,xy,minimum,-1550,4000),inverse,-1350,4000)
    texture=m.expr(material,unreal.MaterialExpressionTextureSampleParameter2D,-1100,4000)
    texture.set_editor_property('parameter_name','SpatialStateTexture')
    texture.set_editor_property('texture',unreal.load_asset('/Engine/EngineMaterials/DefaultBloomKernel'))
    texture.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    m.connect(uv,'',texture,'Coordinates')
    ready=m.scalar_parameter(material,'SpatialReady',0,-1100,4350)
    return texture,ready

nodes=reachable(surface)
assert not any(isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and
               str(n.get_editor_property('parameter_name'))=='SpatialStateTexture' for n in nodes),'Migration already applied'
state,ready=state_texture(surface)
enabled=m.scalar_parameter(surface,'FixedRevealEnabled',0,-1100,4500)
alpha=m.custom_expression(surface,
    'return Enabled > 0.5 ? (Ready > 0.5 ? saturate(State.r) : 0.0) : 1.0;',
    [('State',state,'RGB'),('Enabled',enabled),('Ready',ready)],-700,4000,
    'Persistent per-position discovery: coverage exit changes color, never erases known surface')
shadow=lib.get_material_property_input_node(surface,unreal.MaterialProperty.MP_OPACITY_MASK)
assert isinstance(shadow,unreal.MaterialExpressionShadowReplace)
dither=next(n for n in lib.get_inputs_for_material_expression(surface,shadow) if isinstance(n,unreal.MaterialExpressionMaterialFunctionCall))
m.connect(alpha,'',dither,'Alpha Threshold')
weights=[n for n in nodes if isinstance(n,unreal.MaterialExpressionCustom) and
         n.get_editor_property('code')=='return lerp(lerp(Raw, min(Raw, Soft), UseSoft), 1.0, Whole);']
assert len(weights)==1
weight=weights[0]
inputs=list(weight.get_editor_property('inputs'))
for name in ('State','Enabled','Ready'):
    i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
weight.set_editor_property('inputs',inputs)
m.connect(state,'RGB',weight,'State');m.connect(enabled,'',weight,'Enabled');m.connect(ready,'',weight,'Ready')
weight.set_editor_property('code','return Enabled > 0.5 ? (Ready > 0.5 ? saturate(State.g) : 0.0) : lerp(lerp(Raw, min(Raw, Soft), UseSoft), 1.0, Whole);')
weight.set_editor_property('description','Mode 2 local live-to-gray history; original Mode 0/1 branch unchanged')

assert not assets.does_asset_exist(root+'M_ManualAccumulatedMemory')
proxy=assets.duplicate_asset(root+'M_PropLabStaleMemory',root+'M_ManualAccumulatedMemory')
assert proxy
state,ready=state_texture(proxy)
opacity=m.custom_expression(proxy,'return Ready > 0.5 ? saturate(State.b) : 0.0;',
    [('State',state,'RGB'),('Ready',ready)],-700,4000,
    'Only unresolved previously discovered surfaces; disjoint from present-source ownership')
assert lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
for material in (surface,proxy):
    assert lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET) is None
    lib.recompile_material(material)
    assert assets.save_loaded_asset(material,only_if_is_dirty=False)
unreal.log('ACCUMULATED_MATERIALS_SAVED assets=2 WPO=none shadow=original_constant_one')
