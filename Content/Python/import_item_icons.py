"""Import DARKWELL item source art and bind it to the native item definitions."""

from pathlib import Path

import unreal


DESTINATION_PATH = "/Game/UI/Items"
ITEMS = (
    {
        "source": "T_Item_ShotgunShells.png",
        "texture": "T_Item_ShotgunShells",
        "definition": "/Game/Data/Items/DA_Item_ShotgunShells",
    },
    {
        "source": "T_Item_Scrap.png",
        "texture": "T_Item_Scrap",
        "definition": "/Game/Data/Items/DA_Item_Scrap",
    },
)


def import_texture(source_path: Path, asset_name: str) -> unreal.Texture2D:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(f"{DESTINATION_PATH}/{asset_name}")
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Failed to import Texture2D: {source_path}")

    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    return texture


def main() -> None:
    project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    source_dir = project_dir / "SourceArt" / "UI" / "Items"
    unreal.EditorAssetLibrary.make_directory(DESTINATION_PATH)

    assets_to_save: list[str] = []
    for item in ITEMS:
        source_path = source_dir / item["source"]
        if not source_path.is_file():
            raise RuntimeError(f"Missing source icon: {source_path}")

        texture = import_texture(source_path, item["texture"])
        definition = unreal.EditorAssetLibrary.load_asset(item["definition"])
        if definition is None:
            raise RuntimeError(f"Missing item definition: {item['definition']}")

        definition.set_editor_property("icon", texture)
        # Keep this empty until descriptions are authored as namespace/key-backed
        # asset text. The native catalog supplies the localizable runtime fallback.
        definition.set_editor_property("description", unreal.Text(""))
        assets_to_save.extend((texture.get_path_name(), definition.get_path_name()))

    for asset_path in assets_to_save:
        if not unreal.EditorAssetLibrary.save_loaded_asset(unreal.EditorAssetLibrary.load_asset(asset_path), False):
            raise RuntimeError(f"Failed to save asset: {asset_path}")

    unreal.log(f"DARKWELL item presentation imported and saved: {len(ITEMS)} definitions")


if __name__ == "__main__":
    main()
