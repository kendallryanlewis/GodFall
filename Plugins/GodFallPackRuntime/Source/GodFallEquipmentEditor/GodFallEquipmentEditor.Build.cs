using UnrealBuildTool;
public class GodFallEquipmentEditor : ModuleRules
{
    public GodFallEquipmentEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GodFallPackRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "Slate", "SlateCore", "InputCore", "ToolMenus", "PropertyEditor", "AssetRegistry", "AssetTools", "ContentBrowser", "PythonScriptPlugin", "Kismet", "AdvancedPreviewScene" });
    }
}
