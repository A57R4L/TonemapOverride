// Copyright 2024 Ossi Luoto

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "TonemapOverrideSettings.generated.h"

UENUM(BlueprintType)
enum class ECustomTonemapOperator : uint8
{
	Agx,
	AgxPunchy,
	Reinhard,
	TonyMcMapface,
	Flim,
	ACES,
	MAX,
};


UCLASS(Config=Game, DefaultConfig, DisplayName = "TonemapOverride")
class TONEMAPOVERRIDE_API UTonemapOverrideSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "TonemapOverride", meta = (DisplayName = "Use custom tonemapper", ToolTip = "Use SceneView Extension for a custom tonemapping function and disable engine tonemapper"))
	bool bUseCustomTonemapper = true;

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride", meta = (DisplayName = "TonemappingOperator", ToolTip = "Custom tonemapping operator to use (Shader permutation)"))
	ECustomTonemapOperator CustomTonemapOperator;
	
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride", meta = (DisplayName = "LUT Texture", ToolTip = "Texture asset for tonemapper"))
	TSoftObjectPtr<UTexture> LUTTexture = FSoftObjectPath("/TonemapOverride/Textures/tony_mc_mapface_f32.tony_mc_mapface_f32").ResolveObject();
	
	virtual FName GetContainerName() const override { return FName("Project"); };
	virtual FName GetCategoryName() const override { return FName("Plugins"); };
	virtual FName GetSectionName() const override { return FName("TonemapOverride"); };

	static UTonemapOverrideSettings& Get();
	
};