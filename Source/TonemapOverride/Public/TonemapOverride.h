// Copyright 2024 Ossi Luoto

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

TONEMAPOVERRIDE_API DECLARE_LOG_CATEGORY_EXTERN(TonemapOverrideLog, Log, All);

class FTonemapOverrideModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
