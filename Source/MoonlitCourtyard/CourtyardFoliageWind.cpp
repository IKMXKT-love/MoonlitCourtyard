#include "CourtyardFoliageWind.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

ACourtyardFoliageWind::ACourtyardFoliageWind()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACourtyardFoliageWind::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Collection) return;
    const double Now = GetWorld()->GetTimeSeconds();
    // Feet position: actor location minus the capsule half height.
    auto Feet = [](const AActor* A) {
        const ACharacter* C = Cast<ACharacter>(A);
        const float Half = C && C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
        return A->GetActorLocation() - FVector(0, 0, Half);
    };
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!EmmaActor.IsValid() && Now >= NextFind)
    {
        NextFind = Now + 2;
        for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
            if (*It != Player && It->FindFunction(TEXT("CanHeadPat"))) { EmmaActor = *It; break; }
    }
    const FVector P = Player ? Feet(Player) : FVector(0, 0, -100000);
    UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Player"), FLinearColor(P.X, P.Y, P.Z, Player ? 1.f : 0.f));
    const AActor* E = EmmaActor.Get();
    const FVector EP = E ? Feet(E) : FVector(0, 0, -100000);
    UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Emma"), FLinearColor(EP.X, EP.Y, EP.Z, E ? 1.f : 0.f));
    // Soft gusts: a new target every few seconds, eased in and out.
    if (Now >= NextGust) { NextGust = Now + FMath::FRandRange(3.f, 8.f); GustTarget = FMath::FRandRange(0.f, 1.f) < .3f ? FMath::FRandRange(.6f, 1.2f) : FMath::FRandRange(0.f, .35f); }
    Gust = FMath::FInterpTo(Gust, GustTarget, Dt, .6f);
    const FVector2D D = WindDirection.GetSafeNormal();
    UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Wind"), FLinearColor(D.X, D.Y, WindStrength * (.55f + Gust), Gust));
}
