#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourtyardFoliageWind.generated.h"

class UMaterialParameterCollection;

// Beauty50 (Claude, 2026-10-16): feeds a slowly gusting wind and the feet of the player and Emma to the foliage
// material collection, so garden plants sway and bend away from whoever walks through them. Placed only in the
// Beauty50 map; without it the collection keeps its defaults (calm, nobody nearby).
UCLASS()
class MOONLITCOURTYARD_API ACourtyardFoliageWind : public AActor
{
    GENERATED_BODY()
public:
    ACourtyardFoliageWind();
    virtual void Tick(float DeltaTime) override;
    UPROPERTY(EditAnywhere) TObjectPtr<UMaterialParameterCollection> Collection;
    UPROPERTY(EditAnywhere) float WindStrength = 1.f;
    UPROPERTY(EditAnywhere) FVector2D WindDirection = FVector2D(.8f, .6f);
private:
    TWeakObjectPtr<AActor> EmmaActor;
    double NextFind = 0, NextGust = 0;
    float Gust = 0, GustTarget = 0;
};
