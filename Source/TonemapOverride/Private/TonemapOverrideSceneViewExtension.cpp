// Copyright 2024 - 2025 Ossi Luoto

#include "TonemapOverrideSceneViewExtension.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "NUTUtil.h"
#include "TonemapOverride.h"
#include "HDRHelper.h"
#include "PixelShaderUtils.h"
#include "PostProcess/DrawRectangle.h"


IMPLEMENT_GET_PRIVATE_VAR(FSceneView, EyeAdaptationViewState, FSceneViewStateInterface*);
IMPLEMENT_GET_PRIVATE_VAR(FSceneViewState, CombinedLUTRenderTarget, TRefCountPtr<IPooledRenderTarget>);
// With current setup we visit the TonemappingLUT
IMPLEMENT_GET_PRIVATE_VAR(FSceneViewState, bValidTonemappingLUT, bool);

class FTonemapOverrideShaderCommon : public FGlobalShader
{
public:
	static const int32 GroupSize = 8;
	
	static bool PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(EShaderPlatform Platform)
	{
		return RHIVolumeTextureRenderingSupportGuaranteed(Platform) && (RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform));
	}
	
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GroupSize);

		const int UseVolumeLUT = PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(Parameters.Platform) ? 1 : 0;
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeLUT);
	}

	class FTonemapOperator : SHADER_PERMUTATION_ENUM_CLASS("TONEMAP_OPERATOR", ECustomTonemapOperator);
	class FOutputDeviceSRGB : SHADER_PERMUTATION_BOOL("OUTPUT_DEVICE_SRGB");
	class FSkipTemperature : SHADER_PERMUTATION_BOOL("SKIP_TEMPERATURE");
	class FGT7UCSType : SHADER_PERMUTATION_ENUM_CLASS("TONE_MAPPING_UCSTYPE", EGT7UCSType);
	using FPermutationDomain = TShaderPermutationDomain<FOutputDeviceSRGB, FTonemapOperator, FSkipTemperature, FGT7UCSType>;
	
	FTonemapOverrideShaderCommon() {}
	
	FTonemapOverrideShaderCommon(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{ }
};

// Custom parameters implemented outside native Engine tonemapping/color grading
BEGIN_SHADER_PARAMETER_STRUCT(FCustomTonemapperParameters, )
	SHADER_PARAMETER(int32, TonemapOperator)
	SHADER_PARAMETER(float, ReinhardWhitePoint)
	SHADER_PARAMETER_TEXTURE(Texture3D<float>, LUTTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, LUTTextureSampler)
	SHADER_PARAMETER(float, HejlWhitePoint)
	SHADER_PARAMETER(float, GT7BlendRatio)
	SHADER_PARAMETER(float, GT7FadeStart)
	SHADER_PARAMETER(float, GT7FadeEnd)
	SHADER_PARAMETER(int32, EGT7UCSType)
END_SHADER_PARAMETER_STRUCT()

