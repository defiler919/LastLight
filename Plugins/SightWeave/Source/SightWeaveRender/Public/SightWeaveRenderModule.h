#pragma once

#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSightWeaveRender, Log, All);

class SIGHTWEAVERENDER_API FSightWeaveRenderModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsShaderDirectoryMapped() const { return bShaderDirectoryMapped; }
	const FString& GetShaderDirectory() const { return ShaderDirectory; }

private:
	FString ShaderDirectory;
	bool bShaderDirectoryMapped = false;
};
