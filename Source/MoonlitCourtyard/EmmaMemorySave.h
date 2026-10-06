#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "EmmaMemorySave.generated.h"

UCLASS()
class MOONLITCOURTYARD_API UEmmaMemorySave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version=1;
    UPROPERTY() float Affection=0;
    UPROPERTY() FDateTime LastVisitUtc;
    UPROPERTY() int32 VisitCount=0;
    UPROPERTY() FString LastMood=TEXT("calm");
    bool Valid() const;
};