// Need to bind all parameters for Full ACES Tonemapping & Color Grading for full implementation
// When doing just custom, can limit these to the required 
BEGIN_SHADER_PARAMETER_STRUCT(FACESTonemapShaderParameters, )
	SHADER_PARAMETER(FVector4f, ACESMinMaxData)
	SHADER_PARAMETER(FVector4f, ACESMidData)
	SHADER_PARAMETER(FVector4f, ACESCoefsLow_0)
	SHADER_PARAMETER(FVector4f, ACESCoefsHigh_0)
	SHADER_PARAMETER(float, ACESCoefsLow_4)
	SHADER_PARAMETER(float, ACESCoefsHigh_4)
	SHADER_PARAMETER(float, ACESSceneColorMultiplier)
	SHADER_PARAMETER(float, ACESGamutCompression)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTonemapOverrideLUTParameters, )
	// Tonemap parameters
	SHADER_PARAMETER_STRUCT_REF(FWorkingColorSpaceShaderParameters, WorkingColorSpace)
	SHADER_PARAMETER_STRUCT_INCLUDE(FACESTonemapShaderParameters, ACESTonemapParameters)
	SHADER_PARAMETER(float, LUTSize)
	SHADER_PARAMETER(FVector4f, OverlayColor)
	SHADER_PARAMETER(FVector3f, ColorScale)
	SHADER_PARAMETER(FVector4f, ColorSaturation)
	SHADER_PARAMETER(FVector4f, ColorContrast)
	SHADER_PARAMETER(FVector4f, ColorGamma)
	SHADER_PARAMETER(FVector4f, ColorGain)
	SHADER_PARAMETER(FVector4f, ColorOffset)
	SHADER_PARAMETER(FVector4f, ColorSaturationShadows)
	SHADER_PARAMETER(FVector4f, ColorContrastShadows)
	SHADER_PARAMETER(FVector4f, ColorGammaShadows)
	SHADER_PARAMETER(FVector4f, ColorGainShadows)
	SHADER_PARAMETER(FVector4f, ColorOffsetShadows)
	SHADER_PARAMETER(FVector4f, ColorSaturationMidtones)
	SHADER_PARAMETER(FVector4f, ColorContrastMidtones)
	SHADER_PARAMETER(FVector4f, ColorGammaMidtones)
	SHADER_PARAMETER(FVector4f, ColorGainMidtones)
	SHADER_PARAMETER(FVector4f, ColorOffsetMidtones)
	SHADER_PARAMETER(FVector4f, ColorSaturationHighlights)
	SHADER_PARAMETER(FVector4f, ColorContrastHighlights)
	SHADER_PARAMETER(FVector4f, ColorGammaHighlights)
	SHADER_PARAMETER(FVector4f, ColorGainHighlights)
	SHADER_PARAMETER(FVector4f, ColorOffsetHighlights)
	SHADER_PARAMETER(float, ColorCorrectionShadowsMax)
	SHADER_PARAMETER(float, ColorCorrectionHighlightsMin)
	SHADER_PARAMETER(float, ColorCorrectionHighlightsMax)
	SHADER_PARAMETER(float, WhiteTemp)
	SHADER_PARAMETER(float, WhiteTint)
	SHADER_PARAMETER(float, BlueCorrection)
	SHADER_PARAMETER(float, ExpandGamut)
	SHADER_PARAMETER(float, ToneCurveAmount)
	SHADER_PARAMETER(float, FilmSlope)
	SHADER_PARAMETER(float, FilmToe)
	SHADER_PARAMETER(float, FilmShoulder)
	SHADER_PARAMETER(float, FilmBlackClip)
	SHADER_PARAMETER(float, FilmWhiteClip)
	SHADER_PARAMETER(uint32, bIsTemperatureWhiteBalance)
	SHADER_PARAMETER(FVector3f, MappingPolynomial)
	SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapperOutputDeviceParameters, OutputDevice)
	SHADER_PARAMETER_STRUCT_INCLUDE(FCustomTonemapperParameters, CustomTonemapperParameters)
END_SHADER_PARAMETER_STRUCT()

#define UPDATE_CACHE_SETTINGS(DestParameters, ParamValue, bOutHasChanged) \
if(DestParameters != (ParamValue)) \
{ \
	DestParameters = (ParamValue); \
	bOutHasChanged = true; \
}

struct FCachedLUTSettings
{
	uint32 UniqueID = 0;
	EShaderPlatform ShaderPlatform = GMaxRHIShaderPlatform;
	FTonemapOverrideLUTParameters Parameters;
	FWorkingColorSpaceShaderParameters WorkingColorSpaceShaderParameters;
	bool bUseCompute = false;
	ECustomTonemapOperator CachedTonemapOperator;
	EGT7UCSType CachedGT7UCSType;
	
