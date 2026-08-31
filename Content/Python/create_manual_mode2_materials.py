"""Editor APIs only: four new manual-Mode2 assets. No existing assets/maps rewritten."""
import unreal
import create_darkwell_project_fog_materials as fog

ROOT='/Game/Darkwell/Vision/PropLab'

def make(name, lit=False, translucent=False):
    path=ROOT+'/'+name
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT if lit else unreal.MaterialShadingModel.MSM_UNLIT)
    if translucent:
        mat.set_editor_property('translucency_pass',unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
        mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        mat.set_editor_property('output_translucent_velocity',True)
    return mat,path

for name in ('M_ManualMode2Memory','M_ManualMode2Cut','M_ManualMode2Floor','M_ManualMode2Live'):
    live=name.endswith('Live'); floor=name.endswith('Floor'); cut=name.endswith('Cut')
    mat,path=make(name,floor,live)
    if cut:
        color=fog.scalar(mat,0,-300,0)
    elif floor:
        color=fog.vector_parameter(mat,'Tint',unreal.LinearColor(*((.14,.48,.25,1) if live else (.28,.32,.35,1))),-800,0)
        unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(fog.scalar(mat,.85,-500,200),'',unreal.MaterialProperty.MP_ROUGHNESS)
        color=fog.scalar(mat,0,-300,0)
    else:
        n=fog.expr(mat,unreal.MaterialExpressionVertexNormalWS,-900,200)
        tint=fog.vector_parameter(mat,'Tint',unreal.LinearColor(.14,.48,.25,1),-900,0)
        code='return Tint*(.62+.38*abs(N.z));' if live else 'return lerp(dot(Tint,float3(.299,.587,.114)),Tint,.16)*1.2*(.62+.38*abs(N.z));'
        gray=fog.custom_expression(mat,code,[('Tint',tint),('N',n)],-600,0,'Existing Lab unlit relief; actual geometry alone casts real shadows')
        gray.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        color=fog.expr(mat,unreal.MaterialExpressionEyeAdaptationInverse,-300,0)
        fog.connect(gray,'',color,'LightValue');fog.connect(fog.scalar(mat,1,-500,300),'',color,'Alpha')
    unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if live:
        wp=fog.expr(mat,unreal.MaterialExpressionWorldPosition,-1000,600)
        bounds=fog.vector_parameter(mat,'RevealMinInv',unreal.LinearColor(0,0,1,1),-1000,800)
        uv=fog.custom_expression(mat,'return (World.xy-MinInv.xy)*MinInv.zw;',[('World',wp),('MinInv',bounds,'RGBA')],-700,600,'Per-position .20s ramp; geometry is clipped independently by legal coverage')
        uv.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        tex=fog.expr(mat,unreal.MaterialExpressionTextureSampleParameter2D,-400,600)
        tex.set_editor_property('parameter_name','Reveal');tex.set_editor_property('texture',unreal.load_asset('/Engine/EngineMaterials/DefaultBloomKernel'))
        tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        fog.connect(uv,'',tex,'Coordinates')
        unreal.MaterialEditingLibrary.connect_material_property(tex,'R',unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('MODE2_MATERIAL_READY '+path)
