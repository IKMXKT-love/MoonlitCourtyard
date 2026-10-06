#include "CourtyardPreferences.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace { FString PreferencePath() { return FPaths::ProjectSavedDir()/TEXT("Config/CourtyardPreferences.json"); } }

FCourtyardPreferences::FCourtyardPreferences()
{
    Keys = { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::LeftShift, EKeys::LeftControl,
        EKeys::C, EKeys::X, EKeys::SpaceBar, EKeys::E, EKeys::F, EKeys::M };
}

FString FCourtyardPreferences::ActionName(int32 Index)
{
    static const TCHAR* Names[] = { TEXT("前进"), TEXT("向左"), TEXT("后退"), TEXT("向右"), TEXT("按住加速"),
        TEXT("蹲下 / 起身"), TEXT("趴下 / 起身"), TEXT("仰躺 / 起身"), TEXT("跳跃"), TEXT("艾玛回应"), TEXT("座椅坐起"), TEXT("庭院地图") };
    return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("");
}

bool FCourtyardPreferences::IsBindable(FKey Key)
{
    return Key.IsValid() && !Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsAxis1D()
        && !Key.IsAxis2D() && !Key.IsAxis3D() && Key != EKeys::Escape && Key != EKeys::AnyKey
        && Key != EKeys::LeftAlt && Key != EKeys::RightAlt && Key != EKeys::LeftCommand && Key != EKeys::RightCommand;
}

void FCourtyardPreferences::Sanitize()
{
    if (Version != 1) { *this = FCourtyardPreferences(); return; }
    auto Bound = [](float V, float Lo, float Hi, float Default) { return FMath::IsFinite(V) ? FMath::Clamp(V, Lo, Hi) : Default; };
    Master = Bound(Master, 0, 1, 1); Music = Bound(Music, 0, 1, 1); Environment = Bound(Environment, 0, 1, 1);
    Sensitivity = Bound(Sensitivity, .2f, 3.f, 1.f); FOV = Bound(FOV, 60, 100, 80); Gamma = Bound(Gamma, 1.8f, 2.6f, 2.2f);
    if(WindowMode<0 || WindowMode>2)WindowMode=1;
    if (Resolution.X < 800 || Resolution.Y < 600 || Resolution.X > 16384 || Resolution.Y > 16384)
        Resolution = FIntPoint::ZeroValue;
    if (FrameLimit != 0 && FrameLimit != 30 && FrameLimit != 60 && FrameLimit != 120) FrameLimit = 30;
    Preset = FMath::Clamp(Preset, -1, 3);
    BasePreset = FMath::Clamp(BasePreset, 0, 3);
    Shadows = FMath::Clamp(Shadows, 0, 3); AntiAliasing = FMath::Clamp(AntiAliasing, 0, 3);
    Post = FMath::Clamp(Post, 0, 3); Foliage = FMath::Clamp(Foliage, 0, 3);
    if (Preset >= 0 && (Shadows != Preset || AntiAliasing != Preset || Post != Preset || Foliage != Preset)) Preset = -1;
    bool ValidKeys = Keys.Num() == static_cast<int32>(ECourtyardAction::Count);
    TSet<FKey> Seen;
    for (const FKey Key : Keys) { if (!IsBindable(Key) || Seen.Contains(Key)) ValidKeys = false; Seen.Add(Key); }
    if (!ValidKeys) Keys = FCourtyardPreferences().Keys;
}

void FCourtyardPreferences::SetPreset(int32 Level)
{
    Preset = FMath::Clamp(Level, 0, 3);
    BasePreset = Preset;
    Shadows = AntiAliasing = Post = Foliage = Preset;
}