	bool UpdateCachedValues(const FViewInfo& View, uint32 LUTSize, const UTonemapOverrideSettings& TonemapOverrideSettings)
	{
		bool bHasChanged = false;
		GetCombineLUTParameters(View, LUTSize, bHasChanged);
		GetCustomLUTParameters(TonemapOverrideSettings, bHasChanged);
		UPDATE_CACHE_SETTINGS(UniqueID, View.State ? View.State->GetViewKey() : 0, bHasChanged);
		UPDATE_CACHE_SETTINGS(ShaderPlatform, View.GetShaderPlatform(), bHasChanged);
		UPDATE_CACHE_SETTINGS(bUseCompute, View.bUseComputePasses, bHasChanged);
		UPDATE_CACHE_SETTINGS(CachedTonemapOperator,TonemapOverrideSettings.CustomTonemapOperator, bHasChanged);
		UPDATE_CACHE_SETTINGS(CachedGT7UCSType,TonemapOverrideSettings.UCSType, bHasChanged);
		
		const FWorkingColorSpaceShaderParameters* InWorkingColorSpaceShaderParameters = reinterpret_cast<const FWorkingColorSpaceShaderParameters*>(GDefaultWorkingColorSpaceUniformBuffer.GetContents());
		if (InWorkingColorSpaceShaderParameters)
		{
			UPDATE_CACHE_SETTINGS(WorkingColorSpaceShaderParameters.ToXYZ, InWorkingColorSpaceShaderParameters->ToXYZ, bHasChanged);
			UPDATE_CACHE_SETTINGS(WorkingColorSpaceShaderParameters.FromXYZ, InWorkingColorSpaceShaderParameters->FromXYZ, bHasChanged);
			UPDATE_CACHE_SETTINGS(WorkingColorSpaceShaderParameters.ToAP1, InWorkingColorSpaceShaderParameters->ToAP1, bHasChanged);
			UPDATE_CACHE_SETTINGS(WorkingColorSpaceShaderParameters.FromAP1, InWorkingColorSpaceShaderParameters->FromAP1, bHasChanged);
			UPDATE_CACHE_SETTINGS(WorkingColorSpaceShaderParameters.ToAP0, InWorkingColorSpaceShaderParameters->ToAP0, bHasChanged);
			UPDATE_CACHE_SETTINGS(WorkingColorSpaceShaderParameters.bIsSRGB, InWorkingColorSpaceShaderParameters->bIsSRGB, bHasChanged);
		}

		return bHasChanged;
	}

	FVector3f GetMappingPolynomial()
	{
		static const auto CVarMinValue = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Min"));
		static const auto CVarMidValue = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Mid"));
		static const auto CVarMaxValue = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Max"));

		float MinValue = FMath::Clamp(CVarMinValue->GetFloat() , -10.0f, 10.0f);
		float MidValue = FMath::Clamp(CVarMidValue->GetFloat(), -10.0f, 10.0f);
		float MaxValue = FMath::Clamp(CVarMaxValue->GetFloat(), -10.0f, 10.0f);

		float c = MinValue;
		float b = 4 * MidValue - 3 * MinValue - MaxValue;
		float a = MaxValue - MinValue - b;

		return FVector3f(a, b, c);
	}

	void GetCombineLUTParameters(
		const FViewInfo& View,
		int32 LUTSize,
		bool& bHasChanged)
	{

		static const FPostProcessSettings DefaultSettings;

		const FSceneViewFamily& ViewFamily = *(View.Family);

		const FPostProcessSettings& Settings = ViewFamily.EngineShowFlags.ColorGrading
			? View.FinalPostProcessSettings
			: DefaultSettings;

		Parameters.WorkingColorSpace = GDefaultWorkingColorSpaceUniformBuffer.GetUniformBufferRef();

		FACESTonemapParams TonemapperParams;
		GetACESTonemapParameters(TonemapperParams);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESMinMaxData, TonemapperParams.ACESMinMaxData, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESMidData, TonemapperParams.ACESMidData, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESCoefsLow_0, TonemapperParams.ACESCoefsLow_0, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESCoefsHigh_0, TonemapperParams.ACESCoefsHigh_0, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESCoefsLow_4, TonemapperParams.ACESCoefsLow_4, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESCoefsHigh_4, TonemapperParams.ACESCoefsHigh_4, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESSceneColorMultiplier, TonemapperParams.ACESSceneColorMultiplier, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ACESTonemapParameters.ACESGamutCompression, TonemapperParams.ACESGamutCompression, bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.ColorScale, FVector3f(View.ColorScale), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.OverlayColor, FVector4f(View.OverlayColor), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.MappingPolynomial, GetMappingPolynomial(), bHasChanged);

		// White balance
		UPDATE_CACHE_SETTINGS(Parameters.bIsTemperatureWhiteBalance, uint32(Settings.TemperatureType == ETemperatureMethod::TEMP_WhiteBalance), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.LUTSize, LUTSize, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.WhiteTemp, Settings.WhiteTemp, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.WhiteTint, Settings.WhiteTint, bHasChanged);

