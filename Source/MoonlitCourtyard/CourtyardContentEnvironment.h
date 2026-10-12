#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourtyardContentEnvironment.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class UAudioComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Placed only in the Content candidate. No changes to the shipped sky or old environment scheduler.
UCLASS()
class MOONLITCOURTYARD_API ACourtyardContentEnvironment : public AActor
{
 GENERATED_BODY()
public:
 ACourtyardContentEnvironment();
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Petals;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Sparks;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlameFront;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlameLeft;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlameRight;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Smoke;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MeteorShell;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> HearthLight;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> MeteorLight;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> FireAudio;
 UPROPERTY(EditAnywhere) FVector TreeCenter=FVector(-800,-2530,0);
 UPROPERTY(EditAnywhere) FVector FireCenter=FVector(350,1342,12);
 UPROPERTY(EditAnywhere) int32 ParticleBudget=600;
 UPROPERTY(EditAnywhere) bool Enabled=true;
 UPROPERTY(EditAnywhere) bool FixEnabled=false;
 UPROPERTY(EditAnywhere) bool LandedBodyGuardEnabled=false;
 UPROPERTY(EditAnywhere) bool ArmPetalGuardEnabled=false;
 UPROPERTY(VisibleAnywhere) int32 ArmAvoided=0,ArmCleared=0;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FireVolume;
 UPROPERTY(VisibleAnywhere) int32 Airborne=0,Landed=0,Avoided=0,GroundCleared=0;
 UPROPERTY(VisibleAnywhere) int32 BodyCleared=0;
 UPROPERTY(VisibleAnywhere) int32 MeteorKind=-1;
 UPROPERTY(VisibleAnywhere) float MeteorBrightness=0,CurrentWind=0;
 UPROPERTY(VisibleAnywhere) FVector MeteorDirection=FVector(0,-1,.6f);
 void TriggerMeteor(int32 Kind,bool Review=false);
 void TriggerGust(bool Review=false);
 void ReviewStationaryBodyPetals(AActor* Person,bool UseBodyGuard,bool Seed);
 void ReviewArmBodyPetals(AActor* Person,bool UseArmGuard,bool Seed);
 bool ReviewPetalsEnabled=true; // Transient observation switch, never serialized.
 bool ReviewMeteorFraming=false; // Explicit review fixture only; natural directions remain random.
private:
 struct FPetal { FVector P=FVector::ZeroVector,V=FVector::ZeroVector; FRotator R=FRotator::ZeroRotator; float Spin=0,Age=0,Life=0,Scale=1,Phase=0; uint8 State=0; };
 TArray<FPetal> Leaves;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MeteorMaterial;
 UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> FlameMaterials;
 FRandomStream Random;
 double NextDrop=0,NextGust=0,GustStart=-100,NextMeteor=0,MeteorStart=-100,NextReport=0;
 float GustDuration=6,MeteorDuration=0,FlameFlicker=1,FlameTarget=1,NextFlicker=0;
 int32 LastKind=-1,Cursor=0;
 void SpawnPetal(FPetal& P,bool Initial=false);
 void TickPetals(float Dt,double Now);
 void TickFire(float Dt,double Now);
 void TickMeteor(double Now);
};