void FCourtyardPreferences::ApplyEngine(bool ApplyDisplay) const
{
    if (!GEngine) return;
    UGameUserSettings* S = GEngine->GetGameUserSettings();
    if (!S) return;
    S->SetOverallScalabilityLevel(BasePreset);
    S->SetShadowQuality(Shadows); S->SetAntiAliasingQuality(AntiAliasing);
    S->SetPostProcessingQuality(Post); S->SetFoliageQuality(Foliage);
    S->SetVSyncEnabled(VSync); S->SetFrameRateLimit(FrameLimit);
    S->ApplyNonResolutionSettings();
    // Preserve the courtyard's existing texture budget for both launch paths.
    if (auto* Pool = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize")))
        Pool->Set(2048, ECVF_SetByGameSetting);
    GEngine->DisplayGamma = Gamma;
    if (ApplyDisplay)
    {
        const FIntPoint Desktop = S->GetDesktopResolution();
        S->SetFullscreenMode(static_cast<EWindowMode::Type>(WindowMode));
        S->SetScreenResolution(WindowMode == 1 || Resolution == FIntPoint::ZeroValue ? Desktop : Resolution);
        // Do not save a trial mode: only our confirmed preferences are authoritative on launch.
        S->ApplyResolutionSettings(false);
    }
}

bool FCourtyardPreferences::DisplayDiffers(const FCourtyardPreferences& Other) const
{
    return WindowMode != Other.WindowMode || (WindowMode != 1 && Resolution != Other.Resolution);
}

FCourtyardPreferences UCourtyardPreferencesSave::Load(bool& Found)
{
    Found=false;FCourtyardPreferences V;const FString Path=PreferencePath();
    const int64 Size=IFileManager::Get().FileSize(*Path);
    if(Size<=0 || Size>65536)return V;
    FString Text;TSharedPtr<FJsonObject> J;
    if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid())return V;
    double Version=0;if(!J->TryGetNumberField(TEXT("version"),Version) || Version!=1)return V;
    auto F=[&](const TCHAR* K,float& X){double N;if(J->TryGetNumberField(K,N)&&FMath::IsFinite(N)&&FMath::Abs(N)<=MAX_flt)X=(float)N;};
    auto I=[&](const TCHAR* K,int32& X){double N;if(J->TryGetNumberField(K,N)&&FMath::IsFinite(N)&&N>=MIN_int32&&N<=MAX_int32)X=(int32)N;};
    F(TEXT("master"),V.Master);F(TEXT("music"),V.Music);F(TEXT("environment"),V.Environment);
    F(TEXT("sensitivity"),V.Sensitivity);F(TEXT("fov"),V.FOV);F(TEXT("gamma"),V.Gamma);
    I(TEXT("windowMode"),V.WindowMode);I(TEXT("width"),V.Resolution.X);I(TEXT("height"),V.Resolution.Y);
    I(TEXT("frameLimit"),V.FrameLimit);I(TEXT("preset"),V.Preset);I(TEXT("basePreset"),V.BasePreset);
    I(TEXT("shadows"),V.Shadows);I(TEXT("antiAliasing"),V.AntiAliasing);I(TEXT("post"),V.Post);I(TEXT("foliage"),V.Foliage);
    J->TryGetBoolField(TEXT("musicEnabled"),V.MusicEnabled);J->TryGetBoolField(TEXT("vsync"),V.VSync);
    const TArray<TSharedPtr<FJsonValue>>* Keys=nullptr;
    if(J->TryGetArrayField(TEXT("keys"),Keys) && Keys->Num()==12)
    {
        TArray<FKey> Read;for(const auto& Item:*Keys){FString K;if(!Item.IsValid()||!Item->TryGetString(K)||K.Len()>64||K.IsEmpty()){Read.Empty();break;}Read.Add(FKey(*K));}
        if(Read.Num()==12)V.Keys=Read;
    }
    V.Sanitize();Found=true;return V;
}

bool UCourtyardPreferencesSave::Store(const FCourtyardPreferences& Preferences)
{
    FCourtyardPreferences V=Preferences;V.Sanitize();auto J=MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("version"),1);
    J->SetNumberField(TEXT("master"),V.Master);J->SetNumberField(TEXT("music"),V.Music);J->SetNumberField(TEXT("environment"),V.Environment);
    J->SetNumberField(TEXT("sensitivity"),V.Sensitivity);J->SetNumberField(TEXT("fov"),V.FOV);J->SetNumberField(TEXT("gamma"),V.Gamma);
    J->SetNumberField(TEXT("windowMode"),V.WindowMode);J->SetNumberField(TEXT("width"),V.Resolution.X);J->SetNumberField(TEXT("height"),V.Resolution.Y);
    J->SetNumberField(TEXT("frameLimit"),V.FrameLimit);J->SetNumberField(TEXT("preset"),V.Preset);J->SetNumberField(TEXT("basePreset"),V.BasePreset);
    J->SetNumberField(TEXT("shadows"),V.Shadows);J->SetNumberField(TEXT("antiAliasing"),V.AntiAliasing);J->SetNumberField(TEXT("post"),V.Post);J->SetNumberField(TEXT("foliage"),V.Foliage);
    J->SetBoolField(TEXT("musicEnabled"),V.MusicEnabled);J->SetBoolField(TEXT("vsync"),V.VSync);
    TArray<TSharedPtr<FJsonValue>> Keys;for(FKey K:V.Keys)Keys.Add(MakeShared<FJsonValueString>(K.ToString()));J->SetArrayField(TEXT("keys"),Keys);
    FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
    const FString Path=PreferencePath(),Temp=Path+TEXT(".tmp");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    return FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) && IFileManager::Get().Move(*Path,*Temp,true,false);
}