		// Color grade
		UPDATE_CACHE_SETTINGS(Parameters.ColorSaturation, FVector4f(Settings.ColorSaturation), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorContrast, FVector4f(Settings.ColorContrast), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGamma, FVector4f(Settings.ColorGamma), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGain, FVector4f(Settings.ColorGain), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorOffset, FVector4f(Settings.ColorOffset), bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.ColorSaturationShadows, FVector4f(Settings.ColorSaturationShadows), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorContrastShadows, FVector4f(Settings.ColorContrastShadows), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGammaShadows, FVector4f(Settings.ColorGammaShadows), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGainShadows, FVector4f(Settings.ColorGainShadows), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorOffsetShadows, FVector4f(Settings.ColorOffsetShadows), bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.ColorSaturationMidtones, FVector4f(Settings.ColorSaturationMidtones), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorContrastMidtones, FVector4f(Settings.ColorContrastMidtones), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGammaMidtones, FVector4f(Settings.ColorGammaMidtones), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGainMidtones, FVector4f(Settings.ColorGainMidtones), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorOffsetMidtones, FVector4f(Settings.ColorOffsetMidtones), bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.ColorSaturationHighlights, FVector4f(Settings.ColorSaturationHighlights), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorContrastHighlights, FVector4f(Settings.ColorContrastHighlights), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGammaHighlights, FVector4f(Settings.ColorGammaHighlights), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorGainHighlights, FVector4f(Settings.ColorGainHighlights), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorOffsetHighlights, FVector4f(Settings.ColorOffsetHighlights), bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.ColorCorrectionShadowsMax, Settings.ColorCorrectionShadowsMax, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorCorrectionHighlightsMin, Settings.ColorCorrectionHighlightsMin, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ColorCorrectionHighlightsMax, Settings.ColorCorrectionHighlightsMax, bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.BlueCorrection, Settings.BlueCorrection, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ExpandGamut, Settings.ExpandGamut, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.ToneCurveAmount, Settings.ToneCurveAmount, bHasChanged);

		UPDATE_CACHE_SETTINGS(Parameters.FilmSlope, Settings.FilmSlope, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.FilmToe, Settings.FilmToe, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.FilmShoulder, Settings.FilmShoulder, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.FilmBlackClip, Settings.FilmBlackClip, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.FilmWhiteClip, Settings.FilmWhiteClip, bHasChanged);

		FTonemapperOutputDeviceParameters TonemapperOutputDeviceParameters = GetTonemapperOutputDeviceParameters(ViewFamily);
		UPDATE_CACHE_SETTINGS(Parameters.OutputDevice.InverseGamma, TonemapperOutputDeviceParameters.InverseGamma, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.OutputDevice.OutputDevice, TonemapperOutputDeviceParameters.OutputDevice, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.OutputDevice.OutputGamut, TonemapperOutputDeviceParameters.OutputGamut, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.OutputDevice.OutputMaxLuminance, TonemapperOutputDeviceParameters.OutputMaxLuminance, bHasChanged);
	}

	void GetCustomLUTParameters(
	const UTonemapOverrideSettings& TonemapOverrideSettings,
	bool& bHasChanged)
	{
//		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.TonemapOperator, int32(TonemapOverrideSettings.CustomTonemapOperator), bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.ReinhardWhitePoint, TonemapOverrideSettings.ReinhardWhitePoint, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.HejlWhitePoint, TonemapOverrideSettings.HejlWhitePoint, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.GT7BlendRatio, TonemapOverrideSettings.GT7BlendRatio, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.GT7FadeStart, TonemapOverrideSettings.GT7FadeStart, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.GT7FadeEnd, TonemapOverrideSettings.GT7FadeEnd, bHasChanged);
		
		// Use fallback texture if not set
		FTextureRHIRef LUTTexture = GBlackTexture->TextureRHI;
		FRHISamplerState* LUTSamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		
		if (TonemapOverrideSettings.CustomTonemapOperator == ECustomTonemapOperator::TonyMcMapface)
		{
			if (TonemapOverrideSettings.LUTTexture && TonemapOverrideSettings.LUTTexture->GetResource() && TonemapOverrideSettings.LUTTexture->GetResource()->TextureRHI)
			{
				LUTTexture = TonemapOverrideSettings.LUTTexture->GetResource()->TextureRHI;
			}
		}
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.LUTTexture, LUTTexture, bHasChanged);
		UPDATE_CACHE_SETTINGS(Parameters.CustomTonemapperParameters.LUTTextureSampler, LUTSamplerState, bHasChanged);
	}

};


