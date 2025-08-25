// Copyright 2024 - 2025 Ossi Luoto

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
	Hejl,
	GranTurismo,
	GT7,
	ACES,
	MAX,
};

UENUM(BlueprintType)
enum class EGT7UCSType : uint8
{
	ICtCp,
	Jzazbz,
	MAX
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
	
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | Tony", meta = (DisplayName = "LUT Texture", ToolTip = "Texture asset for tonemapper"))
	TSoftObjectPtr<UTexture> LUTTexture = TSoftObjectPtr<UTexture>(FSoftObjectPath(TEXT("/TonemapOverride/Content/Textures/tony_mc_mapface_f32.tony_mc_mapface_f32")).ResolveObject());

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | Reinhard", meta = (DisplayName = "WhitePoint", ToolTip = "Reinhard Whitepoint"))
	float ReinhardWhitePoint = 20.0;

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | Hejl", meta = (DisplayName = "WhitePoint", ToolTip = "Hejl Whitepoint"))
	float HejlWhitePoint = 20.0;

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | GT7", meta = (DisplayName = "GT7 UCS", ToolTip = "GT7 Unified color space"))
	EGT7UCSType UCSType = EGT7UCSType::ICtCp;

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | GT7", meta = (DisplayName = "GT7 Blend Ratio", ToolTip = "GT7 Blend Ratio between Skewed and Scaled Colors"))
	float GT7BlendRatio = 0.6f;

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | GT7", meta = (DisplayName = "GT7 Fade Start", ToolTip = "GT7 Fade Start"))
	float GT7FadeStart = 0.98f;

	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category = "TonemapOverride | GT7", meta = (DisplayName = "GT7 Fade End", ToolTip = "GT7 Fade End"))
	float GT7FadeEnd = 1.16f;
	
	virtual FName GetContainerName() const override { return FName("Project"); };
	virtual FName GetCategoryName() const override { return FName("Plugins"); };
	virtual FName GetSectionName() const override { return FName("TonemapOverride"); };

	static UTonemapOverrideSettings& Get();

};