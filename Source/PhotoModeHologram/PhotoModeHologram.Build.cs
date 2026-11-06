using UnrealBuildTool;

public class PhotoModeHologram : ModuleRules
{
	public PhotoModeHologram(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject",
			"Engine",
			"InputCore", "EnhancedInput",
			"GameplayTags", "NetCore",
		});

		// Header stubs
		PublicDependencyModuleNames.AddRange(new string[] {
			"DummyHeaders",
		});

		PublicDependencyModuleNames.AddRange(new string[] {"FactoryGame", "SML"});
	}
}
