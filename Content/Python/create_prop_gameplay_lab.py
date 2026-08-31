"""Run only through Unreal Editor Python. Writes only the independent lab map/assets."""
import unreal
import create_darkwell_project_fog_materials as fog

MAP = "/Game/Maps/L_ProjectFogPropGameplayLab"
ROOT = "/Game/Darkwell/Vision/PropLab"

def materials():
    fog.ASSET_PATH = ROOT
    fog.SURFACE_NAME = "M_PropLabSurface"
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.EditorAssetLibrary.make_directory(ROOT)
    fog.create_surface(tools, lab=True)
    mat, path = fog.make_asset(tools, "M_PropLabSoft")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    samples = []
    for i, name in enumerate(("Raw", "Previous")):
        node = fog.expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, i*250)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultBloomKernel"))
        node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        samples.append(fog.mask(mat, node, "r", -350, i*250))
    step = fog.scalar_parameter(mat, "Step", 0.083333, -350, 500)
    output = fog.custom_expression(mat,
        "return Raw > 0.001 ? min(Raw, Previous + Step) : 0.0;",
        [("Raw", samples[0]), ("Previous", samples[1]), ("Step", step)], 0, 0,
        "0.20 second finite ramp; immediate zero outside current legal coverage; no scene history")
    unreal.MaterialEditingLibrary.connect_material_property(output, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)

def main():
    materials()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        raise RuntimeError("Lab map already exists; edit through the editor, do not silently replace it")
    levels.new_level(MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.DarkwellVisionIntegrationGameMode)
    fixture = actors.spawn_actor_from_class(unreal.DarkwellPropGameplayLab, unreal.Vector())
    fixture.set_actor_label("PROP LAB - native fixture and controls")
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-520,-190,100), unreal.Rotator(0,90,0))
    start.set_actor_label("Start - kitchen sweep")
    def prop(id, shape, pos, size, tint=(0.25,0.42,0.55), yaw=0):
        a = actors.spawn_actor_from_class(unreal.DarkwellPropLabFurniture, unreal.Vector(*pos), unreal.Rotator(pitch=0,yaw=yaw,roll=0))
        a.set_editor_property("stable_id", id)
        a.set_editor_property("shape", shape)
        a.set_editor_property("dimensions", unreal.Vector(*size))
        a.set_editor_property("tint", unreal.LinearColor(*tint,1))
        a.set_actor_label(id)
        # PostEditChange on each property reruns native construction.
        return a
    for i in range(8):
        prop(f"Lab.Kitchen.Row8.{i+1:02}",0,(-770+i*60,350,0),(60,60,88),(.18+.03*i,.36,.46))
    for i in range(4):
        prop(f"Lab.Kitchen.Row4.{i+1:02}",0,(-850,320-i*60,0),(60,60,88),(.43,.30,.18),90)
    prop("Lab.Counter.Long",4,(-560,350,91),(484,66,5),(.65,.62,.49))
    prop("Lab.Counter.Return",4,(-850,230,91),(66,244,5),(.65,.62,.49))
    prop("Lab.Fridge",1,(-200,350,0),(82,76,190),(.62,.66,.69))
    prop("Lab.TallCabinet",0,(-65,350,0),(70,65,210),(.29,.39,.24))
    prop("Lab.Island",0,(-460,35,0),(210,85,88),(.20,.34,.40))
    prop("Lab.Shelf",2,(500,540,0),(150,55,190),(.42,.43,.45))
    prop("Lab.DestroyBox",3,(410,240,0),(45,50,42),(.59,.34,.15))
    prop("Lab.Box.Large",3,(600,200,0),(85,70,80),(.47,.27,.12))
    prop("Lab.Box.Small",3,(540,160,0),(28,32,25),(.66,.43,.21))
    prop("Lab.MobileCabinet",0,(850,540,0),(90,65,130),(.27,.46,.32))
    prop("Lab.TwinA",0,(180,430,0),(65,62,110),(.43,.36,.23))
    prop("Lab.TwinB",0,(320,430,0),(65,62,110),(.43,.36,.23))
    prop("Lab.ReplaceOld",0,(860,190,0),(70,60,95),(.34,.28,.47))
    # Navigation bounds are authored with the editor brush builder, leaving core AI unchanged.
    nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0,0,80))
    nav.set_actor_scale3d(unreal.Vector(12,9,3))
    navdata = actors.spawn_actor_from_class(unreal.RecastNavMesh, unreal.Vector())
    navdata.set_editor_property('runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
    levels.save_current_level()
    unreal.log("PROP_LAB_ASSETS_READY furniture=25 map=" + MAP)
    # Keep fresh creation consistent with the comparison migration.
    import update_prop_lab_comparison

if __name__ == "__main__":
    main()