class FTonemapOverrideLUTShaderPS : public FTonemapOverrideShaderCommon
{
public:
	DECLARE_GLOBAL_SHADER(FTonemapOverrideLUTShaderPS);
	SHADER_USE_PARAMETER_STRUCT(FTonemapOverrideLUTShaderPS, FTonemapOverrideShaderCommon);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapOverrideLUTParameters, TonemapLUTParameters)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FTonemapOverrideLUTShaderCS : public FTonemapOverrideShaderCommon
{
public:
	DECLARE_GLOBAL_SHADER(FTonemapOverrideLUTShaderCS);
	SHADER_USE_PARAMETER_STRUCT(FTonemapOverrideLUTShaderCS, FTonemapOverrideShaderCommon);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapOverrideLUTParameters, TonemapLUTParameters)
		SHADER_PARAMETER(FVector2f, OutputExtentInverse)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWOutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTonemapOverrideLUTShaderPS, "/Plugins/TonemapOverride/CustomTonemapLUT.usf", "CreateLUTPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FTonemapOverrideLUTShaderCS, "/Plugins/TonemapOverride/CustomTonemapLUT.usf", "CreateLUTCS", SF_Compute);

FTonemapOverrideSceneViewExtension::FTonemapOverrideSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
	UE_LOG(TonemapOverrideLog, Log, TEXT("Tonemap SceneViewExtension registered"));

	// TODO: Loading pre-set asset might fail
	UTonemapOverrideSettings& TonemapOverrideSettings = UTonemapOverrideSettings::Get();
	TonemapOverrideSettings.LUTTexture.LoadSynchronous();
	
}

#if ENGINE_VERSION_CUSTOM == true
void FTonemapOverrideSceneViewExtension::SubscribeToPostProcessCombineLUTPass(const FSceneView& InView, FTonemapLUTCallbackDelegateArray& LUTPassCallbacks)
{
	const UTonemapOverrideSettings& TonemapOverrideSettings = UTonemapOverrideSettings::Get();

	if (bCachedOverride != TonemapOverrideSettings.bUseCustomTonemapper)
	{
		UE_LOG(TonemapOverrideLog, Warning, TEXT("Manually refresh postprocess settings"));
		bCachedOverride = TonemapOverrideSettings.bUseCustomTonemapper;
	}

	if (TonemapOverrideSettings.bUseCustomTonemapper)
	{
		LUTPassCallbacks.Add(FTonemapLUTCallbackDelegate::CreateRaw(this, &FTonemapOverrideSceneViewExtension::CreateOverrideLUT_RenderThread));
	}
}
#else

#if UE_VERSION_OLDER_THAN(5, 5, 0)
void FTonemapOverrideSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
#else
void FTonemapOverrideSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
#endif
{
	const UTonemapOverrideSettings& TonemapOverrideSettings = UTonemapOverrideSettings::Get();

	if (TonemapOverrideSettings.bUseCustomTonemapper)
	{
		if (PassId == EPostProcessingPass::MotionBlur)
		{
			InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FTonemapOverrideSceneViewExtension::CreateOverrideLUT));
		}
	}

}
#endif

