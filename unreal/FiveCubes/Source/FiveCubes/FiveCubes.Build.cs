using UnrealBuildTool;
public class FiveCubes : ModuleRules
{
    public FiveCubes(ReadOnlyTargetRules Target) : base(Target)
    {
        foreach (var File in System.IO.Directory.GetFiles(System.IO.Path.Combine(ModuleDirectory, "../../Content/Lab"), "*.json"))
            RuntimeDependencies.Add(File, StagedFileType.NonUFS);
        PublicFrameworks.Add("AVFoundation");
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "WebSockets", "Json", "JsonUtilities", "AudioCaptureCore", "AudioCapture", "AudioMixer", "AudioExtensions", "UMG", "Slate", "SlateCore"
        });
    }
}
