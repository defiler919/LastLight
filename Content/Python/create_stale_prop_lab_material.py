"""Editor API only. Adds one Lab-only asset; does not touch the source material or maps."""
import unreal
import create_darkwell_project_fog_materials as fog

ROOT='/Game/Darkwell/Vision/PropLab'
PATH=ROOT+'/M_PropLabStaleMemory'
mat=unreal.load_asset(PATH) if unreal.EditorAssetLibrary.does_asset_exist(PATH) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_PropLabStaleMemory',ROOT,unreal.Material,unreal.MaterialFactoryNew())
assert mat
unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
# Draw before DOF/TSR with ordinary depth testing; no stencil/composite/history.
mat.set_editor_property('translucency_pass',unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
wp=fog.expr(mat,unreal.MaterialExpressionWorldPosition,-1000,0)
bounds=fog.vector_parameter(mat,'StaleMinInv',unreal.LinearColor(0,0,1,1),-1000,200)
uv=fog.custom_expression(mat,'return (World.xy - MinInv.xy) * MinInv.zw;',
    [('World',wp),('MinInv',bounds,'RGBA')],-650,0,'World XY occupied-footprint cells; persistent empty evidence, no current-light texture')
uv.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2)
sample=fog.expr(mat,unreal.MaterialExpressionTextureSampleParameter2D,-350,0)
sample.set_editor_property('parameter_name','StaleOpacity')
sample.set_editor_property('texture',unreal.load_asset('/Engine/EngineMaterials/DefaultBloomKernel'))
sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
fog.connect(uv,'',sample,'Coordinates')
unreal.MaterialEditingLibrary.connect_material_property(sample,'R',unreal.MaterialProperty.MP_OPACITY)
# Same constant gray/normal relief as the Lab's remembered WhiteSquare surface.
tint=fog.vector_parameter(mat,'OriginalBaseColorTint',unreal.LinearColor(.24,.40,.56,1),-1000,500)
normal=fog.expr(mat,unreal.MaterialExpressionVertexNormalWS,-1000,700)
gray=fog.custom_expression(mat,'return lerp(dot(Tint,float3(.299,.587,.114)),Tint,.16)*1.2*(.62+.38*abs(N.z));',
    [('Tint',tint),('N',normal)],-650,500,'Static remembered relief; no light or scene input')
gray.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
stable=fog.expr(mat,unreal.MaterialExpressionEyeAdaptationInverse,-350,500)
fog.connect(gray,'',stable,'LightValue');fog.connect(fog.scalar(mat,1,-650,750),'',stable,'Alpha')
gi=fog.expr(mat,unreal.MaterialExpressionGIReplace,0,500)
fog.connect(stable,'',gi,'Default')
zero=fog.scalar(mat,0,-350,750)
fog.connect(zero,'',gi,'StaticIndirect');fog.connect(zero,'',gi,'DynamicIndirect')
unreal.MaterialEditingLibrary.connect_material_property(gi,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(PATH)
unreal.log('STALE_MATERIAL_READY '+PATH)
