#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "CourtyardPreferences.generated.h"

UENUM()
enum class ECourtyardAction : uint8
{
    Forward, Left, Back, Right, Sprint, Crouch, Prone, Supine, Jump, Respond, Seat, Map, Count
};

USTRUCT()
struct FCourtyardPreferences
{
    GENERATED_BODY()
    UPROPERTY() int32 Version = 1;
    UPROPERTY() float Master = 1.f;
    UPROPERTY() float Music = 1.f;
    UPROPERTY() float Environment = 1.f;
    UPROPERTY() bool MusicEnabled = true;
    UPROPERTY() float Sensitivity = 1.f;
    UPROPERTY() float FOV = 80.f;
    UPROPERTY() float Gamma = 2.2f;
    UPROPERTY() int32 WindowMode = 1;
    UPROPERTY() FIntPoint Resolution = FIntPoint::ZeroValue;
    UPROPERTY() bool VSync = false;
    UPROPERTY() int32 FrameLimit = 30;
    UPROPERTY() int32 Preset = 2;
    UPROPERTY() int32 BasePreset = 2;
    UPROPERTY() int32 Shadows = 2;
    UPROPERTY() int32 AntiAliasing = 2;
    UPROPERTY() int32 Post = 2;
    UPROPERTY() int32 Foliage = 2;
    UPROPERTY() TArray<FKey> Keys;

    FCourtyardPreferences();
    void Sanitize();
    void SetPreset(int32 Level);
    void ApplyEngine(bool ApplyDisplay) const;
    bool DisplayDiffers(const FCourtyardPreferences& Other) const;
    static FString ActionName(int32 Index);
    static bool IsBindable(FKey Key);
};

// Only preferences are persisted. World, NPC and player pose state never enter this file.
UCLASS()
class MOONLITCOURTYARD_API UCourtyardPreferencesSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() FCourtyardPreferences Confirmed;
    static FCourtyardPreferences Load(bool& Found);
    static bool Store(const FCourtyardPreferences& Preferences);
};