FRDGTextureRef FTonemapOverrideSceneViewExtension::RenderOverrideLUT(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef OutputTexture, FCachedLUTSettings& CachedLUTSettings, const bool bUseComputePass, const bool bUseVolumeTextureLUT, const int32 TextureLUTSize)
{
	const FIntPoint OutputViewSize(bUseVolumeTextureLUT ? TextureLUTSize : TextureLUTSize * TextureLUTSize, TextureLUTSize);

	FTonemapOverrideShaderCommon::FPermutationDomain PermutationVector;

	const float DefaultTemperature = 6500;
	const float DefaultTint = 0;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

	if (bUseComputePass)
	{
		FTonemapOverrideLUTShaderCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTonemapOverrideLUTShaderCS::FParameters>();
		PassParameters->TonemapLUTParameters = CachedLUTSettings.Parameters;
		PassParameters->OutputExtentInverse = FVector2f(1.0f, 1.0f) / FVector2f(OutputViewSize);
		PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(OutputTexture);

		const bool ShouldSkipTemperature = FMath::IsNearlyEqual(PassParameters->TonemapLUTParameters.WhiteTemp, DefaultTemperature) && FMath::IsNearlyEqual(PassParameters->TonemapLUTParameters.WhiteTint, DefaultTint);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FSkipTemperature>(ShouldSkipTemperature);

		const bool bOutputDeviceSRGB = (PassParameters->TonemapLUTParameters.OutputDevice.OutputDevice == (uint32)EDisplayOutputFormat::SDR_sRGB);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FOutputDeviceSRGB>(bOutputDeviceSRGB);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FTonemapOperator>(CachedLUTSettings.CachedTonemapOperator);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FGT7UCSType>(CachedLUTSettings.CachedGT7UCSType);

		const uint32 GroupSizeXY = FMath::DivideAndRoundUp(OutputViewSize.X, FTonemapOverrideLUTShaderCS::GroupSize);
		const uint32 GroupSizeZ = bUseVolumeTextureLUT ? GroupSizeXY : 1;

		TShaderMapRef<FTonemapOverrideLUTShaderCS> ComputeShader(GlobalShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Tonemap Create LUT CS Shader %d", TextureLUTSize),
			ComputeShader,
			PassParameters,
			FIntVector(GroupSizeXY, GroupSizeXY, GroupSizeZ));
	}
	else
	{
		FTonemapOverrideLUTShaderPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTonemapOverrideLUTShaderPS::FParameters>();
		PassParameters->TonemapLUTParameters = CachedLUTSettings.Parameters;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);

		const bool ShouldSkipTemperature = FMath::IsNearlyEqual(PassParameters->TonemapLUTParameters.WhiteTemp, DefaultTemperature) && FMath::IsNearlyEqual(PassParameters->TonemapLUTParameters.WhiteTint, DefaultTint);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FSkipTemperature>(ShouldSkipTemperature);

		const bool bOutputDeviceSRGB = (PassParameters->TonemapLUTParameters.OutputDevice.OutputDevice == (uint32)EDisplayOutputFormat::SDR_sRGB);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FOutputDeviceSRGB>(bOutputDeviceSRGB);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FTonemapOperator>(CachedLUTSettings.CachedTonemapOperator);
		PermutationVector.Set<FTonemapOverrideShaderCommon::FGT7UCSType>(CachedLUTSettings.CachedGT7UCSType);

		TShaderMapRef<FTonemapOverrideLUTShaderPS> PixelShader(View.ShaderMap, PermutationVector);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("Tonemap Create LUT PS Shader %d", TextureLUTSize),
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, PixelShader, PassParameters, bUseVolumeTextureLUT, TextureLUTSize](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

				if (bUseVolumeTextureLUT)
				{
					const FVolumeBounds VolumeBounds(TextureLUTSize);

					TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);

					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(VolumeBounds.MaxX - VolumeBounds.MinX));
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				}
				else
				{
					TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);

					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

					const int32 LUTSize = TextureLUTSize;
					FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
					UE::Renderer::PostProcess::SetDrawRectangleParameters(BatchedParameters, VertexShader.GetShader(), 0, 0, LUTSize*LUTSize, LUTSize, 0, 0, LUTSize*LUTSize, LUTSize, FIntPoint(LUTSize* LUTSize, LUTSize), FIntPoint(LUTSize* LUTSize, LUTSize));
					RHICmdList.SetBatchedShaderParameters(VertexShader.GetVertexShader(), BatchedParameters);

					FPixelShaderUtils::DrawFullscreenTriangle(RHICmdList, 1);

				}
			});

	}
	return OutputTexture;
}

#if ENGINE_VERSION_CUSTOM == true

FRDGTextureRef FTonemapOverrideSceneViewExtension::CreateOverrideLUT_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef OutputTexture)
{
	const bool bUseComputePass = uint64(OutputTexture->Desc.Flags) & uint64(ETextureCreateFlags::UAV); // ? true : false;
	const bool bUseVolumeTextureLUT = OutputTexture->Desc.Extent.X == OutputTexture->Desc.Extent.Y;
	int32 TextureLUTSize = OutputTexture->Desc.Extent.Y;

	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	static FCachedLUTSettings CachedLUTSettings;

	// Check if postprocessing values have been updated
	const UTonemapOverrideSettings& TonemapOverrideSettings = UTonemapOverrideSettings::Get();
	const bool bHasChanged = CachedLUTSettings.UpdateCachedValues(ViewInfo, TextureLUTSize, TonemapOverrideSettings);

	static const auto CVarUpdateEveryFrame = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LUT.UpdateEveryFrame"));

	if (bHasChanged || CVarUpdateEveryFrame->GetInt() > 0)
	{
		RenderOverrideLUT(GraphBuilder, ViewInfo, OutputTexture, CachedLUTSettings, bUseComputePass, bUseVolumeTextureLUT, TextureLUTSize);
	}

	return OutputTexture;
}

