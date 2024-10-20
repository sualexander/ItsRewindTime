// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRewindCodeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
