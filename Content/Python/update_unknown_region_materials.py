"""Rebuild only the two ground/environment parents with the CPU knowledge gate."""
import unreal
import create_darkwell_project_fog_materials as fog
tools=unreal.AssetToolsHelpers.get_asset_tools()
fog.create_surface(tools)
fog.ASSET_PATH="/Game/Darkwell/Vision/PropLab"
fog.SURFACE_NAME="M_PropLabSurface"
fog.create_surface(tools,lab=True)
