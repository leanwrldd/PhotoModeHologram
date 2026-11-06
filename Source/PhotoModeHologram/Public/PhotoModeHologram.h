#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPhotoModeHologram, Log, All)

class AFGCharacterPlayer;

class FPhotoModeHologramModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/// Stops photo mode from clearing the build gun, and locks the hologram while photo mode is open.
	static void KeepBuildGunInPhotoMode();

	/// Locks the current hologram so it stops following the view.
	static void LockHologram(AFGCharacterPlayer* character, bool wasLocked);

	/// Releases the hologram, but only if LockHologram was what locked it.
	static void UnlockHologram(AFGCharacterPlayer* character);
};
