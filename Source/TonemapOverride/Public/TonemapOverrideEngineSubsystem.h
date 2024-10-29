// Copyright 2024 Ossi Luoto

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "TonemapOverrideEngineSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class TONEMAPOVERRIDE_API UTonemapOverrideEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<class FTonemapOverrideSceneViewExtension, ESPMode::ThreadSafe> TonemapOverrideSceneViewExtension;
	
};
