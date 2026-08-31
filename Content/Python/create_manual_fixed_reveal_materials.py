"""Stage 2 assets only. Run through Unreal Editor Python; never saves a map.

Original surface lighting is duplicated unchanged. The only new surface output
is OpacityMask, sampled in stable world XY. There is no WPO or geometry output.
ShadowPassSwitch returns 1 for the SAME mesh's entire shadow silhouette.
Do not run on top of existing assets: edits should be reviewed explicitly.
"""
import unreal
import create_darkwell_project_fog_materials as m

ROOT='/Game/Darkwell/Vision/PropLab/'
SURFACE=ROOT+'M_ManualFixedReveal'
RAMP=ROOT+'M_ManualFixedRevealRamp'
for path in (SURFACE,RAMP):
    if '-RebuildManualFixedReveal' in unreal.SystemLibrary.get_command_line() and unreal.EditorAssetLibrary.does_asset_exist(path):
        assert unreal.EditorAssetLibrary.delete_asset(path)
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), path+' already exists'

surface=unreal.EditorAssetLibrary.duplicate_asset(ROOT+'M_PropLabSurface',SURFACE)
assert surface
surface.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
surface.set_editor_property('opacity_mask_clip_value',0.333333)

def texture(material,name,uv,x,y):
    node=m.expr(material,unreal.MaterialExpressionTextureSampleParameter2D,x,y)
    node.set_editor_property('parameter_name',name)
    # Engine linear fallback, matching the existing lab materials. Ready=0
    # guarantees no main-pass pixels before runtime textures are bound.
    node.set_editor_property('texture',unreal.load_asset('/Engine/EngineMaterials/DefaultBloomKernel'))
    node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    m.connect(uv,'',node,'Coordinates')
    return node

world=m.expr(surface,unreal.MaterialExpressionWorldPosition,-2000,2800)
world.set_editor_property('world_position_shader_offset',unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
xy=m.mask(surface,world,'rg',-1800,2800)
minimum=m.mask(surface,m.vector_parameter(surface,'FogWorldMin',unreal.LinearColor(0,0,0,0),-2000,3000),'rg',-1800,3000)
inverse=m.mask(surface,m.vector_parameter(surface,'FogWorldInvExtent',unreal.LinearColor(0,0,0,0),-2000,3200),'rg',-1800,3200)
uv=m.binary(surface,unreal.MaterialExpressionMultiply,m.binary(surface,unreal.MaterialExpressionSubtract,xy,minimum,-1500,2800),inverse,-1300,2800)
raw=texture(surface,'DarkwellLiveCoverageTexture',uv,-1000,2800)
soft=texture(surface,'LabSoftCoverageTexture',uv,-1000,3050)
enabled=m.scalar_parameter(surface,'FixedRevealEnabled',0,-1000,3300)
ready=m.scalar_parameter(surface,'FixedRevealReady',0,-1000,3400)
alpha=m.custom_expression(surface,
    'return Enabled > 0.5 ? (Ready > 0.5 && Raw >= 0.99 ? saturate(Soft) : 0.0) : 1.0;',
    [('Raw',raw,'R'),('Soft',soft,'R'),('Enabled',enabled),('Ready',ready)],-700,2800,
    'Fixed geometry: legal world-space local reveal, fail closed before binding')
dither=m.expr(surface,unreal.MaterialExpressionMaterialFunctionCall,-400,2800)
dither.set_editor_property('material_function',unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
m.connect(alpha,'',dither,'Alpha Threshold')
shadow=m.expr(surface,unreal.MaterialExpressionShadowReplace,-100,2800)
m.connect(dither,'',shadow,'Default')
m.connect(m.scalar(surface,1,-400,3100),'',shadow,'Shadow')
assert unreal.MaterialEditingLibrary.connect_material_property(shadow,'',unreal.MaterialProperty.MP_OPACITY_MASK)
unreal.MaterialEditingLibrary.recompile_material(surface)

ramp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ManualFixedRevealRamp',ROOT.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
assert ramp
ramp.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
uv=m.expr(ramp,unreal.MaterialExpressionTextureCoordinate,-800,0)
raw=texture(ramp,'Raw',uv,-600,0)
previous=texture(ramp,'Previous',uv,-600,200)
step=m.scalar_parameter(ramp,'Step',0,-600,400)
result=m.custom_expression(ramp,'return Raw >= 0.99 ? min(1.0, Previous + max(0.0, Step)) : 0.0;',
    [('Raw',raw,'R'),('Previous',previous,'R'),('Step',step)],-250,0,
    'Only legal pixels start a 0.20 second local visual ramp; never authority')
assert unreal.MaterialEditingLibrary.connect_material_property(result,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(ramp)
for path in (SURFACE,RAMP):assert unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False)
unreal.log('FIXED_REVEAL_MATERIALS_CREATED surface=masked shadow=1 WPO=none assets=2')
