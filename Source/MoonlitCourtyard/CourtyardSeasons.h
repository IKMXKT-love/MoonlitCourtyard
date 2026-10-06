#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourtyardSeasons.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UAudioComponent;
UCLASS()
class MOONLITCOURTYARD_API ACourtyardSeasons : public AActor {
 GENERATED_BODY()
public:
 ACourtyardSeasons();
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Petals;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlameA;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlameB;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlameC;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> HearthLight;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> FireAudio;
 UPROPERTY(EditAnywhere) FVector CanopyCenter=FVector(-800,-2530,0);
 UPROPERTY(EditAnywhere) int32 PetalCount=140;
private:
 struct FPetal {float Age,Life,X,Y,Height,Drift,Phase,Size,Spin;};
 TArray<FPetal> Falling;
 FRandomStream Random;
 float Elapsed=0,ReportAt=0;
 bool Review=false;
 void ResetPetal(FPetal& P,bool Initial);
};
