// Copyright 2024 Ossi Luoto


#include "TonemapOverrideEngineSubsystem.h"
#include "TonemapOverrideSceneViewExtension.h"
#include "TonemapOverride.h"

void UTonemapOverrideEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{

	TonemapOverrideSceneViewExtension = FSceneViewExtensions::NewExtension<FTonemapOverrideSceneViewExtension>();
	UE_LOG(TonemapOverrideLog, Log, TEXT("TonemapOverride SceneViewExtension created"));

}

void UTonemapOverrideEngineSubsystem::Deinitialize()
{
	{
		TonemapOverrideSceneViewExtension->IsActiveThisFrameFunctions.Empty();

		FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

		IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
		{
			return TOptional<bool>(false);
		};

		TonemapOverrideSceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
	}

	TonemapOverrideSceneViewExtension.Reset();
	TonemapOverrideSceneViewExtension = nullptr;
}
