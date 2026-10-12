#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourtyardContentDoor.generated.h"
class UStaticMeshComponent;class UBoxComponent;class UMaterialInstanceDynamic;
UCLASS()
class MOONLITCOURTYARD_API ACourtyardContentDoor : public AActor
{
 GENERATED_BODY()
public:
 ACourtyardContentDoor();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Leaf;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> OccupancyFlag;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> PrivateVolume;
 UPROPERTY(EditAnywhere) bool Bathroom=false;
 UPROPERTY(EditAnywhere) bool PrivacyRoom=true;
 UPROPERTY(EditAnywhere) bool FixEnabled=false;
 UPROPERTY(EditAnywhere) float ClosedYaw=270;
 UPROPERTY(EditAnywhere) bool SlidingDoor=false;
 UPROPERTY(EditAnywhere) FVector SlideOffset=FVector(-118,0,0);
 UPROPERTY(EditAnywhere) FVector SlideClosedOffset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) FVector PrivateCenter=FVector(-150,55,192);
 UPROPERTY(EditAnywhere) FVector PrivateExtent=FVector(148,145,192);
 UPROPERTY(VisibleAnywhere) bool Occupied=false;
 UPROPERTY(VisibleAnywhere) bool Open=false;
 UPROPERTY(VisibleAnywhere) float Angle=0;
 UPROPERTY(VisibleAnywhere) int32 KnockCount=0,HandleAttempts=0;
 UPROPERTY(VisibleAnywhere) double LastKnock=-100,WaitedOutside=0;
 bool RequestToggle(AActor* User);
 bool SetOccupied(bool Value,AActor* Occupant=nullptr);
 void Knock();
 bool InMotion() const {return FMath::Abs(Angle-(Open?88.f:0.f))>.1f;}
 FVector HandlePoint() const;
 FVector InteractionPoint() const;
private:
 TWeakObjectPtr<AActor> PrivateOccupant;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlagMaterial;
 float BlockedSeconds=0;
 bool SweepClear(float From,float To,AActor* Ignore=nullptr) const;
};
