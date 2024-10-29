// Copyright 2024 Ossi Luoto

#include "TonemapOverride.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FTonemapOverrideModule"

DEFINE_LOG_CATEGORY(TonemapOverrideLog);

void FTonemapOverrideModule::StartupModule()
{
	// Set up the Shader Directories 
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("TonemapOverride"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugins/TonemapOverride"), PluginShaderDir);
	UE_LOG(TonemapOverrideLog, Log, TEXT("TonemapOverride module loaded and Shader directory %s registered"), *PluginShaderDir);
}

void FTonemapOverrideModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTonemapOverrideModule, TonemapOverride)