import json
import unreal


MAP_PATH = "/Game/Maps/L_VisionIntegration"


def asset_path(obj):
    return obj.get_path_name() if obj else "None"


def audit():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if not world:
        raise RuntimeError(f"Could not load {MAP_PATH}")

    records = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        actor_record = {
            "actor": actor.get_name(),
            "class": actor.get_class().get_path_name(),
            "components": [],
        }
        for component in actor.get_components_by_class(unreal.MeshComponent):
            mesh = None
            nanite = False
            if isinstance(component, unreal.StaticMeshComponent):
                mesh = component.get_editor_property("static_mesh")
                if mesh:
                    try:
                        nanite = bool(mesh.get_editor_property("nanite_settings").enabled)
                    except Exception:
                        nanite = False
            materials = []
            for index in range(component.get_num_materials()):
                material = component.get_material(index)
                textures = []
                if material:
                    try:
                        textures = sorted(
                            asset_path(texture)
                            for texture in unreal.MaterialEditingLibrary.get_used_textures(material)
                            if texture
                        )
                    except Exception:
                        textures = []
                materials.append(
                    {
                        "slot": index,
                        "material": asset_path(material),
                        "textures": textures,
                    }
                )
            actor_record["components"].append(
                {
                    "component": component.get_name(),
                    "class": component.get_class().get_path_name(),
                    "mesh": asset_path(mesh),
                    "nanite": nanite,
                    "materials": materials,
                    "render_custom_depth": bool(
                        component.get_editor_property("render_custom_depth")
                    ),
                    "custom_depth_stencil_value": int(
                        component.get_editor_property("custom_depth_stencil_value")
                    ),
                }
            )
        if actor_record["components"]:
            records.append(actor_record)

    unreal.log("DARKWELL_SURFACE_ASSET_AUDIT=" + json.dumps(records, indent=2))


if __name__ == "__main__":
    audit()
