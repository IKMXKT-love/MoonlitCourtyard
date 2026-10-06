#include "CourtyardController.h"
#include "EmmaMemorySave.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace {
bool ReviewSession(){return FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"));}
bool MemoryWriteBlocked=false;
FString MemoryPath()
{
#if WITH_EDITOR
    const TCHAR* File=TEXT("EmmaMemory_Editor.sav");
#else
    const TCHAR* File=TEXT("EmmaMemory.sav");
#endif
    return FPaths::Combine(FPlatformProcess::UserSettingsDir(),TEXT("MoonlitCourtyard"),File);
}
bool WriteMemory(UEmmaMemorySave* Save)
{
    if(MemoryWriteBlocked || ReviewSession())return false;
    TArray<uint8> Bytes;
    if(!UGameplayStatics::SaveGameToMemory(Save,Bytes))return false;
    const FString Path=MemoryPath(),Temp=Path+TEXT(".tmp");
    auto& Files=IFileManager::Get();
    if(!Files.MakeDirectory(*FPaths::GetPath(Path),true) || !FFileHelper::SaveArrayToFile(Bytes,*Temp))return false;
    // IFileManager::Move(Replace=true) deletes Dest first in UE 5.7.
    // These files share a directory/volume: replace by rename without that gap.
#if PLATFORM_WINDOWS
    return ::MoveFileExW(*Temp,*Path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    // This project ships on Windows; do not delete a previous save on other platforms.
    return !Files.FileExists(*Path) && Files.Move(*Path,*Temp,false,false,false,true);
#endif
}
UEmmaMemorySave* ReadMemory()
{
    TArray<uint8> Bytes;const FString Path=MemoryPath();
    if(IFileManager::Get().FileExists(*Path)){
        auto* Loaded=FFileHelper::LoadFileToArray(Bytes,*Path)?Cast<UEmmaMemorySave>(UGameplayStatics::LoadGameFromMemory(Bytes)):nullptr;
        if(Loaded && Loaded->Valid())return Loaded;
        MemoryWriteBlocked=true;
        UE_LOG(LogTemp,Warning,TEXT("COURTYARD_MEMORY invalid fixed file preserved; writes blocked path=%s"),*Path);
        return nullptr;
    }
    auto* Legacy=Cast<UEmmaMemorySave>(UGameplayStatics::LoadGameFromSlot(TEXT("EmmaMemory"),0));
    if(Legacy && Legacy->Valid() && !ReviewSession())
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_MEMORY migrated=%d path=%s"),WriteMemory(Legacy),*Path);
    return Legacy;
}
// User decision 2026-10-04: no affection, no event-driven mood. Each visit starts in a random mood and
// she drifts to another one every few minutes. Weights: calm 35, happy 35, sleepy 15, lonely 15.
const TCHAR* MoodNames[]={TEXT("calm"),TEXT("happy"),TEXT("sleepy"),TEXT("lonely")};
const float MoodWeights[]={35.f,35.f,15.f,15.f};
constexpr float MoodMinSeconds=240.f, MoodMaxSeconds=480.f;
}
FString ACourtyardController::RandomMood(const FString& Except) const
{
    float Total=0;for(int32 N=0;N<4;++N)if(Except!=MoodNames[N])Total+=MoodWeights[N];
    float Pick=FMath::FRandRange(0.f,Total);
    for(int32 N=0;N<4;++N){if(Except==MoodNames[N])continue;Pick-=MoodWeights[N];if(Pick<=0)return MoodNames[N];}
    return Except==MoodNames[0]?MoodNames[1]:MoodNames[0];
}
void ACourtyardController::SetMood(const FString& NewMood,const TCHAR* Why)
{
    const double Now=GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_MOOD from=%s to=%s reason=%s next_shift_in=%.0f time=%.2f"),*Mood,*NewMood,Why,MoodShiftAt-Now,Now);
    Mood=NewMood;MoodChangedAt=Now;
}
void ACourtyardController::InitializeMood()
{
    if(MoodReady || !LivelyEnabled)return;
    MoodReady=true;
    // Memory keeps only when she last saw the player (for the long-absence reunion), never affinity.
    if(auto* S=ReadMemory();S && S->Valid()){
        VisitCount=S->VisitCount;
        LastVisitHours=FMath::Max(0.,(FDateTime::UtcNow()-S->LastVisitUtc).GetTotalHours());
    }
    ++VisitCount;
    const double Now=GetWorld()->GetTimeSeconds();
    Mood=RandomMood(TEXT(""));MoodChangedAt=Now;MoodShiftAt=Now+FMath::FRandRange(MoodMinSeconds,MoodMaxSeconds);
    MemorySaveAt=Now+120;LastPlayerSeen=Now;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_MOOD init=%s next_shift_in=%.0f hours=%.6f visits=%d review=%d time=%.2f"),*Mood,MoodShiftAt-Now,LastVisitHours,VisitCount,ReviewSession(),Now);
}
void ACourtyardController::SaveEmmaMemory()
{
    if(!MoodReady || ReviewSession() || ReviewMemoryOverride)return;
    auto* S=Cast<UEmmaMemorySave>(UGameplayStatics::CreateSaveGameObject(UEmmaMemorySave::StaticClass()));
    if(!S)return;
    // Affection stays in the save layout for compatibility but is always 0 and never read.
    S->Affection=0;S->LastVisitUtc=FDateTime::UtcNow();S->VisitCount=VisitCount;S->LastMood=Mood;
    const bool Ok=WriteMemory(S);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_MEMORY saved=%d visits=%d path=%s"),Ok,VisitCount,*MemoryPath());
}
void ACourtyardController::EndPlay(const EEndPlayReason::Type Reason)
{
    SaveEmmaMemory();ClearLivelyFace();CleanupLivelyEnvironment();Super::EndPlay(Reason);
}
void ACourtyardController::TickMood(float Dt)
{
    InitializeMood();if(!MoodReady)return;
    const double Now=GetWorld()->GetTimeSeconds();
    if(PlayerWatching)LastPlayerSeen=Now;
    if(Now>=MoodShiftAt){
        MoodShiftAt=Now+FMath::FRandRange(MoodMinSeconds,MoodMaxSeconds);
        SetMood(RandomMood(Mood),TEXT("random_drift"));
    }
    if(Now>=MemorySaveAt){SaveEmmaMemory();MemorySaveAt=Now+120;}
}
// Events no longer change her mood; only recent pats are counted for the playful protest response.
void ACourtyardController::MoodEvent(FName Event)
{
    if(!MoodReady)return;const double Now=GetWorld()->GetTimeSeconds();
    if(Event==TEXT("pat")){
        MoodPatTimes.RemoveAll([Now](double T){return Now-T>=60;});MoodPatTimes.Add(Now);RecentPatCount=MoodPatTimes.Num();LastMoodPat=Now;
    }
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_MOOD event=%s mood=%s time=%.2f"),*Event.ToString(),*Mood,Now);
}
float ACourtyardController::MoodBodyGap() const
{
    return Mood==TEXT("happy")?FMath::FRandRange(25.f,45.f):Mood==TEXT("sleepy")?FMath::FRandRange(60.f,100.f):Mood==TEXT("lonely")?FMath::FRandRange(30.f,60.f):FMath::FRandRange(40.f,70.f);
}
// Review only: 0 calm, 1 happy, 2 sleepy, 3 lonely. The forced mood holds until the next review command.
void ACourtyardController::CourtyardReviewMood(int32 MoodIndex)
{
    if(!ReviewSession() || !LivelyEnabled)return;
    InitializeMood();ReviewMemoryOverride=true;
    MoodShiftAt=GetWorld()->GetTimeSeconds()+100000;
    SetMood(MoodNames[FMath::Clamp(MoodIndex,0,3)],TEXT("review_forced"));
}
// Review only: drift to a random other mood now, then resume the normal random timer.
void ACourtyardController::CourtyardReviewMoodShift()
{
    if(!ReviewSession() || !LivelyEnabled)return;
    InitializeMood();MoodShiftAt=GetWorld()->GetTimeSeconds()+FMath::FRandRange(MoodMinSeconds,MoodMaxSeconds);
    SetMood(RandomMood(Mood),TEXT("review_shift"));
}
void ACourtyardController::CourtyardReviewMemory(float Hours)
{
    if(!ReviewSession() || !LivelyEnabled)return;
    InitializeMood();ReviewMemoryOverride=true;LastVisitHours=FMath::Max(0.f,Hours);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_MEMORY simulated_hours=%.1f production_write=0"),Hours);
}
void ACourtyardController::AddMoodReview(TSharedRef<FJsonObject> J) const
{
    auto M=MakeShared<FJsonObject>();M->SetBoolField(TEXT("enabled"),LivelyEnabled);M->SetStringField(TEXT("state"),Mood);
    const double Now=GetWorld()->GetTimeSeconds();
    M->SetNumberField(TEXT("sinceChange"),Now-MoodChangedAt);M->SetNumberField(TEXT("nextShiftIn"),MoodShiftAt-Now);
    M->SetNumberField(TEXT("patCount"),RecentPatCount);M->SetNumberField(TEXT("lastVisitHours"),LastVisitHours);M->SetNumberField(TEXT("visits"),VisitCount);
    J->SetObjectField(TEXT("mood"),M);
}

void ACourtyardController::CourtyardReviewMemoryCheck()
{
    if(!ReviewSession())return;
    const FString Slot=TEXT("EmmaMemory_LivelyReview");
    if(UGameplayStatics::DoesSaveGameExist(Slot,0)){UE_LOG(LogTemp,Warning,TEXT("COURTYARD_MEMORY check skipped existing private test slot"));return;}
    auto* S=Cast<UEmmaMemorySave>(UGameplayStatics::CreateSaveGameObject(UEmmaMemorySave::StaticClass()));
    S->Affection=0;S->LastVisitUtc=FDateTime::UtcNow();S->VisitCount=3;S->LastMood=TEXT("happy");
    const bool Saved=UGameplayStatics::SaveGameToSlot(S,Slot,0);
    auto* Loaded=Cast<UEmmaMemorySave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
    const bool RoundTrip=Loaded && Loaded->Valid() && Loaded->VisitCount==3 && Loaded->LastMood==TEXT("happy");
    S->Version=0;const bool BadVersion=!S->Valid();S->Version=1;S->LastMood=TEXT("invalid");const bool BadMood=!S->Valid();
    const bool Removed=Saved && UGameplayStatics::DeleteGameInSlot(Slot,0);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_MEMORY check disk_roundtrip=%d reject_version=%d reject_mood=%d own_test_slot_removed=%d production_write=0"),RoundTrip,BadVersion,BadMood,Removed);
}
