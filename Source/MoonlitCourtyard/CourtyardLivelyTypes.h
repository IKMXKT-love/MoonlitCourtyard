#pragma once
#include "CoreMinimal.h"
struct FLivelyFacePhase
{
    float Start=0,End=0,Fade=.35f;
    TMap<int32,float> Values; // User reference numbering, 1-based; never 74..76.
};
struct FLivelyLip
{
    int32 Shape=7;
    float Start=0,Length=.16f,Weight=.55f;
};
struct FLivelyRule
{
    FName Id;
    FString Name;
    bool Enabled=false;
    float CooldownMin=90,CooldownMax=150;
    double Due=0;
};
