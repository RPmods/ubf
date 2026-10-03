using UnrealBuildTool;

public class UBF : ModuleRules
{
	public UBF(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore",
			"Media", "MediaAssets", "ImageWrapper"
		});
	}
}