#else

FScreenPassTexture FTonemapOverrideSceneViewExtension::CreateOverrideLUT(FRDGBuilder& GraphBuilder, const FSceneView& SceneView, const FPostProcessMaterialInputs& Inputs)
{
	// Save SceneColor for exit and exit early if not a valid pass
	const FScreenPassTexture& SceneColor = Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	if (!SceneColor.IsValid()) return SceneColor;
	
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneView.GetFeatureLevel());
	const FSceneViewFamily& ViewFamily = *(SceneView.Family);

	const FViewInfo& View = static_cast<const FViewInfo&>(SceneView);
	
	// PostprocessCombineLUT uses EyeAdaptationViewState (not View.Viewstate), so we access the same here
	// Based on testing, this would work from the given SceneView as well
	FSceneViewStateInterface* Interface = GET_PRIVATE(FSceneView, &View, EyeAdaptationViewState);
	FSceneViewState* ViewState = static_cast<FSceneViewState*>(Interface);

	static FCachedLUTSettings CachedLUTSettings;
	
	// Engine LUT adds the LDR Luts here, but we skip them as those shouldn't be used with HDR grading in the first place at all
	
	bool _bUseFloatOutput = false;

	const bool bUseComputePass = View.bUseComputePasses;
	const bool bUseVolumeTextureLUT =  FTonemapOverrideLUTShaderCS::PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(View.GetShaderPlatform());
	const bool bUseFloatOutput = ViewFamily.SceneCaptureSource == SCS_FinalColorHDR || ViewFamily.SceneCaptureSource == SCS_FinalToneCurveHDR;

	if (_bUseFloatOutput != bUseFloatOutput)
	{
		UE_LOG(LogTemp, Log, TEXT("Use float output not consistent"));
		return SceneColor;
	}

	// Use which ever LUT size is the biggest (native engine or one implemented in settings)  
	static const auto CVarLUTSize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LUT.Size"));
	const int32 TextureLUTSize = CVarLUTSize->GetInt();

	FRDGTextureRef OutputTexture = nullptr;
	
	// Not being able to register viewstate (ie. when hovering thumbnail in editor) will fail the update of tonemapLUT
	// Have tried to cache the texture pointers and viewstate, but none really works here, maybe there is a solution
	// This is a hack anyways so maybe some flickering in editor can be tolerated
	if (ViewState)
	{
		IPooledRenderTarget* CombinedLUTRenderTarget = GET_PRIVATE_REF(FSceneViewState, ViewState, CombinedLUTRenderTarget);
		OutputTexture = TryRegisterExternalTexture(GraphBuilder, CombinedLUTRenderTarget);
	}
	
	if (!OutputTexture)
	{
		// Either this is not supported or the creation of persistent texture has failed
		// There is nothing we can really do
		UE_LOG(TonemapOverrideLog, Error, TEXT("LUT Texture register/creation failed"));
		return SceneColor;
	}

	bProcessed = true;
	
	// Check if postprocessing values have been updated
	const UTonemapOverrideSettings& TonemapOverrideSettings = UTonemapOverrideSettings::Get();
	const bool bHasChanged = CachedLUTSettings.UpdateCachedValues(View, TextureLUTSize, TonemapOverrideSettings);

	// Doesn't really work as the editor might overwrite our LUT texture later with updated values
	// So we need to be regenerating this, but might work better in runtime
	// if (!bHasChanged) return SceneColor;

	// Hack viewinfo to set postprocess settings to default to prevent the settings-cache update at the tonemapLUT pass
	FViewInfo& nonConstView = const_cast<FViewInfo&>(View);
	nonConstView.FinalPostProcessSettings = FFinalPostProcessSettings();
	
	// Another hack, set viewfamily to skip colorgrading
	FSceneViewFamily& nonConstViewFamily = const_cast<FSceneViewFamily&>(ViewFamily);
	nonConstViewFamily.EngineShowFlags.ColorGrading = 0;

	RenderOverrideLUT(GraphBuilder, View, OutputTexture, CachedLUTSettings, bUseComputePass, bUseVolumeTextureLUT, TextureLUTSize);

	return SceneColor;
}

#endif