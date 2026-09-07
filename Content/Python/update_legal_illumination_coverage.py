import sys
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(unreal.Paths.project_content_dir()) / "Python"))
from create_darkwell_project_fog_materials import create_coverage
create_coverage(unreal.AssetToolsHelpers.get_asset_tools())
unreal.log("LEGAL_ILLUMINATION_COVERAGE_MATERIAL_UPDATED")
