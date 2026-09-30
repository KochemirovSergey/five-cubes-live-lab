using UnrealBuildTool;
public class FiveCubes : ModuleRules
{
    public FiveCubes(ReadOnlyTargetRules Target) : base(Target)
    {
        RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Content/Lab/scenario.json"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Content/Lab/targets.json"), StagedFileType.NonUFS);
        PublicFrameworks.Add("AVFoundation");
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "WebSockets", "Json", "JsonUtilities", "AudioCaptureCore", "AudioCapture", "AudioMixer", "UMG", "Slate", "SlateCore"
        });
    }
}
