"""Create the Lab-only opaque black cut-cap material through Unreal APIs."""
import unreal

ROOT='/Game/Darkwell/Vision/PropLab'
NAME='M_ManualStaleCutCap'
PATH=ROOT+'/'+NAME
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
mat=unreal.load_asset(PATH) if assets.does_asset_exist(PATH) else tools.create_asset(NAME,ROOT,unreal.Material,unreal.MaterialFactoryNew())
assert mat
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_SURFACE)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
lib=unreal.MaterialEditingLibrary
for node in list(lib.get_material_expressions(mat)):
    lib.delete_material_expression(mat,node)
black=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-200,0)
black.set_editor_property('constant',unreal.LinearColor(0,0,0,1))
assert lib.connect_material_property(black,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
assert assets.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('MANUAL_STALE_CUT_CAP_MATERIAL_SAVED opaque=1 unlit=1 twoSided=1')
