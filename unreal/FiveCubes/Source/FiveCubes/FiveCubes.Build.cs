using UnrealBuildTool;
public class FiveCubes : ModuleRules
{
    public FiveCubes(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicFrameworks.Add("AVFoundation");
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "WebSockets", "Json", "JsonUtilities", "AudioCaptureCore", "AudioCapture", "AudioMixer", "UMG", "Slate", "SlateCore"
        });
    }
}
