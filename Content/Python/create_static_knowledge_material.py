"""Current P4 Live + sparse immutable spatial knowledge. No legacy Fog asset."""
import unreal
from create_darkwell_project_fog_materials import expr, connect, scalar_parameter, vector_parameter, custom_expression, hard_coverage, mask

path='/Game/Darkwell/Vision/ProjectFog/M_DarkwellStaticKnowledge'
tools=unreal.AssetToolsHelpers.get_asset_tools()
m=unreal.load_asset(path)
if m:
    unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
else:
    m=tools.create_asset('M_DarkwellStaticKnowledge','/Game/Darkwell/Vision/ProjectFog',unreal.Material,unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
world=expr(m,unreal.MaterialExpressionWorldPosition,-1400,0)
def tex(name,y):
    n=expr(m,unreal.MaterialExpressionTextureObjectParameter,-1400,y)
    n.set_editor_property('parameter_name',name)
    n.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture'))
    n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    return n
atlas=tex('StaticAtlas',150);pages=tex('StaticPages',300);live=tex('DarkwellLiveCoverageTexture',450)
bands=tex('StaticHeightBands',470)
layers=scalar_parameter(m,'StaticLayerCount',0,-1400,480)
ps=scalar_parameter(m,'StaticPageSide',16,-1400,600)
probes=scalar_parameter(m,'StaticLookupProbes',1,-1400,625)
ready=scalar_parameter(m,'StaticReady',0,-1400,650)
blocked=scalar_parameter(m,'StaticBlocked',0,-1400,700)
bb=vector_parameter(m,'StaticBlockBounds',unreal.LinearColor(0,0,0,0),-1400,750)
origin=vector_parameter(m,'FogWorldMin',unreal.LinearColor(0,0,0,0),-1400,800)
inv=vector_parameter(m,'FogWorldInvExtent',unreal.LinearColor(1,1,0,0),-1400,850)
node=custom_expression(m,r'''
if(Ready<0.5) return float3(0,0,0);
float2 P=World.xy;
int Layer=-1;
[loop] for(int I=0;I<int(LayerCount);I++) {
 float4 B=Bands.Load(int3(I,0,0));
 if((B.z>0.5 && World.z==B.x)||(B.z<0.5 && World.z>B.x && World.z<B.y)){Layer=int(B.w);break;}
}
if(Layer<0)return float3(0,0,0);
int2 K=int2(floor(P/80.0));
uint H=(uint(K.x)*73856093u ^ uint(K.y)*19349663u ^ uint(Layer)*83492791u)&4095u;
float Known=0;
[loop] for(uint I=0;I<uint(LookupProbes);I++) {
 uint Slot=(H+I)&4095u;
 float4 E=Pages.Load(int3(Slot%2048u,Slot/2048u,0));
 if(E.z==0) break;
 if(all(E.xy==float2(K)) && E.w==Layer) {
  uint Page=uint(E.z)-1u;
  int2 Local=int2(floor((P-float2(K)*80.0)/0.625));
  int2 Texel=int2(Page%uint(PageSide),Page/uint(PageSide))*128+Local;
  Known=Atlas.Load(int3(Texel,0)).r;break;
 }
}
// Match Region CPU half-open fine-sample CENTER membership exactly.
float2 Center=(floor(P/0.625)+0.5)*0.625;
if(Blocked>0.5 && all(Center>=BlockBounds.xy) && all(Center<BlockBounds.zw))Known=0;
float2 UV=(P-Origin.xy)*Inv.xy;
float Live=all(UV>=0)&&all(UV<=1)?saturate(Texture2DSampleLevel(LiveTex,LiveTexSampler,UV,0).r):0;
return float3(Live,Known,0);
''',[('World',world),('Atlas',atlas),('Pages',pages),('Bands',bands),('LayerCount',layers),('LiveTex',live),('PageSide',ps),('LookupProbes',probes),('Ready',ready),('Blocked',blocked),('BlockBounds',bb,'RGBA'),('Origin',origin),('Inv',inv)],-700,0,'Shared height-partitioned fine-support knowledge')
node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
hard=hard_coverage(m,mask(m,world,'rg',-700,100),mask(m,world,'b',-700,200),origin,inv,mask(m,node,'r',-700,300))
node=custom_expression(m,'return float3(Hard,State.g,0);',[('Hard',hard),('State',node)],-550,0,'Hard published current layer')
node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
tint=vector_parameter(m,'StaticTint',unreal.LinearColor(.4,.4,.4,1),-600,500)
base=custom_expression(m,'return Tint.rgb*State.r;',[('Tint',tint),('State',node)],-250,0,'Lit current')
normal=expr(m,unreal.MaterialExpressionVertexNormalWS,-600,650)
emissive=custom_expression(m,'float L=dot(Tint.rgb,float3(0.299,0.587,0.114)); return lerp(L.xxx,Tint.rgb,0.16)*1.2*(0.62+0.38*abs(N.z))*State.g*(1-State.r);',[('State',node),('Tint',tint),('N',normal)],-250,150,'Existing Lab neutral remembered relief; initially unknown')
for n,prop in [(base,unreal.MaterialProperty.MP_BASE_COLOR),(emissive,unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    unreal.MaterialEditingLibrary.connect_material_property(n,'',prop)
rough=scalar_parameter(m,'Roughness',.9,-250,300)
unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
spec=custom_expression(m,'return 0.1*State.r;',[('State',node)],-250,450,'No unexplored specular')
spec.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
unreal.MaterialEditingLibrary.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
unreal.MaterialEditingLibrary.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('STATIC_KNOWLEDGE_MATERIAL_CREATED')
