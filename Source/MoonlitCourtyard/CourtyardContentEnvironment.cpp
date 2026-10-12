#include "CourtyardContentEnvironment.h"
#include "CourtyardController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

namespace { float Ease(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);} }
ACourtyardContentEnvironment::ACourtyardContentEnvironment()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 Petals=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Petals"));Petals->SetupAttachment(RootComponent);
 Sparks=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks"));Sparks->SetupAttachment(RootComponent);
 for(auto* C:{Petals.Get(),Sparks.Get()}){C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetMobility(EComponentMobility::Movable);C->SetCanEverAffectNavigation(false);}
 Petals->SetCullDistances(4200,5200);
 FlameFront=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameFront"));
 FlameLeft=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameLeft"));
 FlameRight=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameRight"));
 Smoke=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Smoke"));
 FireVolume=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FireVolume"));
 MeteorShell=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeteorShell"));
 for(auto* C:{FlameFront.Get(),FlameLeft.Get(),FlameRight.Get(),Smoke.Get(),FireVolume.Get(),MeteorShell.Get()}){
  C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetMobility(EComponentMobility::Movable);C->SetCanEverAffectNavigation(false);
 }
 HearthLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("HearthLight"));HearthLight->SetupAttachment(RootComponent);
 HearthLight->SetIntensity(75);HearthLight->SetLightColor(FLinearColor(1,.34f,.10f));HearthLight->SetAttenuationRadius(350);HearthLight->SetCastShadows(true);HearthLight->SetMobility(EComponentMobility::Movable);
 MeteorLight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MeteorLight"));MeteorLight->SetupAttachment(RootComponent);MeteorLight->SetIntensity(0);MeteorLight->SetCastShadows(false);MeteorLight->SetMobility(EComponentMobility::Movable);
 FireAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("FireAudio"));FireAudio->SetupAttachment(RootComponent);FireAudio->bAutoActivate=false;
}
void ACourtyardContentEnvironment::BeginPlay()
{
 Super::BeginPlay();Random.Initialize(FGuid::NewGuid().A);
 if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))Random.Initialize(10723);
 if(!Enabled){SetActorTickEnabled(false);SetActorHiddenInGame(true);return;}
 Leaves.SetNum(FMath::Clamp(ParticleBudget,180,FixEnabled?1400:720));
 for(int32 N=0;N<Leaves.Num();++N){if(N<(FixEnabled?330:140))SpawnPetal(Leaves[N],true);Petals->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));}
 for(int32 N=0;N<22;++N)Sparks->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
 int32 K=0;for(auto* C:{FlameFront.Get(),FlameLeft.Get(),FlameRight.Get()})if(auto* M=C->CreateDynamicMaterialInstance(0)){M->SetScalarParameterValue(TEXT("Phase"),K++*.31f);FlameMaterials.Add(M);}
 MeteorMaterial=MeteorShell->CreateDynamicMaterialInstance(0);
 if(MeteorMaterial)MeteorMaterial->SetScalarParameterValue(TEXT("EventAge"),-100);
 HearthLight->SetRelativeLocation(FireCenter+(FixEnabled?FVector(0,-2,38):FVector(0,-17,52)));
 if(FixEnabled){for(auto* Old:{FlameFront.Get(),FlameLeft.Get(),FlameRight.Get(),Smoke.Get()})Old->SetHiddenInGame(true);HearthLight->SetIntensityUnits(ELightUnits::Lumens);HearthLight->SetAttenuationRadius(460);HearthLight->SetSourceRadius(7);HearthLight->SetLightingChannels(true,true,false);}
 FireVolume->SetHiddenInGame(!FixEnabled);
 FireAudio->SetRelativeLocation(FireCenter+FVector(0,-10,10));
 if(FireAudio->Sound)FireAudio->FadeIn(2.f,.32f,Random.FRandRange(0.f,25.f));
 const double Now=GetWorld()->GetTimeSeconds();NextMeteor=Now+Random.FRandRange(140.f,260.f);NextGust=Now+Random.FRandRange(65.f,145.f);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT environment_ready petals=%d fire=%s meteor_types=4 next=%.1f"),Leaves.Num(),*GetNameSafe(FireAudio->Sound),NextMeteor);
}
void ACourtyardContentEnvironment::SpawnPetal(FPetal& P,bool Initial)
{
 const float A=Random.FRandRange(0,2*PI),R=FMath::Sqrt(Random.FRandRange(.02f,1.f))*(FixEnabled?410:500);
 P.P=TreeCenter+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R*.88f,Initial?Random.FRandRange(15.f,580.f):Random.FRandRange(420.f,650.f));
 P.V=FVector(Random.FRandRange(-5,7),Random.FRandRange(-5,5),Random.FRandRange(-29,-18));P.Age=0;P.Life=Random.FRandRange(30,60);
 P.Scale=Random.FRandRange(.9f,1.7f);P.Phase=Random.FRandRange(0,2*PI);P.Spin=Random.FRandRange(-90,90);P.R=FRotator(Random.FRandRange(-40,40),Random.FRandRange(0,360),Random.FRandRange(-70,70));P.State=1;
 if(FixEnabled){P.Scale=Random.FRandRange(1.4f,1.9f);P.Life=Random.FRandRange(45,75);}
}
void ACourtyardContentEnvironment::TriggerGust(bool Review)
{
 const double Now=GetWorld()->GetTimeSeconds();GustStart=Now;GustDuration=Random.FRandRange(6,10);NextGust=Now+Random.FRandRange(95,190);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT gust t=%.2f seconds=%.2f review=%d"),Now,GustDuration,Review);
}
void ACourtyardContentEnvironment::TickPetals(float Dt,double Now)
{
 if(Now>=NextGust)TriggerGust();
 const float U=float(Now-GustStart)/GustDuration;
 CurrentWind=(U>0 && U<1)?FMath::Square(FMath::Sin(PI*U)):0;
 // Rate changes at gust time; allocation is bounded and never grows with running time.
 const float Gap=1.f/(FixEnabled?18.f+20.f*CurrentWind:6.f+13.f*CurrentWind);
 while(Now>=NextDrop){bool Spawned=false;for(int32 I=0;I<Leaves.Num();++I){Cursor=(Cursor+1)%Leaves.Num();if(!Leaves[Cursor].State){SpawnPetal(Leaves[Cursor]);Spawned=true;break;}}NextDrop+=Gap;if(!Spawned || Now-NextDrop>2){NextDrop=Now+Gap;break;}}
 struct FArmGuard {FVector A,B;float Radius;};TArray<FArmGuard,TInlineAllocator<4>> ArmGuards;
 ACharacter* EmmaForArms=nullptr;float PetalMeshRadius=0;
 if(ArmPetalGuardEnabled){
  if(auto* C=Cast<ACourtyardController>(UGameplayStatics::GetPlayerController(this,0)))EmmaForArms=Cast<ACharacter>(C->Emma());
  if(EmmaForArms && EmmaForArms->GetMesh()){
   auto* M=EmmaForArms->GetMesh();for(const TCHAR* Side:{TEXT("L"),TEXT("R")}){
    const FName Elbow(*(FString(TEXT("ひじ_"))+Side)),Wrist(*(FString(TEXT("手首_"))+Side)),Finger(*(FString(TEXT("中指２_"))+Side));
    if(M->DoesSocketExist(Elbow) && M->DoesSocketExist(Wrist))ArmGuards.Add({M->GetSocketLocation(Elbow),M->GetSocketLocation(Wrist),10.f});
    if(M->DoesSocketExist(Wrist) && M->DoesSocketExist(Finger))ArmGuards.Add({M->GetSocketLocation(Wrist),M->GetSocketLocation(Finger),8.f});
   }
  }
  if(UStaticMesh* Mesh=Petals->GetStaticMesh()){const FVector Ext=Mesh->GetBounds().BoxExtent;PetalMeshRadius=(Ext*FVector(1,1,.18f)).Size();}
 }
 auto ArmContains=[&](const FVector& Point,float Margin){for(const auto& G:ArmGuards){const FVector Q=FMath::ClosestPointOnSegment(Point,G.A,G.B);if(FVector::DistSquared(Point,Q)<FMath::Square(G.Radius+Margin))return true;}return false;};
 TArray<ACharacter*> People;
 if(auto* C=Cast<ACourtyardController>(UGameplayStatics::GetPlayerController(this,0))){if(auto* P=Cast<ACharacter>(C->GetPawn()))People.Add(P);if(auto* E=Cast<ACharacter>(C->Emma()))People.Add(E);}
 Airborne=Landed=0;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ContentPetals),false,this);for(auto* P:People)Q.AddIgnoredActor(P);
 for(int32 N=0;N<Leaves.Num();++N){auto& P=Leaves[N];float Scale=0;
  if(P.State){P.Age+=Dt;
   if(P.State==1){++Airborne;const FVector Before=P.P;
    const FVector Air(5+30*CurrentWind+5*FMath::Sin(Now*.35+P.Phase),3*FMath::Sin(Now*.21+P.Phase),P.V.Z);
    P.V.X=FMath::FInterpTo(P.V.X,Air.X,Dt,1.2f);P.V.Y=FMath::FInterpTo(P.V.Y,Air.Y,Dt,1.1f);
    P.P+=P.V*Dt+FVector(7*FMath::Cos(P.Age*1.8+P.Phase),6*FMath::Sin(P.Age*1.4+P.Phase),0)*Dt;
    P.R.Yaw+=P.Spin*Dt;P.R.Roll+=FMath::Sin(P.Age*2.1+P.Phase)*60*Dt;P.R.Pitch+=38*Dt;
    for(auto* Person:People){const FVector C=Person->GetActorLocation();const float Radius=Person->GetCapsuleComponent()->GetScaledCapsuleRadius()+14,Half=Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+6;
     FVector Flat=(P.P-C)*FVector(1,1,0);const float Distance=Flat.Size();if(FMath::Abs(P.P.Z-C.Z)<Half && Distance<Radius){const FVector Away=Flat.GetSafeNormal2D(SMALL_NUMBER,FVector(1,0,0));P.P=C+Away*Radius+FVector(0,0,P.P.Z-C.Z);P.V+=Away*9;++Avoided;}}
    if(ArmGuards.Num() && EmmaForArms && FVector::DistSquared(P.P,EmmaForArms->GetActorLocation())<FMath::Square(150.f)){
     const float Margin=PetalMeshRadius*P.Scale+1.f;bool Changed=false;
     for(int32 Pass=0;Pass<2;++Pass)for(const auto& G:ArmGuards){const FVector Close=FMath::ClosestPointOnSegment(P.P,G.A,G.B);const FVector Delta=P.P-Close;const float Radius=G.Radius+Margin;
      if(Delta.SizeSquared()<Radius*Radius){const FVector Normal=Delta.GetSafeNormal(SMALL_NUMBER,(P.P-EmmaForArms->GetActorLocation()).GetSafeNormal(SMALL_NUMBER,FVector(1,0,0)));P.P=Close+Normal*(Radius+.3f);P.V+=Normal*4;Changed=true;}}
     if(Changed)++ArmAvoided;
     // Intersecting envelopes are conservative. Never leave an unresolved petal parked in an arm.
     if(ArmContains(P.P,Margin)){P.State=0;++ArmCleared;}
    }
    FHitResult Hit;if(GetWorld()->LineTraceSingleByChannel(Hit,Before+FVector(0,0,2),P.P-FVector(0,0,3),ECC_WorldStatic,Q)){
     if(Hit.ImpactNormal.Z>.55f){P.P=Hit.ImpactPoint+Hit.ImpactNormal*.25f;P.R=FRotator(0,P.R.Yaw,0);P.State=2;P.Age=0;}else{P.P=Hit.ImpactPoint+Hit.ImpactNormal*4;P.V+=Hit.ImpactNormal*12;}}
    if(P.P.Z<-15 || P.Age>60)P.State=0;
    Scale=P.Scale*Ease(P.Age/.8f);
   }else{++Landed;Scale=P.Scale*(1-Ease((P.Age-P.Life+7)/7));if(P.Age>=P.Life)P.State=0;}
  }
  // Landing is not necessarily at floor height: seats and other raised supports
  // also catch petals. A person can approach after landing, so the opt-in guard
  // covers the same bounded body volume as airborne avoidance, not only feet.
  // This remains a capsule approximation, not a full skin or cloth solver.
  if(FixEnabled && P.State==2)for(auto* Person:People){
   const FVector C=Person->GetActorLocation();
   const float Radius=Person->GetCapsuleComponent()->GetScaledCapsuleRadius()+14;
   const float Bottom=C.Z-Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   const bool AtFoot=FMath::Abs(P.P.Z-Bottom)<8;
   const bool InBody=LandedBodyGuardEnabled && FMath::Abs(P.P.Z-C.Z)<Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+6;
   if(FVector::DistSquared2D(P.P,C)<Radius*Radius && (AtFoot || InBody)){P.State=0;++GroundCleared;if(InBody && !AtFoot)++BodyCleared;break;}
  }
  if(P.State==2 && ArmGuards.Num() && EmmaForArms && FVector::DistSquared(P.P,EmmaForArms->GetActorLocation())<FMath::Square(150.f) && ArmContains(P.P,PetalMeshRadius*P.Scale+1.f)){P.State=0;++ArmCleared;}
  if(!P.State)Scale=0;
  const FVector DrawScale=FixEnabled?FVector(Scale,Scale,Scale*.18f):FVector(Scale);
  Petals->UpdateInstanceTransform(N,FTransform(P.R,P.P,DrawScale),true,false,true);
 }
 Petals->MarkRenderStateDirty();
}
void ACourtyardContentEnvironment::ReviewStationaryBodyPetals(AActor* Person,bool UseBodyGuard,bool Seed)
{
 if(!Person || !FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 LandedBodyGuardEnabled=UseBodyGuard;
 if(Seed && Leaves.Num()>=4){
  const FVector C=Person->GetActorLocation();
  const FVector Offsets[]={FVector(0,0,-80),FVector(0,0,22),FVector(0,0,55),FVector(90,0,22)};
  for(int32 K=0;K<4;++K){auto& P=Leaves[K];P.P=C+Offsets[K];P.State=2;P.Age=0;P.Life=75;P.Scale=1.6f;P.R=FRotator(0,0,0);}
 }
 FString States;for(int32 K=0;K<4 && K<Leaves.Num();++K)States+=FString::Printf(TEXT("%d:%d;"),K,Leaves[K].State);
 UE_LOG(LogTemp,Display,TEXT("CONTENT32 stationary_body_probe body_guard=%d seeded=%d states=%s body_cleared=%d ground_cleared=%d"),LandedBodyGuardEnabled,Seed,*States,BodyCleared,GroundCleared);
}
void ACourtyardContentEnvironment::ReviewArmBodyPetals(AActor* Person,bool UseArmGuard,bool Seed)
{
 if(!Person || !FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;ArmPetalGuardEnabled=UseArmGuard;
 if(Seed && Leaves.Num()>=3){
  auto* A=Cast<ACharacter>(Person);auto* M=A?A->GetMesh():nullptr;auto* RD=M?M->GetSkeletalMeshRenderData():nullptr;if(!RD || !RD->LODRenderData.Num())return;
  TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);FVector Point=FVector::ZeroVector;float Best=-1e9f;
  const FVector Root=A->GetActorLocation();const float Radius=A->GetCapsuleComponent()->GetScaledCapsuleRadius()+14;
  for(const auto& S:RD->LODRenderData[0].RenderSections)if(S.MaterialIndex==2)for(uint32 K=0;K<S.NumVertices;++K){const uint32 N=S.BaseVertexIndex+K;if(!V.IsValidIndex(N))continue;const FVector W=M->GetComponentTransform().TransformPosition(FVector(V[N].Position));const float Score=FVector::Dist2D(W,Root)-Radius;if(Score>Best){Best=Score;Point=W;}}
  if(Best<.5f){UE_LOG(LogTemp,Warning,TEXT("CONTENT41 arm_petal_seed no_actual_outside_hand=1 max_margin=%.3f"),Best);return;}
  for(int32 K=0;K<3;++K){auto& P=Leaves[K];P.P=Point+(K==1?FVector(90,0,0):FVector::ZeroVector);P.State=K==2?1:2;P.V=FVector(0,0,-20);P.Age=K==2?1:0;P.Life=75;P.Scale=1.6f;P.Phase=0;P.Spin=0;P.R=FRotator(0,0,0);}
  UE_LOG(LogTemp,Display,TEXT("CONTENT41 arm_petal_seed actual_hand_skin=%s outside_old_guard_cm=%.3f control_offset90=1 no_avatar_writes=1"),*Point.ToString(),Best);
 }
 FString States;for(int32 K=0;K<3 && K<Leaves.Num();++K)States+=FString::Printf(TEXT("%d:%d@%s;"),K,Leaves[K].State,*Leaves[K].P.ToString());
 UE_LOG(LogTemp,Display,TEXT("CONTENT41 arm_petal_probe arm_guard=%d seeded=%d states=%s arm_avoided=%d arm_cleared=%d"),UseArmGuard,Seed,*States,ArmAvoided,ArmCleared);
}
void ACourtyardContentEnvironment::TickFire(float Dt,double Now)
{
 if(Now>=NextFlicker){NextFlicker=Now+Random.FRandRange(.10f,.26f);FlameTarget=Random.FRandRange(.87f,1.12f);}
 FlameFlicker=FMath::FInterpTo(FlameFlicker,FlameTarget,Dt,5.f);HearthLight->SetIntensity((FixEnabled?32:240)*FlameFlicker);
 if(auto* C=Cast<ACourtyardController>(UGameplayStatics::GetPlayerController(this,0)))FireAudio->SetVolumeMultiplier(.32f*C->Applied.Master*C->Applied.Environment);
 for(int32 N=0;N<22;++N){const float T=FMath::Fmod(float(Now)*.31f+N*.618f,1.f);const float Width=18*(1-T);const float S=Ease(T/.1f)*(1-Ease((T-.75f)/.25f));
  const FVector P=FireCenter+FVector(FMath::Sin(N*17.3f+T*3)*Width,FMath::Cos(N*6.8f)*6,T*64+6);
  Sparks->UpdateInstanceTransform(N,FTransform(FRotator(0,0,0),P,FVector(.002f,.002f,.006f)*S),true,false,true);
 }Sparks->MarkRenderStateDirty();
}
void ACourtyardContentEnvironment::TriggerMeteor(int32 Kind,bool Review)
{
 const double Now=GetWorld()->GetTimeSeconds();if(Now-MeteorStart<MeteorDuration+2)return;
 if(Kind<0){const float P=Random.FRand();Kind=P<.56f?0:P<.85f?1:P<.96f?2:3;if(Kind==LastKind && Kind<2)Kind=1-Kind;}
 MeteorKind=FMath::Clamp(Kind,0,3);LastKind=MeteorKind;MeteorStart=Now;MeteorDuration=MeteorKind==0?3.2f:MeteorKind==1?6.f:MeteorKind==2?5.5f:9.f;
 float Az=Random.FRandRange(-165.f,165.f),El=Random.FRandRange(25.f,61.f);
 if(Review && ReviewMeteorFraming && FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"))){Az=-112.f+MeteorKind*16.f;El=42.f;}
 MeteorDirection=FRotator(El,Az,0).Vector();
 float SkySpeed=Random.FRandRange(.08f,.13f);
 if(Review && ReviewMeteorFraming && FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))SkySpeed=.1f;
 if(MeteorMaterial){MeteorMaterial->SetScalarParameterValue(TEXT("EventKind"),MeteorKind);MeteorMaterial->SetScalarParameterValue(TEXT("EventDuration"),MeteorDuration);MeteorMaterial->SetVectorParameterValue(TEXT("EventOrigin"),FLinearColor(FMath::DegreesToRadians(Az),FMath::DegreesToRadians(El),SkySpeed,0));}
 MeteorLight->SetWorldRotation((-MeteorDirection).Rotation());MeteorLight->SetLightColor(MeteorKind==2?FLinearColor(.76f,.84f,1.f):FLinearColor(.65f,.73f,1.f));
 NextMeteor=Now+Random.FRandRange(180.f,350.f)+(MeteorKind>1?180:0);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT meteor kind=%d t=%.2f duration=%.2f direction=%s next=%.2f review=%d"),MeteorKind,Now,MeteorDuration,*MeteorDirection.ToString(),NextMeteor,Review);
}
void ACourtyardContentEnvironment::TickMeteor(double Now)
{
 if(Now>=NextMeteor)TriggerMeteor(-1);
 const float Age=Now-MeteorStart;
 MeteorBrightness=Age>=0 && Age<MeteorDuration?Ease(Age/.45f)*(1-Ease((Age-MeteorDuration+1.4f)/1.4f)):0;
 const float Burst=MeteorKind==2?FMath::Exp(-FMath::Square((Age-2.2f)/.55f)):MeteorKind==3?FMath::Exp(-FMath::Square((Age-2.9f)/1.1f))*.5f:0;
 MeteorLight->SetIntensity(.09f*Burst);
 if(MeteorMaterial)MeteorMaterial->SetScalarParameterValue(TEXT("EventAge"),Age);
}
void ACourtyardContentEnvironment::Tick(float Dt)
{
 Super::Tick(Dt);if(!Enabled || Dt<=0)return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(ReviewPetalsEnabled)TickPetals(FMath::Min(Dt,.1f),Now);
 TickFire(Dt,Now);TickMeteor(Now);
 if(Now>=NextReport){NextReport=Now+15;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT env t=%.2f airborne=%d landed=%d avoided=%d ground_cleared=%d fire_audio=%d fire_gain=%.3f meteor=%d brightness=%.3f"),Now,Airborne,Landed,Avoided,GroundCleared,FireAudio->IsPlaying(),FireAudio->VolumeMultiplier,MeteorKind,MeteorBrightness);}
}
