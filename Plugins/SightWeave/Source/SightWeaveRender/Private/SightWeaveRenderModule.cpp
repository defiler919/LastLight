#include "SightWeaveRenderModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogSightWeaveRender);

namespace
{
	const FString VirtualShaderDirectory(TEXT("/Plugin/SightWeave"));
}

void FSightWeaveRenderModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogSightWeaveRender, Error,
			TEXT("SightWeave plugin descriptor was unavailable; GPU presentation remains fail-closed."));
		return;
	}

	ShaderDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
	if (!FPaths::DirectoryExists(ShaderDirectory))
	{
		UE_LOG(LogSightWeaveRender, Error,
			TEXT("SightWeave shader directory does not exist at '%s'; GPU presentation remains fail-closed."),
			*ShaderDirectory);
		return;
	}

	const FString* ExistingMapping = AllShaderSourceDirectoryMappings().Find(VirtualShaderDirectory);
	if (ExistingMapping)
	{
		bShaderDirectoryMapped = FPaths::IsSamePath(*ExistingMapping, ShaderDirectory);
		if (!bShaderDirectoryMapped)
		{
			UE_LOG(LogSightWeaveRender, Error,
				TEXT("%s is already mapped to '%s', not '%s'; GPU presentation remains fail-closed."),
				*VirtualShaderDirectory,
				**ExistingMapping,
				*ShaderDirectory);
		}
		return;
	}

	AddShaderSourceDirectoryMapping(VirtualShaderDirectory, ShaderDirectory);
	bShaderDirectoryMapped = true;
}

void FSightWeaveRenderModule::ShutdownModule()
{
	// UE 5.8 owns shader directory mappings for process lifetime and exposes no
	// per-mapping removal API. This module deliberately owns no static world or RHI state.
	bShaderDirectoryMapped = false;
	ShaderDirectory.Reset();
}

IMPLEMENT_MODULE(FSightWeaveRenderModule, SightWeaveRender)
