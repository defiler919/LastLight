"""Rebuild only the current SightWeave material assets through Unreal APIs."""
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_darkwell_project_fog_materials as fog

tools = unreal.AssetToolsHelpers.get_asset_tools()
fog.create_coverage(tools)
fog.create_surface(tools)
fog.ASSET_PATH = "/Game/Darkwell/Vision/PropLab"
fog.SURFACE_NAME = "M_PropLabSurface"
fog.create_surface(tools, lab=True)
import create_static_knowledge_material
unreal.log("HARD_COVERAGE_MATERIALS_UPDATED")
