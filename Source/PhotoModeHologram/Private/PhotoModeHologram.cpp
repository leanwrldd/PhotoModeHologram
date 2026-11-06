#include "PhotoModeHologram.h"

#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "FGPhotoModeComponent.h"
#include "FGPlayerController.h"
#include "Hologram/FGHologram.h"
#include "Patching/NativeHookManager.h"

DEFINE_LOG_CATEGORY(LogPhotoModeHologram)

// How vanilla 1.2 drops the hologram, read from the shipping game's disassembly (CL 517554):
//
//  - UFGPhotoModeComponent::EnterPhotoMode and ExitPhotoMode both call
//    AFGCharacterPlayer::HandlePhotoModeOnCharacter on the local client.
//  - That function starts with: if mBuildGun is equipped, Server_ClearOverrideEquipment(mBuildGun).
//    Removing the build gun ends the build state, which destroys the hologram.
//  - Photo mode then moves the player controller to the camera pawn. AFGCharacterPlayer::
//    UnPossessed unequips whatever is in the arms slot, and the build gun is that slot's override
//    equipment, so it is a second way to lose the gun. It runs on the server too.
//  - ExitPhotoMode clears mIsPhotoModeOn, then swaps back to the character, and only calls
//    HandlePhotoModeOnCharacter(false) after that, so GetIsPhotoModeOn is already false while
//    the gun is at risk on the way out.
//  - AFGPlayerController::AcknowledgePossession runs when the character comes back. After calling
//    the engine version it turns the build gun off, unequips it, then makes a virtual call on the
//    gun directly. Blocking only the unequip leaves the gun half removed and unusable until the
//    player respawns, so this whole function has to be skipped instead.
//  - HandlePhotoModeOnCharacter also disables the Actions, EquipmentHandling and EquipmentActions input contexts, so the
//    build gun cannot fire while photo mode is open, even with the gun still equipped.
//  - AFGHologram::LockHologramPosition checks CanLockHologram itself, so an unlockable hologram
//    is simply left unlocked.

namespace
{
	// True from HandlePhotoModeOnCharacter(true) until HandlePhotoModeOnCharacter(false) returns,
	// which covers both pawn swaps. Outside this window the build gun behaves normally.
	bool GKeepBuildGun = false;

	// The hologram this mod locked. A hologram the player had already locked stays locked on exit.
	TWeakObjectPtr<AFGHologram> GHologramWeLocked;

	UFGBuildGunStateBuild* GetBuildState(const AFGCharacterPlayer* character)
	{
		const AFGBuildGun* buildGun = character->GetBuildGun();
		return buildGun ? Cast<UFGBuildGunStateBuild>(buildGun->GetCurrentState()) : nullptr;
	}

	// Only ever protects a gun that is really equipped, so a gun the player had put away can
	// still be equipped normally. The server never runs HandlePhotoModeOnCharacter, so it reads
	// the component instead.
	bool ShouldKeepBuildGun(const AFGCharacterPlayer* character)
	{
		const UFGPhotoModeComponent* photoMode = character->GetCachedPhotoModeComponent();
		return character->IsBuildGunEquipped() && (GKeepBuildGun || (photoMode && photoMode->GetIsPhotoModeOn()));
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
			const UFGBuildGunStateBuild* buildState = GetBuildState(character);
			const AFGHologram* hologram = buildState ? buildState->GetHologram() : nullptr;
			const bool wasLocked = hologram && hologram->IsHologramLocked();
			UE_LOG(LogPhotoModeHologram, Log, TEXT("Photo mode %s. Hologram: %s, locked: %d"),
				enabled ? TEXT("opening") : TEXT("closing"), *GetNameSafe(hologram), wasLocked);

			GKeepBuildGun = true;
			scope(character, enabled);
			GKeepBuildGun = enabled;

			if (enabled)
				LockHologram(character, wasLocked);
			else
			{
				UnlockHologram(character);

				const UFGBuildGunStateBuild* buildStateAfter = GetBuildState(character);
				UE_LOG(LogPhotoModeHologram, Log, TEXT("Photo mode closed. Gun equipped: %d, hologram: %s"),
					character->IsBuildGunEquipped(),
					*GetNameSafe(buildStateAfter ? buildStateAfter->GetHologram() : nullptr));
			}
		});

	SUBSCRIBE_UOBJECT_METHOD(AFGPlayerController, AcknowledgePossession,
		[](auto& scope, AFGPlayerController* controller, APawn* pawn)
		{
			const AFGCharacterPlayer* character = Cast<AFGCharacterPlayer>(pawn);
			if (GKeepBuildGun && character && character->IsBuildGunEquipped())
			{
				UE_LOG(LogPhotoModeHologram, Log, TEXT("Skipped AcknowledgePossession's build gun reset."));
				controller->APlayerController::AcknowledgePossession(pawn);
				scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD(AFGCharacterPlayer::Server_ClearOverrideEquipment,
		[](auto& scope, AFGCharacterPlayer* character, AFGEquipment* equipment)
		{
			if (GKeepBuildGun && equipment == character->GetBuildGun())
			{
				UE_LOG(LogPhotoModeHologram, Log, TEXT("Blocked Server_ClearOverrideEquipment for the build gun."));
				scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD(AFGCharacterPlayer::UnequipEquipment,
		[](auto& scope, AFGCharacterPlayer* character, AFGEquipment* equipment)
		{
			if (equipment == character->GetBuildGun() && ShouldKeepBuildGun(character))
			{
				UE_LOG(LogPhotoModeHologram, Log, TEXT("Blocked UnequipEquipment for the build gun."));
				scope.Cancel();
			}
		});

	// Equipping the gun again would run AFGBuildGun::Equip, which resets its state and so ends
	// the build state. It never left the character, so there is nothing to equip.
	SUBSCRIBE_METHOD(AFGCharacterPlayer::EquipEquipment,
		[](auto& scope, AFGCharacterPlayer* character, AFGEquipment* equipment)
		{
			if (equipment == character->GetBuildGun() && ShouldKeepBuildGun(character))
			{
				UE_LOG(LogPhotoModeHologram, Log, TEXT("Blocked EquipEquipment for the build gun."));
				scope.Cancel();
			}
		});
}

void FPhotoModeHologramModule::LockHologram(AFGCharacterPlayer* character, bool wasLocked)
{
	GHologramWeLocked.Reset();

	UFGBuildGunStateBuild* buildState = GetBuildState(character);
	AFGHologram* hologram = buildState ? buildState->GetHologram() : nullptr;
	if (!hologram)
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Entered photo mode with no hologram."));
		return;
	}

	if (wasLocked)
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Hologram %s was already locked; leaving it."), *hologram->GetName());
		return;
	}

	// Opening photo mode can lock the hologram by itself, so check before toggling it back off.
	if (!hologram->IsHologramLocked())
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
