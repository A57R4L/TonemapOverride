# TonemapOverride

Unreal Engine 5 plugin for replacing ACES with a custom tonemapper while preserving the color grading (ie. Post Process volumes) and tonemap shader functionality (Bloom composition, film grain, CA, Vignette etc). 

### Features

* Tonemap operators: AgX, Reinhard, Tony McMapface, Flim, ACES (native Unreal Engine tonemapper)
* Two implementations: 
	1) Hacky way to bypass native process without engine source modification 
	2) More graceful SceneViewExtension delegate, with minimal engine source modifications

### Installation and use

- Install to plugins folder and enable from project settings
- Use Project settings to change the used tonemap operator and assign LUT texture from Plugin Content folder for Tony McMapface if missing
- Optional: Modify the engine source and uncomment the declaration from build file

- When applying Color Grading, I suggest to ramp up the LUT texture dimensions with r.LUT.Size from the default 32 to 64 for example. Depending on the used tonemapper implementation, artifacts starts to appear quite soon when using low resolution LUT sizes. This helps also with the native engine tonemapper implementation.

### Motivation

When working with colors in the high dynamic range, tonemapping function can make a big difference on how the colors behave. By default, Unreal Engine provides ACES tonemapper to handle the conversion from high dynamic range working colors to display colors. Bypassing/replacing ACES tonemapper is either not trivial, or comes with limitations. Currently the options are:

1. Using PostProcess material with replacing tonemapper material slot
2. Disabling Engine Tonemapping function and using fully custom implementation with f.ex. SceneViewExtension in Tonemap Pass or OCIO transform
3. Modifying the engine code

<img src="tonemap_diagram.jpg" width="640" height="auto" />

As seen from the process breakdown, bypassing current engine implementation requires to rewrite a lot from the current engine implementation and/or leave out the tonemap shader features. These include the composition of Bloom effect, using EyeExposure Adpatation and the Film grain and vignette effects of the tonemapper to name a few. Post Process Volumes are a typical way to apply adjustments for color grading which are implemented in the CombineLUT pass. While it is possible to cover some features, one loses the computational efficieny of using the LUT based approach the engine uses internally if not relying to the method to modify the engince code.

While this plugin provides a way to hack the current post processing pipeline, it is also made to showcase how the SceneViewExtension could be extended to easily hook into the dedicated pass.

### How the pluging works

Unreal Engine tonemapper works in two passes. First creating a lookup texture with color grading parameters and then on the second pass implementing that LUT to the current scene texture while doing compoisition of other textures (like bloom) and effects (vignette etc). The Tonemap pass optimizes this process by using the LUT from previous iteration if no changes are done. The hack version of the plugin utilizes this optimization by trying to bypass the in-engine LUT creation and overwriting the existing texture.

The provided alternative version (with the slight modifications to SceneViewExtension and PostProcessing LUT pass) is a more graceful way to implement this but requires modifying the engine.

### Modifying the Scene View Extension

