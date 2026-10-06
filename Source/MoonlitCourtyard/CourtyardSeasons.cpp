#include "CourtyardSeasons.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"

ACourtyardSeasons::ACourtyardSeasons(){
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 Petals=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FallingPetals"));Petals->SetupAttachment(RootComponent);
 Petals->SetCollisionEnabled(ECollisionEnabled::NoCollision);Petals->SetCastShadow(false);Petals->SetCullDistances(4200,5200);
 FlameA=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameA"));FlameB=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameB"));FlameC=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameC"));
 for(auto* C:{FlameA.Get(),FlameB.Get(),FlameC.Get()}){C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);}
 HearthLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("HearthLight"));HearthLight->SetupAttachment(RootComponent);HearthLight->SetLightColor(FLinearColor(1.f,.30f,.055f));HearthLight->SetIntensity(180.f);HearthLight->SetAttenuationRadius(330.f);HearthLight->SetCastShadows(true);
 FireAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("FireAudio"));FireAudio->SetupAttachment(RootComponent);FireAudio->bAutoActivate=false;FireAudio->SetVolumeMultiplier(.38f);
 Random.Initialize(20261002);
}
void ACourtyardSeasons::ResetPetal(FPetal& P,bool Initial){
 const float A=Random.FRandRange(0,2*PI),R=FMath::Sqrt(Random.FRandRange(.04f,1.f))*500;
 P.X=FMath::Cos(A)*R;P.Y=FMath::Sin(A)*R*.78f;P.Height=Random.FRandRange(420,615);P.Life=Random.FRandRange(17,25);P.Age=Initial?Random.FRand()*P.Life:0;
 P.Drift=Random.FRandRange(20,75);P.Phase=Random.FRandRange(0,2*PI);P.Size=Random.FRandRange(1.1f,1.8f);P.Spin=Random.FRandRange(12,28);
}
void ACourtyardSeasons::BeginPlay(){
 Super::BeginPlay();Review=FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"));
 for(int32 N=0;N<FMath::Clamp(PetalCount,0,220);++N){FPetal P;ResetPetal(P,true);Falling.Add(P);Petals->AddInstance(FTransform(FVector::ZeroVector));}
 int32 N=0;for(auto* C:{FlameA.Get(),FlameB.Get(),FlameC.Get()})if(auto* M=C->CreateDynamicMaterialInstance(0)){M->SetScalarParameterValue(TEXT("Phase"),N++*.31f);}
 if(FireAudio->Sound)FireAudio->FadeIn(2.f,FireAudio->VolumeMultiplier,3.f);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_SEASONS begin petals=%d sound=%s canopy=%s"),Falling.Num(),*GetNameSafe(FireAudio->Sound),*CanopyCenter.ToString());
}
void ACourtyardSeasons::Tick(float Dt){
 Super::Tick(Dt);Elapsed+=Dt;
 for(int32 N=0;N<Falling.Num();++N){
  auto& P=Falling[N];P.Age+=Dt;if(P.Age>P.Life)ResetPetal(P,false);
  const float U=P.Age/P.Life,T=P.Age;
  const FVector Pos=CanopyCenter+FVector(P.X+P.Drift*U+24*FMath::Sin(T*.75f+P.Phase),P.Y+19*FMath::Sin(T*.53f+P.Phase),FMath::Lerp(P.Height,2.f,U));
  const float Fade=FMath::Min(FMath::Clamp(T/.6f,0.f,1.f),FMath::Clamp((P.Life-T)/1.3f,0.f,1.f));
  const FRotator Rot(25*FMath::Sin(T*.8f+P.Phase),T*P.Spin+P.Phase*57,36*FMath::Cos(T*.65f+P.Phase));
  Petals->UpdateInstanceTransform(N,FTransform(Rot,Pos,FVector(P.Size*Fade)),false,N==Falling.Num()-1,true);
 }
 HearthLight->SetIntensity(180.f*(1+.065f*FMath::Sin(Elapsed*3.1f)+.035f*FMath::Sin(Elapsed*7.7f)));
 if(Review && Elapsed>ReportAt){FTransform P;Petals->GetInstanceTransform(0,P);UE_LOG(LogTemp,Display,TEXT("COURTYARD_SEASONS sample t=%.2f petal=%s audio=%d gain=%.3f light=%.2f"),Elapsed,*P.GetLocation().ToString(),FireAudio->IsPlaying(),FireAudio->VolumeMultiplier,HearthLight->Intensity);ReportAt=Elapsed+10;}
}
