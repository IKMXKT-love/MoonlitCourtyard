using UnrealBuildTool;
public class CourtyardAnimationEditor : ModuleRules
{
 public CourtyardAnimationEditor(ReadOnlyTargetRules Target):base(Target){
 PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
 PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","MoonlitCourtyard","AnimGraphRuntime","AnimGraph","BlueprintGraph","UnrealEd"});
 }
}
