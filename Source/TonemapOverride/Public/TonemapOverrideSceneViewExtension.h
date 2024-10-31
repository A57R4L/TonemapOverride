// Copyright 2024 Ossi Luoto

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessTonemap.h"
#include "TonemapOverrideSettings.h"

struct FCachedLUTSettings;

class TONEMAPOVERRIDE_API FTonemapOverrideSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FTonemapOverrideSceneViewExtension(const FAutoRegister& AutoRegister);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};

	// With small modifications to Engine SceneViewExtension.h and PostProcessCombineLUT.cpp we can more gracefully implement a custom tonemapper
#if ENGINE_VERSION_CUSTOM == true
	virtual void SubscribeToPostProcessCombineLUTPass(const FSceneView& InView, FTonemapLUTCallbackDelegateArray& LUTPassCallbacks) override;
#else

	// Alternatively with current 5.4/5.5 we can create a pre-tonemap hook to hijack the LUT creation process
#if ENGINE_MINOR_VERSION == 5 
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
#else
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
#endif
#endif

private:
	// Internal render call
	FRDGTextureRef RenderOverrideLUT(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef OutputTexture, FCachedLUTSettings& CachedLUTSettings, const bool bUseComputePass, const bool bUseVolumeTextureLUT, const int32 TextureLUTSize);

#if ENGINE_VERSION_CUSTOM == true
	// For the engine modification, callback from PostProcessCombineLUT
	FRDGTextureRef CreateOverrideLUT_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef OutputTexture);
#else
	// MotionBlur SVE callback for hacking the Tonemap pass without modified engine
	FScreenPassTexture CreateOverrideLUT(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);
#endif

	bool bProcessed = false;
	bool bCachedOverride = false;
};



