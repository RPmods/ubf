using UnrealBuildTool;

public class UBF : ModuleRules
{
	public UBF(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore",
			"Media", "MediaAssets", "ImageWrapper"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "AudioMixer" });
	}
}