[SceneViewExtensions](https://github.com/A57R4L/SceneViewExtensionTemplate) are a powerful tool to inject your own functions and passes to the rendering pipeline. With slight adjustments to the SceneViewExtension and Postprocessing calls, the users could be provided powerful ways to alter the post processing while utilizing the native engine rendering features.

SceneViewExtension.h
```c++

/** This is used to override ColorGradingTexture in PostProcessCombineLUT */
FUNC_DECLARE_DELEGATE(FTonemapLUTCallbackDelegate, FRDGTextureRef /*ReturnColorGradingTexture*/, FRDGBuilder& /*GraphBuilder*/, const FSceneView& /*View*/, FRDGTextureRef /*InColorGradingTexture*/)
using FTonemapLUTCallbackDelegateArray = TArray<FTonemapLUTCallbackDelegate, SceneRenderingAllocator>;

---

/**
 * This is called in the beginning of the PostProcessCombineLUT for the chance to override the ColorGrading LUT calculation
 */
virtual void SubscribeToPostProcessCombineLUTPass(const FSceneView& InView, FTonemapLUTCallbackDelegateArray& InOutPassCallbacks) {}

```

PostProcessCombineLUTs.cpp
```c++

if (ViewFamily.EngineShowFlags.ColorGrading)
{
	LocalCount = GenerateFinalTable(View.FinalPostProcessSettings, LocalTextures, LocalWeights, GMaxLUTBlendCount);
}

// Subscribe SceneViewExtension callbacks for custom ColorGrading LUT
FTonemapLUTCallbackDelegateArray TonemapLUTDelegates;
for (int32 ViewExt = 0; ViewExt < View.Family->ViewExtensions.Num(); ++ViewExt)
{
	View.Family->ViewExtensions[ViewExt]->SubscribeToPostProcessCombineLUTPass(View, TonemapLUTDelegates);
}

const bool bUseCustomLUT = !TonemapLUTDelegates.IsEmpty();

const bool bUseComputePass = View.bUseComputePasses;

---

const bool bHasChanged = CachedLUTSettings.UpdateCachedValues(View, LocalTextures, LocalWeights, LocalCount);
if (!bHasChanged && !bUseCustomLUT && OutputTexture && CVarLUTUpdateEveryFrame.GetValueOnRenderThread() == 0)
{
	return OutputTexture;
}

---

check(OutputTexture);

// If SceneViewExtension callbacks for Tonemap exists, use those to create the ColorGrading LUT
if (bUseCustomLUT)
{
	for (int32 PassCallbackIndex = 0; PassCallbackIndex < TonemapLUTDelegates.Num(); PassCallbackIndex++)
	{
		FTonemapLUTCallbackDelegate& PassCallback = TonemapLUTDelegates[PassCallbackIndex];
		OutputTexture = PassCallback.Execute(GraphBuilder, View, OutputTexture);
	}

	return OutputTexture;
}

// For a 3D texture, the viewport defaults to 32x32 (per slice); for a 2D texture, it's unwrapped to 1024x32.
const FIntPoint OutputViewSize(bUseVolumeTextureLUT ? LUTSize : LUTSize * LUTSize, LUTSize);

```

### Limitations

Without the engine source modifications, hacking into the tonemapping pass will occasionally fail (f.ex. while previewing another view). This results in flickers when scene view falls back to use the native engine tonemapper. The hack version also fails, if the project doesn't allow caching the LUT texture. 

Implementation to use the color grading LUT texture assets (not to be confused with the LUT talked otherwise here) has been intentionally left out as one shouldn't really use those in the HDR color grading in the first place. That said, adding that implementation shouldn't be too hard. On the sidenote, if one needs a solution to build a look library for HDR Color Grading, I suggest to look how that is implemented in my [Color Editor](https://fab.com/s/a35660119cc1) plugin. Otherwise all applicable parameters from the post process parameters are implemented.


### Comparison

Comparison of included tonemappers in an Unreal Engine scene. No other modifications have been made except changing the tonemapper and LUT resolution. See Images folder for the images in higher resolution.


### License 

My own code is licensed under the terms of the MIT license. Tonemapping operators here are for the sake of demonstration, check copyrights and licenses from their original authors. I've tried to include a comperehensive list of references in the sourcefiles and links below. Apologies for mistakes in the adaptations, feel free to suggest corrections.

For the sake of clarity, this repository extends Unreal Engine classes. For more information and license on using Unreal Engine please refer to: [Unreal Engine download page](https://www.unrealengine.com/download)

### Updates

October 2024: First release

### Links and references

[AgX](https://github.com/sobotka/AgX) by Troy Sobotka 
[Reinhard](https://www-old.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf) by Reinhard et al
[Tony McMapface](https://github.com/h3r2tic/tony-mc-mapface) by Tomasz Stachowiak
[Flim](https://github.com/bean-mhm/flim) by Bean
[ACES](https://github.com/ampas/aces-core) Academy of Motion Picture Arts and Sciences

[AgX Implementation](https://iolite-engine.com/blog_posts/minimal_agx_implementation)
[Comparison scene: Butcher's Alley Palmetro Future City](https://fab.com/s/d72725be0932) by Dekogon Studios