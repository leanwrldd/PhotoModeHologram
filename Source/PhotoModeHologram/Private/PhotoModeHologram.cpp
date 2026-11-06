#include "PhotoModeHologram.h"

#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "Hologram/FGHologram.h"
#include "Patching/NativeHookManager.h"

DEFINE_LOG_CATEGORY(LogPhotoModeHologram)

// How vanilla 1.2 drops the hologram, read from the shipping game's disassembly (CL 517554):
//
//  - UFGPhotoModeComponent::EnterPhotoMode and ExitPhotoMode both call
//    AFGCharacterPlayer::HandlePhotoModeOnCharacter on the local client.
//  - That function starts with: if mBuildGun is equipped, Server_ClearOverrideEquipment(mBuildGun).
//    Removing the build gun ends the build state, which destroys the hologram. Nothing else in
//    photo mode touches the build gun; UnPossessed unequips the hand slot and the next equipment
//    member after mBuildGun, not mBuildGun itself.
//  - It then disables the Actions, EquipmentHandling and EquipmentActions input contexts, so the
//    build gun cannot fire while photo mode is open, even with the gun still equipped.
//  - Server_TogglePhotoMode_Implementation and Server_ExitPhotoMode_Implementation do no work
//    on the character, so stopping the clear on the client stops it everywhere.
//  - AFGHologram::LockHologramPosition checks CanLockHologram itself, so an unlockable hologram
//    is simply left unlocked.

namespace
{
	// True only while HandlePhotoModeOnCharacter runs, so a real build gun clear is untouched.
	bool GInPhotoModeHandler = false;

	// The hologram this mod locked. A hologram the player had already locked stays locked on exit.
	TWeakObjectPtr<AFGHologram> GHologramWeLocked;

	UFGBuildGunStateBuild* GetBuildState(const AFGCharacterPlayer* character)
	{
		const AFGBuildGun* buildGun = character->GetBuildGun();
		return buildGun ? Cast<UFGBuildGunStateBuild>(buildGun->GetCurrentState()) : nullptr;
	}
}

void FPhotoModeHologramModule::StartupModule()
{
	// In editor builds FactoryGame's functions are link stubs too short to patch, and a failed
	// hook is fatal - it kills the editor and Alpakit's cook step.
#if !WITH_EDITOR
	KeepBuildGunInPhotoMode();
	UE_LOG(LogPhotoModeHologram, Log, TEXT("Hooks registered."));
#endif
}

void FPhotoModeHologramModule::ShutdownModule()
{
}

void FPhotoModeHologramModule::KeepBuildGunInPhotoMode()
{
	SUBSCRIBE_METHOD(AFGCharacterPlayer::HandlePhotoModeOnCharacter,
		[](auto& scope, AFGCharacterPlayer* character, bool enabled)
		{
			GInPhotoModeHandler = true;
			scope(character, enabled);
			GInPhotoModeHandler = false;

			if (enabled)
				LockHologram(character);
			else
				UnlockHologram(character);
		});

	SUBSCRIBE_METHOD(AFGCharacterPlayer::Server_ClearOverrideEquipment,
		[](auto& scope, AFGCharacterPlayer* character, AFGEquipment* equipment)
		{
			if (GInPhotoModeHandler && equipment == character->GetBuildGun())
			{
				UE_LOG(LogPhotoModeHologram, Log, TEXT("Kept the build gun equipped."));
				scope.Cancel();
			}
		});
}

void FPhotoModeHologramModule::LockHologram(AFGCharacterPlayer* character)
{
	GHologramWeLocked.Reset();

	UFGBuildGunStateBuild* buildState = GetBuildState(character);
	AFGHologram* hologram = buildState ? buildState->GetHologram() : nullptr;
	if (!hologram)
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Entered photo mode with no hologram."));
		return;
	}

	if (hologram->IsHologramLocked())
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Hologram %s was already locked; leaving it."), *hologram->GetName());
		return;
	}

	buildState->ToggleHologramPositionLock();

	if (hologram->IsHologramLocked())
	{
		GHologramWeLocked = hologram;
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Locked hologram %s."), *hologram->GetName());
	}
	else
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Hologram %s cannot be locked; it will follow the view."), *hologram->GetName());
	}
}

void FPhotoModeHologramModule::UnlockHologram(AFGCharacterPlayer* character)
{
	AFGHologram* lockedHologram = GHologramWeLocked.Get();
	GHologramWeLocked.Reset();
	if (!lockedHologram)
		return;

	UFGBuildGunStateBuild* buildState = GetBuildState(character);
	if (buildState && buildState->GetHologram() == lockedHologram && lockedHologram->IsHologramLocked())
	{
		buildState->ToggleHologramPositionLock();
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Unlocked hologram %s."), *lockedHologram->GetName());
	}
	else
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Locked hologram changed during photo mode; nothing to unlock."));
	}
}

IMPLEMENT_MODULE(FPhotoModeHologramModule, PhotoModeHologram)
