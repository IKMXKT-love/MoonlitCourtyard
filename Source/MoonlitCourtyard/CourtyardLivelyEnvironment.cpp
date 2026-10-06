#include "CourtyardController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Sound/SoundBase.h"
#include "Components/PointLightComponent.h"

namespace {
float EnvEase(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*U*(10+U*(-15+6*U));}
FVector PalmCenter(USkeletalMeshComponent* M,const TCHAR* S){return M->GetSocketLocation(*FString::Printf(TEXT("手首_%s"),S))*.4f+(M->GetSocketLocation(*FString::Printf(TEXT("人指１_%s"),S))+M->GetSocketLocation(*FString::Printf(TEXT("小指１_%s"),S)))*.3f;}
}
void ACourtyardController::InitializeLivelyEnvironment()
{
 if(LivelyStage<3 || P3Environment || !LivingInitialized || !P3Sphere || !P3GlowMaterial || !Emma())return;
 P3Environment=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(P3Environment);P3Environment->SetRootComponent(Root);Root->RegisterComponent();
 auto Instances=[&](const TCHAR* Name,UStaticMesh* Mesh){auto* C=NewObject<UInstancedStaticMeshComponent>(P3Environment,Name);C->SetupAttachment(Root);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetMobility(EComponentMobility::Movable);C->SetCullDistances(2200,2800);C->RegisterComponent();return C;};
 P3Fireflies=Instances(TEXT("LivelyFireflies"),P3Sphere);P3Glow=P3Fireflies->CreateDynamicMaterialInstance(0,P3GlowMaterial);P3Glow->SetScalarParameterValue(TEXT("Life"),0);
 P3Petals=Instances(TEXT("LivelyGustPetals"),LivingMeshes[0]);
 if(P3PetalMaterial)P3Petals->SetMaterial(0,P3PetalMaterial);
 P3Moths=Instances(TEXT("LivelyMothWings"),P3Sphere);P3Moths->SetMaterial(0,P3MothMaterial);
 P3Hat=NewObject<UStaticMeshComponent>(P3Environment,TEXT("LivelyHatPetal"));P3Hat->SetupAttachment(Root);P3Hat->SetStaticMesh(LivingMeshes[0]);P3Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);P3Hat->SetCastShadow(false);P3Hat->SetMobility(EComponentMobility::Movable);P3Hat->RegisterComponent();P3Hat->SetVisibility(false);
 // Choose actual flower clusters, rather than a uniform world-space scatter.
 const FVector Wants[]={FVector(-120,-400,70),FVector(320,-1450,70),FVector(-500,-2230,70)};
 for(const FVector& Want:Wants){float Best=700;FVector Center=Want;
  for(auto Weak:FlowerComponents)if(auto* C=Weak.Get()){if(C->IsVisible()){const float Dist=FVector::Dist2D(C->Bounds.Origin,Want);if(Dist<Best){Best=Dist;Center=C->Bounds.Origin+FVector(0,0,35);}}}
  P3FlyCenters.Add(Center);
 }
 for(int32 N=0;N<30;++N){P3Fireflies->AddInstance(FTransform(FVector::ZeroVector));P3FlySeeds.Add(FVector(FMath::FRandRange(0.f,6.28f),FMath::FRandRange(35.f,115.f),FMath::FRandRange(.12f,.28f)));}
 for(int32 N=0;N<12;++N)P3Petals->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
 // Existing lanterns are currently indoors. If no outdoor fixture exists,
 // reuse one design on two verified exterior wall hits; never move existing decor.
 UStaticMeshComponent* LanternTemplate=nullptr;
 // Only real lantern meshes with an unobstructed outdoor vertical ray qualify.
 for(TActorIterator<AActor> It(GetWorld());It;++It){if(*It==P3Environment)continue;TInlineComponentArray<UStaticMeshComponent*> Cs;It->GetComponents(Cs);
  for(auto* C:Cs)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName().Contains(TEXT("Lantern")) && C->IsVisible()){
   if(!LanternTemplate)LanternTemplate=C;
   const FVector Pos=C->Bounds.Origin;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(LivelyLamp),false,*It);
   if(Pos.Z<450 && !GetWorld()->LineTraceSingleByChannel(H,Pos+FVector(0,0,35),Pos+FVector(0,0,3000),ECC_Visibility,Q))P3LampCenters.Add(Pos);
  }
 }
 if(!P3LampCenters.Num() && LanternTemplate){for(float X:{-330.f,330.f}){
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(LivelyExteriorLamp),false,P3Environment);
  if(GetWorld()->LineTraceSingleByChannel(H,FVector(X,0,210),FVector(X,600,210),ECC_Visibility,Q) && H.ImpactNormal.Y<-.8){
   auto* Lamp=NewObject<UStaticMeshComponent>(P3Environment);Lamp->SetupAttachment(Root);Lamp->SetStaticMesh(LanternTemplate->GetStaticMesh());
   for(int32 I=0;I<LanternTemplate->GetNumMaterials();++I)Lamp->SetMaterial(I,LanternTemplate->GetMaterial(I));
   Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);Lamp->SetCastShadow(false);Lamp->SetMobility(EComponentMobility::Movable);Lamp->RegisterComponent();
   const FVector Pos=H.ImpactPoint+H.ImpactNormal*14;Lamp->SetWorldLocationAndRotation(Pos,FRotator(0,180,0));Lamp->SetWorldScale3D(LanternTemplate->GetComponentScale());
   auto* Light=NewObject<UPointLightComponent>(P3Environment);Light->SetupAttachment(Root);Light->SetMobility(EComponentMobility::Movable);Light->SetIntensity(22);Light->SetLightColor(FLinearColor(1,.70f,.40f));Light->SetAttenuationRadius(240);Light->SetCastShadows(false);Light->RegisterComponent();Light->SetWorldLocation(Pos+FVector(0,-12,15));
   P3LampCenters.Add(Pos+FVector(0,-18,15));UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV exterior_lamp wall=%s pos=%s"),*GetNameSafe(H.GetActor()),*Pos.ToString());
  }
 }}
 for(int32 N=0;N<P3LampCenters.Num()*6;++N)P3Moths->AddInstance(FTransform(FVector::ZeroVector));
 for(TActorIterator<AActor> It(GetWorld());It;++It){TInlineComponentArray<UAudioComponent*> Cs;It->GetComponents(Cs);for(auto* C:Cs)if(C->Sound && C->Sound->GetPathName().Contains(TEXT("Wind_Experience"))){P3WindAudio=C;if(!EnvironmentBase.Contains(C))EnvironmentBase.Add(C,C->VolumeMultiplier);}}
 P3NextGust=GetWorld()->GetTimeSeconds()+FMath::FRandRange(180.f,420.f);P3NextFly=80;P3NextAmbient=60;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV initialized fireflies=30 gust_petals=12 exterior_lamps=%d moths=%d wind=%s old_seasons=not_spawned"),P3LampCenters.Num(),P3LampCenters.Num()*3,*GetNameSafe(P3WindAudio.Get()));
}
void ACourtyardController::StartLivelyGust(bool Review)
{
 if(!P3Environment)return;const double Now=GetWorld()->GetTimeSeconds();P3GustStart=Now;P3GustLength=FMath::FRandRange(4.f,6.f);P3GustPeakSent=false;P3ChimeSent=false;
 TArray<int32> Choices;if(LivelyReady(TEXT("C13")))Choices.Add(0);if(LivelyReady(TEXT("C17")))Choices.Add(2);
 if(LivelyReady(TEXT("C14")) && LivingChime.IsValid() && Emma() && FVector::Dist(Emma()->GetActorLocation(),LivingChime->GetActorLocation())<1000 && (Mood==TEXT("happy") || Mood==TEXT("calm")))Choices.Add(1);
 P3GustChoice=Choices.Num()?Choices[FMath::RandHelper(Choices.Num())]:-1;
 P3GustReplyUntil=P3GustChoice==0?Now+2:-1;P3GustHairPending=false;P3NextGust=Now+FMath::FRandRange(180.f,420.f);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV gust time=%.3f duration=%.2f next=%.2f review=%d"),Now,P3GustLength,P3NextGust,Review);
}
void ACourtyardController::DropHatPetal(bool FromPat)
{
 if(!P3Hat || !P3HatAttached)return;P3Hat->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);P3HatOrigin=P3Hat->GetComponentLocation();P3HatAttached=false;P3HatFallStart=GetWorld()->GetTimeSeconds();
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV hat_drop E=%d time=%.3f pos=%s"),FromPat,P3HatFallStart,*P3HatOrigin.ToString());
}
void ACourtyardController::TickLivelyEnvironment(float Dt)
{
 InitializeLivelyEnvironment();if(!P3Environment)return;const double Now=GetWorld()->GetTimeSeconds();auto* C=Cast<ACharacter>(Emma());auto* Mesh=C?C->GetMesh():nullptr;
 if(Now>=P3NextGust)StartLivelyGust(false);
 const float Age=Now-P3GustStart,U=Age/P3GustLength;const float Gust=Age>=0 && U<1?FMath::Square(FMath::Sin(PI*U)):0;
 P3CurrentGust=Gust;
 if(auto* Audio=P3WindAudio.Get())Audio->SetVolumeMultiplier(EnvironmentBase.FindRef(Audio)*Applied.Master*Applied.Environment*(1+.4f*Gust));
 if(Gust>.65f && !P3GustPeakSent){P3GustPeakSent=true;P3SuddenUntil=P3GustChoice==2?Now+1:-1;
  if(!P3HatAttached && C && ((LivingTree.IsValid() && FVector::Dist2D(C->GetActorLocation(),LivingTree->GetActorLocation())<480) || (AncientTree.IsValid() && FVector::Dist2D(C->GetActorLocation(),AncientTree->GetActorLocation())<600)) && FMath::FRand()<.3f)P3HatDue=Now+1;
 }
 if(Gust>0 && LivingChime.IsValid() && LivingBehavior!=1){
  FRotator R=ChimeRest.Rotator();R.Roll+=FMath::Sin(Age*5)*3*Gust;LivingChime->SetActorRotation(R);
  if(Gust>.4f && !P3ChimeSent){P3ChimeSent=true;P3ChimeUntil=P3GustChoice==1?Now+2:-1;const int32 S=FMath::RandHelper(ChimeSounds.Num());
   if(ChimeSounds.IsValidIndex(S) && ChimeSounds[S])UGameplayStatics::PlaySoundAtLocation(this,ChimeSounds[S],LivingChime->GetActorLocation(),.32f*Applied.Master*Applied.Environment,1,0,ChimeAttenuation);
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV chime time=%.3f gain=%.3f"),Now,.32f*Applied.Master*Applied.Environment);
  }
 }else if(P3ChimeSent && LivingBehavior!=1 && LivingChime.IsValid() && Age>=P3GustLength){LivingChime->SetActorTransform(ChimeRest);P3ChimeSent=false;}
 for(int32 N=0;N<12;++N){const float A=Age+N*.19f;const bool Live=Age>=0 && Age<P3GustLength+4;
  FVector Center=LivingTree.IsValid()?LivingTree->GetActorLocation():FVector::ZeroVector;
  FVector Pos=Center+FVector(-180+N*32+A*(10+30*Gust),50*FMath::Sin(N*2.4f+A*.6f),290+N*6-A*25);
  const float Fade=Live?EnvEase(A/.6f)*(1-EnvEase((Age-P3GustLength-2)/2)):0;
  P3Petals->UpdateInstanceTransform(N,FTransform(FRotator(20*FMath::Sin(A),A*18+N*27,12),Pos,FVector(.9f*Fade)),true,N==11,true);
 }
 if(P3Glow)P3Glow->SetScalarParameterValue(TEXT("Life"),EnvEase((Now-60)/8));
 for(int32 N=0;N<30;++N){const FVector Seed=P3FlySeeds[N];const float T=Now*Seed.Z+Seed.X;FVector Pos=P3FlyCenters[N%P3FlyCenters.Num()]+FVector(FMath::Cos(T)*Seed.Y,FMath::Sin(T*.73f)*Seed.Y*.65f,18*FMath::Sin(T*.55f));
  if(N==0){

   if(Now<P3FlyVisitUntil && Mesh){
   FVector E,F;FaceFrame(E,F);FVector Goal=E+F*85+FVector(0,0,-10);
   if(LivelyBodyId==TEXT("C11")){
    const float MotionAge=Now-P3MotionStart;const float W=EnvEase((MotionAge-.2f)/2)*(1-EnvEase((MotionAge-5)/2));
    Goal=FMath::Lerp(E+F*85+FVector(0,0,20), (PalmCenter(Mesh,TEXT("L"))+PalmCenter(Mesh,TEXT("R")))*.5f+FVector(0,0,5),W);
   }
   Pos=Goal;
   }
   if(P3GuidedFly.IsZero())P3GuidedFly=Pos;
   // Continuous bounded travel also covers cancellation and rejoining the moving group.
   P3GuidedFly=FMath::VInterpConstantTo(P3GuidedFly,Pos,Dt,65.f);Pos=P3GuidedFly;
  }
  P3Fireflies->UpdateInstanceTransform(N,FTransform(FQuat::Identity,Pos,FVector(.0075f+.0015f*FMath::Sin(T*.5f))),true,N==29,true);
 }
 for(int32 N=0;N<P3LampCenters.Num()*3;++N){const float T=Now*.45f+N*2.1f;const FVector Pos=P3LampCenters[N/3]+FVector(25*FMath::Cos(T),22*FMath::Sin(T),20+12*FMath::Sin(T*.63f));
  for(int32 Side=0;Side<2;++Side){const int32 I=N*2+Side;const float Sign=Side?1:-1;P3Moths->UpdateInstanceTransform(I,FTransform(FRotator(0,T*57,Sign*(15+25*FMath::Sin(Now*12))),Pos+FVector(Sign*.45f,0,0),FVector(.010f,.017f,.0015f)),true,I==P3Moths->GetInstanceCount()-1,true);}
 }
 if(P3HatDue>0 && Now>P3HatDue+2)P3HatDue=-1;
 const bool UnderTree=C && ((LivingTree.IsValid() && FVector::Dist2D(C->GetActorLocation(),LivingTree->GetActorLocation())<480) || (AncientTree.IsValid() && FVector::Dist2D(C->GetActorLocation(),AncientTree->GetActorLocation())<600));
 if(Now>=P3HatDue && P3HatDue>0 && UnderTree && Mesh && LivelyReady(TEXT("C8")) && !P3HatAttached && !PendingLivelyPat && !LivelyResponseActive){
  P3HatDue=-1;FVector E,F;FaceFrame(E,F);const FVector Right=FVector::CrossProduct(FVector::UpVector,F);
  // This rig's local head Z points toward the face, not up. Place in the standing
  // world frame first, then retain the measured socket-relative transform.
  P3Hat->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  P3Hat->SetWorldLocationAndRotation(Mesh->GetSocketLocation(TEXT("頭"))+FVector(0,0,25)-F*1-Right*6,FRotator(5,25,8));P3Hat->SetWorldScale3D(FVector(.8f));
  P3Hat->AttachToComponent(Mesh,FAttachmentTransformRules::KeepWorldTransform,TEXT("頭"));P3Hat->SetVisibility(true);P3HatAttached=true;P3HatNoticeAt=Now+FMath::FRandRange(3.f,6.f);UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV hat_attached time=%.3f pos=%s"),Now,*P3Hat->GetComponentLocation().ToString());
 }
 if(P3HatAttached && LivelyBodyId==TEXT("C8") && Now-P3MotionStart>2.7)DropHatPetal(false);
 if(P3Hat && !P3HatAttached && P3HatFallStart>0){const float T=Now-P3HatFallStart;P3Hat->SetWorldLocation(P3HatOrigin+FVector(9*T,4*FMath::Sin(T*1.3),4*T-9*T*T));P3Hat->SetVisibility(T<5);if(T>=5)P3HatFallStart=-1;}
 if(Now>P3NextFly && Now>70 && Mesh && !LivingTrip && (Mood==TEXT("calm") || Mood==TEXT("happy")) && LivelyReady(TEXT("C11"))){
  FVector E,F;FaceFrame(E,F);FTransform FT;P3Fireflies->GetInstanceTransform(0,FT,true);const FVector Target=E+F*85;
  if(VisibleTarget(Target) && LivelySkyOpen(12)){
   P3FlyVisitStart=Now;P3FlyVisitUntil=Now+13;P3NextFly=Now+FMath::FRandRange(360.f,600.f);
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV firefly_visit time=%.3f"),Now);
  }else P3NextFly=Now+45;
 }
}
void ACourtyardController::CourtyardReviewEnvironment(int32 Event)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;InitializeLivelyEnvironment();if(!P3Environment)return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Event==0)StartLivelyGust(true);
 if(Event==1){P3NextFly=Now;}
 if(Event==2){P3HatDue=Now;}
 if(Event==9){P3NextAmbient=Now;}
 if(Event==3 || Event==4){ReviewPetalOutcome=Event-3;StartLiving(0,true);}
 if(Event==8){FVector E,F;if(FaceFrame(E,F)){
  for(const FVector& P:P3LampCenters){const FVector D=P+FVector(0,0,20)-E;UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV lamp_context dist=%.2f yaw=%.2f pitch=%.2f visible=%d eyes=%s"),D.Size(),FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(F,D.GetSafeNormal2D()))),FMath::RadiansToDegrees(FMath::Atan2(D.Z,D.Size2D())),VisibleTarget(P),*E.ToString());}
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV flower_near=%d moon_aligned=%d"),CloseFlowerContext(),MoonPinchContext());
 }}
 if((Event==5 || Event==6) && ContextSky){
  FVector E,F;if(FaceFrame(E,F)){
   const uint32 Seed=uint32(ContextSky->K2_GetScalarParameterValue(TEXT("SessionSeed")));
   auto Hash=[](uint32 X,uint32 Y,uint32 Z){uint32 N=X*1597334677u ^ Y*3812015801u ^ Z*2798796415u;N^=N>>16;N*=2246822519u;N^=N>>13;return float(N&16777215u)/16777216.f;};
   for(int32 Epoch=100;Epoch<1000;++Epoch){
    const float S=Hash(Epoch,249,Seed),Dir=Hash(Epoch,49,Seed+37);const int32 Kind=FMath::FloorToInt(Hash(Epoch,8,Seed+21)*4);
    if((Kind==3)!=(Event==6) || Epoch==LastMeteorEpoch)continue;
    const float Az=(Dir<.76f?FMath::Lerp(-2.7f,-.3f,Dir/.76f):FMath::Lerp(.15f,2.95f,(Dir-.76f)/.24f))+(.12f+S*.045f)*1.15f;
    const float El=.43f+Hash(Epoch,88,Seed+3)*.40f+(-.055f-S*.018f)*1.15f;
    const FVector D(FMath::Cos(El)*FMath::Cos(Az),FMath::Cos(El)*FMath::Sin(Az),FMath::Sin(El));
    if(El<.30f || El>.62f || FVector::DotProduct(F,D.GetSafeNormal2D())<.98f || !VisibleTarget(E+D*5000))continue;
    ContextSky->SetScalarParameterValue(TEXT("ReviewTimeOffset"),Epoch*68.f+12+S*30-2-Now);ReviewWishOnce=Event==5;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV sky_fixture epoch=%d kind=%d due=%.3f wish_roll_override=%d geometry_unchanged=1"),Epoch,Kind,Now+2,ReviewWishOnce);break;
   }
  }
 }
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENV review event=%d time=%.3f"),Event,Now);
}

void ACourtyardController::CleanupLivelyEnvironment(){
 if(auto* Audio=P3WindAudio.Get())Audio->SetVolumeMultiplier(EnvironmentBase.FindRef(Audio)*Applied.Master*Applied.Environment);
 if(LivingChime.IsValid() && P3ChimeSent)LivingChime->SetActorTransform(ChimeRest);
 if(P3Environment){P3Environment->Destroy();P3Environment=nullptr;}
}
