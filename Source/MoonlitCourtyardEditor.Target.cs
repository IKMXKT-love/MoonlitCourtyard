using UnrealBuildTool;
using System.Collections.Generic;
public class MoonlitCourtyardEditorTarget : TargetRules
{
    public MoonlitCourtyardEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("MoonlitCourtyard");
    }
}
