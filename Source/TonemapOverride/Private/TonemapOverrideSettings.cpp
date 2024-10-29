// Copyright 2024 Ossi Luoto


#include "TonemapOverrideSettings.h"


UTonemapOverrideSettings& UTonemapOverrideSettings::Get()
{
	UTonemapOverrideSettings* MutableCDO = GetMutableDefault<UTonemapOverrideSettings>();
	check(MutableCDO != nullptr)
	return *MutableCDO;
}