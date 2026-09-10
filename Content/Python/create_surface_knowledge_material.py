"""Build the Box Surface Knowledge presenter using official Editor asset APIs.

This shader only addresses CPU-published face cells. It contains no observer,
light, occlusion, Whole recognition or knowledge-write policy.
"""
import unreal
from create_darkwell_project_fog_materials import expr, connect, vector_parameter, scalar_parameter, custom_expression

path = "/Game/Darkwell/Vision/SurfaceKnowledge"
name = "M_SurfaceKnowledgeV1"
material = unreal.load_asset(path + "/" + name)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
world = expr(material, unreal.MaterialExpressionWorldPosition, -1000, 0)
center = expr(material, unreal.MaterialExpressionObjectPositionWS, -1000, -150)
relative = expr(material, unreal.MaterialExpressionSubtract, -850, 0)
connect(world, "", relative, "A")
connect(center, "", relative, "B")
world_float = expr(material, unreal.MaterialExpressionTruncateLWC, -800, 0)
connect(relative, "", world_float, "")
inputs = [("World", world_float)]
for i, (key, value) in enumerate([
    ("SurfaceOrigin", (0, 0, 0, 0)), ("SurfaceExtent", (50, 50, 50, 0)),
    ("SurfaceAxis0", (1, 0, 0, 0)), ("SurfaceAxis1", (0, 1, 0, 0)),
    ("SurfaceAxis2", (0, 0, 1, 0)), ("SurfaceTint", (.3, .2, .1, 0))
] + [("SurfaceFace" + str(f), (1, 1, f, 0)) for f in range(6)]):
    source = vector_parameter(material, key, unreal.LinearColor(*value), -1000, 150 + i*100)
    inputs.append((key, source, "RGBA") if key.startswith("SurfaceFace") else (key, source))
inputs.append(("SurfaceReady", scalar_parameter(material, "SurfaceReady", 0, -1000, 1450)))
texture = expr(material, unreal.MaterialExpressionTextureObjectParameter, -1000, 1600)
texture.set_editor_property("parameter_name", "SurfaceAtlas")
texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
texture.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
inputs.append(("Atlas", texture))
code = """
// Subtract the validated Box primitive center in LWC before float conversion.
float3 d = World;
float3 p = float3(dot(d,SurfaceAxis0),dot(d,SurfaceAxis1),dot(d,SurfaceAxis2)) / max(SurfaceExtent, .001);
float3 a = abs(p);
int axis = a.x >= a.y && a.x >= a.z ? 0 : a.y >= a.z ? 1 : 2;
int face = axis*2 + (p[axis] > 0 ? 1 : 0);
float4 meta = face==0 ? SurfaceFace0 : face==1 ? SurfaceFace1 : face==2 ? SurfaceFace2 : face==3 ? SurfaceFace3 : face==4 ? SurfaceFace4 : SurfaceFace5;
float2 uv = saturate(float2(p[(axis+1)%3],p[(axis+2)%3])*.5+.5);
int2 cell = min(int2(uv*meta.xy),int2(meta.xy)-1);
if(meta.w>.5)cell=cell.yx;
float3 facts = Atlas.Load(int3(cell.x,cell.y+(int)meta.z,0)).rgb;
return SurfaceReady * facts;
"""
node = custom_expression(material, code, inputs, -200, 0, "CPU Surface Live / Known only")
node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
base = custom_expression(material, "return Tint * State.r;", [("Tint", inputs[6][1]), ("State", node)], 0, 0, "Lit current surface")
base.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
gray = custom_expression(material, "return float3(.15,.15,.15)*State.g*(1-State.r)*(1-State.b);", [("State", node)], 0, 150, "Known surface memory")
gray.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
unreal.MaterialEditingLibrary.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
unreal.MaterialEditingLibrary.connect_material_property(gray, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
rough = scalar_parameter(material, "Roughness", .9, 0, 300)
unreal.MaterialEditingLibrary.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
spec = custom_expression(material, "return .1*State.r;", [("State", node)], 0, 450, "Unknown has no specular")
spec.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
unreal.MaterialEditingLibrary.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log("Surface Knowledge material saved")
