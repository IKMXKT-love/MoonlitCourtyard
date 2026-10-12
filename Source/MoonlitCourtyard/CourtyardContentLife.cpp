#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentEnvironment.h"
#include "CourtyardContentDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
namespace ContentLife {
void Flag(UObject* O,const TCHAR* Name,bool Value){if(O)if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),Name))P->SetPropertyValue_InContainer(O,Value);}
FLivelyFacePhase Face(float A,float B,TMap<int32,float> Values,float Fade=.6f){FLivelyFacePhase P;P.Start=A;P.End=B;P.Fade=Fade;P.Values=MoveTemp(Values);return P;}
}
ACourtyardContentLife::ACourtyardContentLife()
{
 PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 Gift=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GiftFlower"));Gift->SetupAttachment(RootComponent);Gift->SetCollisionEnabled(ECollisionEnabled::NoCollision);Gift->SetCastShadow(false);Gift->SetMobility(EComponentMobility::Movable);Gift->SetVisibility(false);
 WornFlower=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WornFlower"));WornFlower->SetupAttachment(RootComponent);WornFlower->SetCollisionEnabled(ECollisionEnabled::NoCollision);WornFlower->SetCastShadow(false);WornFlower->SetMobility(EComponentMobility::Movable);WornFlower->SetVisibility(false);
}
void ACourtyardContentLife::BeginPlay()
{
 Super::BeginPlay();if(!Enabled){SetActorTickEnabled(false);return;}
 C=Cast<ACourtyardController>(UGameplayStatics::GetPlayerController(this,0));if(!C.IsValid()){SetActorTickEnabled(false);return;}
 C->ContentLayer=this;C->AddTickPrerequisiteActor(this);Random.Initialize(FGuid::NewGuid().A);for(TActorIterator<ACourtyardContentEnvironment> I(GetWorld());I;++I){Environment=*I;break;}
 CaptureOnly=FParse::Param(FCommandLine::Get(),TEXT("ContentNaturalCapture"));
 if(CaptureOnly){C->ReviewMemoryOverride=true;C->SetIgnoreLookInput(true);C->SetIgnoreMoveInput(true);if(auto* V=GetWorld()->GetGameViewport())V->SetIgnoreInput(true);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT natural_capture memory_writes_disabled=1 behavior_timers_unchanged=1 stationary_input=1 viewport_input_ignored=1"));}
 NextDecision=GetWorld()->GetTimeSeconds()+Random.FRandRange(75,135);LastNear=GetWorld()->GetTimeSeconds();
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT life_ready stairs=%d seating=%d contact=%d old_mood_unchanged=1"),StairsValidated,SeatingValidated,ContactValidated);
}
void ACourtyardContentLife::EndPlay(const EEndPlayReason::Type Reason){FinishFlowerScent(TEXT("world_end"));FinishTerracePalm(TEXT("world_end"));ReleaseTravel();if(FixEnabled)ClearFixedEmotion();if(C.IsValid()){C->RemoveTickPrerequisiteActor(this);C->ContentLayer.Reset();}Super::EndPlay(Reason);}
bool ACourtyardContentLife::Safe(bool Body) const
{
 if(!C.IsValid() || C->Page!=ECourtyardPage::Game || !C->WelcomeDone || !C->Emma())return false;
 auto* E=Cast<ACharacter>(C->Emma());auto* I=E?E->GetMesh()->GetAnimInstance():nullptr;
 return I && !I->IsAnyMontagePlaying() && !C->ActiveAmbient && C->LivingBehavior<0 && !C->LivingTrip && !C->BPBool(E,TEXT("TravelActive")) && C->BPBool(E,TEXT("AllowInteraction")) && !C->PendingLivelyPat && C->LocoState==ACourtyardController::ELocoState::Idle && (!Body || GetWorld()->GetTimeSeconds()-C->AmbientStillSince>=5);
}
bool ACourtyardContentLife::Play(FName Name,FName Task)
{
 // The new deliberate hearth pause and the earlier content-layer warmth share a cooldown.
 // Older maps leave HearthPauseEnabled false and retain their original decisions.
 if(HearthPauseEnabled && (Task==TEXT("HearthWarm") || Task==TEXT("WarmHands")) && Cooldowns.FindRef(TEXT("HearthWarmPause"))>GetWorld()->GetTimeSeconds())return false;
 if(WindowPauseEnabled && Task==TEXT("WindowReflect") && Cooldowns.FindRef(TEXT("WindowPause"))>GetWorld()->GetTimeSeconds())return false;
 if(BlocksOrdinaryBehavior() && !Task.ToString().StartsWith(TEXT("Emotion")) && !(LeadScenesEnabled && LeadPhase>0 && Task.ToString().StartsWith(TEXT("Lead"))))return false;
 if(!Safe(!(BlocksOrdinaryBehavior() && Task.ToString().StartsWith(TEXT("Emotion")))))return false;if(Name==TEXT("RearNotice") && C->Mood==TEXT("happy"))Name=TEXT("RearNoticeHappy");auto* P=Motions.Find(Name);if(!P || !*P){UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX missing_motion name=%s"),*Name.ToString());return false;}
 const bool SeatedSkyEntry=SeatedSkyScenesEnabled && C->Pose==ECourtyardPose::Seated && (Task==TEXT("SkyTogether") || Task==TEXT("SharedSkyMeteor"));
 if(!C->PlayCompanion(P->Get(),SeatedSkyEntry?736:723,SeatedSkyEntry?TEXT("seated_sky_scene"):TEXT("content23")))return false;
 ActiveTask=Task;TaskSince=GetWorld()->GetTimeSeconds();TaskUntil=TaskSince+(*P)->GetPlayLength()+.5;
 C->LivelyStarted(FName(*(TEXT("N_")+Task.ToString())),TEXT("content_context"),true);
 // New copies own their authored face curves; the old face mixer remains untouched.
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT action=%s motion=%s"),*Task.ToString(),*(*P)->GetName());return true;
}
bool ACourtyardContentLife::Travel(const FVector& Goal,float Yaw,FName Why,FName Arrival,float Tolerance)
{
 // Route this candidate's existing hearth request through the verified free dock.
 // BeginQuietVisit calls Travel with QuietVisit, so this does not recurse.
 if(HearthPauseEnabled && Why==TEXT("HearthVisit"))return BeginQuietVisit(GetWorld()->GetTimeSeconds(),TEXT("HearthWarmPause"));
 const bool NewTerraceVisit=TerraceContactEnabled && Why==TEXT("TerracePalmVisit");
 if(!Safe(!NewTerraceVisit) || OwnsTravel)return false;
 OldPaused=C->LocoLifePaused;C->LocoLifePaused=true;RestoreLifePause=true;
 if(!C->LocoStartWalk(Goal,Yaw,true,Why,FixEnabled && (Why.ToString().StartsWith(TEXT("Bench")) || Why.ToString().StartsWith(TEXT("Floor")))?0:Tolerance)){ReleaseTravel();return false;}
 OwnsTravel=true;ActiveTask=Why;ArriveAction=Arrival;TaskSince=GetWorld()->GetTimeSeconds();TaskUntil=TaskSince+100;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT travel=%s goal=%s"),*Why.ToString(),*Goal.ToString());return true;
}
void ACourtyardContentLife::ReleaseTravel(){if(C.IsValid() && RestoreLifePause){C->LocoLifePaused=OldPaused;C->LocoLastArrived=true;C->LocoNextDecision=FMath::Max(C->LocoNextDecision,double(GetWorld()->GetTimeSeconds()+12));}RestoreLifePause=false;OwnsTravel=false;LeadWaitSince=-1;SharedSkyTravelOwned=false;}
void ACourtyardContentLife::TickTravel(double Now)
{
 if(LeadScenesEnabled && LeadPhase>0){TickLeadScene(Now);return;}
 if(!OwnsTravel)return;
 if(SharedSkyScenesEnabled && SharedSkyTravelOwned && GuardSharedSkyTravel(Now))return;
 if(Now>TaskUntil){C->LocoStopRequested=true;ArriveAction=NAME_None;}
 APawn* Player=C->GetPawn();auto* Emma=C->Emma();if(!Player || !Emma){ReleaseTravel();return;}
 const bool SeatedSky=SeatedSkyScenesEnabled && SharedSkyTravelOwned && C->Pose==ECourtyardPose::Seated && C->PlayerCameraManager;
 const FVector PlayerSpace=SeatedSky?C->PlayerCameraManager->GetCameraLocation():Player->GetActorLocation();
 const FVector Delta=PlayerSpace-Emma->GetActorLocation();const float Distance=Delta.Size2D();
 if(C->LocoState!=ACourtyardController::ELocoState::Idle){
  if(Distance<95 && FVector::DotProduct(Emma->GetActorForwardVector(),Delta.GetSafeNormal2D())>.6 && Player->GetVelocity().Size2D()<80){
   if(SeatedSky && !C->LocoStopRequested)UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky_stop actual_personal_space=1 distance=%.2f"),Distance);
   C->LocoStopRequested=true;
  }
  if(ActiveTask==TEXT("LeadInvitation") && (Distance>750 || Now>LeadExpires))C->LocoStopRequested=true;
  return;
 }
 if(Now-TaskSince<1)return;
 if(!C->LocoEndArrived){UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT arrival_cancelled task=%s reason=walk_interrupted"),*ActiveTask.ToString());if(ArriveAction==TEXT("SeatApproach"))ReadingSeat=false;ReleaseTravel();ActiveTask=ArriveAction=NAME_None;NextDecision=Now+120;return;}
 if(ActiveTask==TEXT("LeadInvitation")){
  if(LeadWaitSince<0){LeadWaitSince=Now;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT lead_wait leg=%d distance=%.0f"),LeadLeg,Distance);}
  if(Distance>650 || Now>LeadExpires || Now-LeadWaitSince>18){ReleaseTravel();ActiveTask=NAME_None;NextDecision=Now+180;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT lead_abandoned no_follow=1"));return;}
  if(PlayerVisible()){C->GazePoi=Player->GetActorLocation()+FVector(0,0,60);C->GazePoiUntil=Now+.5;C->GazePoiHead=.55f;}
  if(Distance>330 || Now-LeadWaitSince<2.5)return;
  const FVector ToGoal=LeadGoal-Emma->GetActorLocation();
  if(ToGoal.Size2D()>80){
   if(!Safe())return;const FVector Next=Emma->GetActorLocation()+ToGoal.GetClampedToMaxSize(240);
   if(!C->LocoStartWalk(Next,ToGoal.Rotation().Yaw,false,TEXT("LeadInvitation"),14)){ReleaseTravel();ActiveTask=ArriveAction=NAME_None;return;}
   ++LeadLeg;LeadWaitSince=-1;TaskSince=Now;TaskUntil=LeadExpires;return;
  }
 }
 const FName Arrive=ArriveAction;
 if(Arrive==TEXT("FloorUpDock") || Arrive==TEXT("FloorDownDock") || Arrive==TEXT("FloorRest") || Arrive==TEXT("FloorExit")){
  if(!Safe(false) && Now-C->LocoLastEndAt<2)return;
  ReleaseTravel();ArriveAction=ActiveTask=NAME_None;
  FloorJourney=Arrive==TEXT("FloorUpDock")?2:Arrive==TEXT("FloorDownDock")?9:Arrive==TEXT("FloorRest")?6:13;
  FloorDue=Now+(FloorJourney==6?Random.FRandRange(25,50):0);return;
 }
 if(Arrive==TEXT("SeatApproach")){
  // The walking controller reports arrival before its blend-out has finished.
  // Keep this arrival pending until the real pose is available; never snap it.
  if(!Safe(false) && Now-C->LocoLastEndAt<2)return;
  ReleaseTravel();ArriveAction=NAME_None;
  if(!BeginSeatApproach()){ReadingSeat=false;ActiveTask=NAME_None;UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seat_approach_rejected alignment_or_state=1"));}return;
 }
 if(Arrive==TEXT("BenchSit")){ReleaseTravel();ArriveAction=NAME_None;if(!BeginSeat())UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT seat_rejected alignment_or_occupancy=1"));return;}
 if(!Arrive.IsNone() && !Play(Arrive,Arrive)){if(Now-C->LocoLastEndAt<14)return;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT arrival_action_expired action=%s"),*Arrive.ToString());}
 else if(SharedSkyScenesEnabled && SharedSkyTravelOwned && Arrive==TEXT("SkyTogether")){SharedSkyWindowUntil=Now+45;UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_shared arrived=1 distance=%.2f yaw_error=%.2f"),FVector::Dist2D(Emma->GetActorLocation(),SharedSkyGoal),FMath::Abs(FMath::FindDeltaAngleDegrees(Emma->GetActorRotation().Yaw,SharedSkyYaw)));}
 ReleaseTravel();ArriveAction=NAME_None;
 if(Arrive.IsNone())ActiveTask=NAME_None;
}
TWeakObjectPtr<ACourtyardContentDoor> ACourtyardContentLife::NearDoor() const
{
 if(!C.IsValid() || !C->PlayerCameraManager)return nullptr;auto* Cam=C->PlayerCameraManager.Get();const FVector P=Cam->GetCameraLocation(),F=Cam->GetCameraRotation().Vector();
 ACourtyardContentDoor* Best=nullptr;float D=185;
 for(TActorIterator<ACourtyardContentDoor> I(GetWorld());I;++I){const FVector Target=I->InteractionPoint(),V=Target-P;const float Dist=V.Size();if(Dist<D && FVector::DotProduct(F,V.GetSafeNormal())>.45){FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(C->GetPawn());if(GetWorld()->LineTraceSingleByChannel(H,P,Target,ECC_Visibility,Q) && H.GetActor()!=*I)continue;D=Dist;Best=*I;}}
 return Best;
}
bool ACourtyardContentLife::HandleKey(FKey Key)
{
 if(!Enabled || !C.IsValid() || C->Page!=ECourtyardPage::Game)return false;
 if(Key==EKeys::R){if(auto D=NearDoor();D.IsValid())D->RequestToggle(C->GetPawn());return true;}
 if(Key==EKeys::N){if(auto D=NearDoor();D.IsValid())D->Knock();return true;}
 if(Key==EKeys::G){FlowerKey();return true;}return false;
}
void ACourtyardContentLife::SetEmotion(FName Type,const TCHAR* Why,bool Forced)
{
 const double Now=GetWorld()->GetTimeSeconds();if(!Forced && (Cooldowns.FindRef(Type)>Now || !Emotion.IsNone()))return;
 Emotion=Type;Intensity=TargetIntensity=1;EmotionSince=Now;ComfortMask=0;NextFace=Now;NearComfort=SpaceComfort=ScenicComfort=0;
 Cooldowns.Add(Type,Now+(Type==TEXT("Abandoned")?3600:Type==TEXT("PatAnger")?900:600));
 if(FixEnabled)BeginFixedEmotion();
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT emotion=%s reason=%s mood=%s forced=%d"),*Type.ToString(),Why,*C->Mood,Forced);
}
void ACourtyardContentLife::PatAccepted()
{
 if(LeadScenesEnabled && LeadPhase>0)CancelLeadScene(TEXT("headpat_accepted"));
 const double Now=GetWorld()->GetTimeSeconds();Pats.RemoveAll([Now](double T){return Now-T>75;});Pats.Add(Now);LastPat=Now;PatPending=Now+10;
 if(Emotion==TEXT("PatAnger")){TargetIntensity=FMath::Min(1.f,TargetIntensity+.18f);NextFace=Now+9;return;}
 if(Pats.Num()==3 && Emotion.IsNone()){PendingScene=TEXT("MildProtest");PendingUntil=Now+20;}
 if(Pats.Num()>=6)SetEmotion(TEXT("PatAnger"),TEXT("six_accepted_pats_in_75s"));
}
void ACourtyardContentLife::Comfort(int32 Method)
{
 if(Emotion.IsNone() || (ComfortMask&(1<<Method)))return;
 if(FixEnabled && Method==4 && Emotion!=TEXT("PatAnger") && Emotion!=TEXT("DoorAnger"))return;
 if(Method==2 && Emotion==TEXT("PatAnger"))return;
 ComfortMask|=1<<Method;
 float Benefit=.18f;if(Method==4)Benefit=Emotion==TEXT("PatAnger")?.42f:.09f;if(Method==0)Benefit=Emotion==TEXT("PatAnger")?.12f:.30f;
 TargetIntensity=FMath::Max(.1f,TargetIntensity-Benefit);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT comfort method=%d emotion=%s target=%.3f mask=%d smooth=1"),Method,*Emotion.ToString(),TargetIntensity,ComfortMask);
}
void ACourtyardContentLife::TickEmotion(float Dt,double Now)
{
 if(FixEnabled){TickFixedEmotion(Dt,Now);return;}
 if(!C->GetPawn() || !C->Emma())return;const float Dist=FVector::Dist(C->GetPawn()->GetActorLocation(),C->Emma()->GetActorLocation());
 Pats.RemoveAll([Now](double T){return Now-T>75;});
 if(PatPending>0 && Now>=PatPending){if(Now-LastPat>=9.5 && Pats.Num()==1)Comfort(2);PatPending=-1;}
 if(Emotion.IsNone())return;
 const bool Visible=PlayerVisible();
 if(Dist<250 && Visible && C->GetPawn()->GetVelocity().Size()<30)NearComfort+=Dt;else NearComfort=FMath::Max(0.,NearComfort-Dt);
 if(Dist>450)SpaceComfort+=Dt;else SpaceComfort=0;
 if(NearComfort>28)Comfort(1);if(SpaceComfort>45)Comfort(4);
 if(Dist<350 && Visible && C->GetControlRotation().Pitch>23 && SharedSkyVisible()){ScenicComfort+=Dt;if(ScenicComfort>16)Comfort(3);if(Environment.IsValid() && Environment->MeteorBrightness>.3f)Comfort(5);}else ScenicComfort=0;
 int Count=0;for(int N=0;N<6;++N)if(ComfortMask&(1<<N))++Count;
 const bool Despair=Emotion==TEXT("Abandoned") || Emotion==TEXT("PetalDespair");
 const bool Recover=Despair?Count>=3:Count>0 || Now-EmotionSince>90;
 if(Recover)TargetIntensity=FMath::Max(0.f,TargetIntensity-Dt*.0075f);
 if((Emotion==TEXT("Reunion") || Emotion==TEXT("ReunionDeep")) && Now-EmotionSince>25)TargetIntensity=FMath::Max(0.f,TargetIntensity-Dt*.012f);
 Intensity=FMath::FInterpConstantTo(Intensity,TargetIntensity,Dt,.028f);
 if(Now>=EmotionReportAt){EmotionReportAt=Now+10;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT emotion_state type=%s intensity=%.3f target=%.3f comforts=%d"),*Emotion.ToString(),Intensity,TargetIntensity,Count);}
 if(Intensity<=.035f){UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT recovered emotion=%s mood=%s"),*Emotion.ToString(),*C->Mood);Emotion=NAME_None;C->CancelFace();return;}
 if(Now<NextFace || !Safe(false))return;
 TArray<FLivelyFacePhase> Phases;const float W=FMath::Clamp(Intensity,.18f,1.f);
 if(Emotion==TEXT("PatAnger") || Emotion==TEXT("DoorAnger"))Phases={ContentLife::Face(0,1.3,{{50,.45f*W}}),ContentLife::Face(1.35,4.8,{{51,.66f*W}}),ContentLife::Face(4.85,6.5,{{14,.42f*W}})};
 else if(Emotion==TEXT("Surprise"))Phases={ContentLife::Face(0,1.1,{{13,.6f*W}},.18f),ContentLife::Face(1.15,3.2,{{57,.5f*W}}),ContentLife::Face(3.25,6.2,{{45,.3f}})};
 else if(Emotion==TEXT("Overwhelmed") || Emotion==TEXT("DoorShy"))Phases={ContentLife::Face(0,2.1,{{35,.50f*W},{72,.4f*W}}),ContentLife::Face(2.15,5.9,{{66,.50f*W}})};
 else if(Emotion==TEXT("Reunion") || Emotion==TEXT("ReunionDeep")){
  if(Emotion==TEXT("ReunionDeep") && Now-EmotionSince<12)Phases={ContentLife::Face(0,4,{{59,.52f},{69,.45f}}),ContentLife::Face(4.1,6.5,{{64,.65f},{69,.35f}})};
  else Phases={ContentLife::Face(0,3,{{45,.40f},{18,.35f},{69,.35f}}),ContentLife::Face(3.1,6.5,{{64,.55f},{69,.22f}})};
 }else if(Despair)Phases={ContentLife::Face(0,2.6,{{58,.36f*W},{69,.36f*W}}),ContentLife::Face(2.7,6.5,{{60,.45f*W},{70,.24f*W}})};
 else Phases={ContentLife::Face(0,6.5,{{53,.45f*W},{69,.22f*W}})};
 const bool Started=C->StartFace(FName(*(TEXT("Content_")+Emotion.ToString())),Phases,6.8f,TEXT(""),5);
 NextFace=Now+(Started?Random.FRandRange(11,17):.5f);
}
void ACourtyardContentLife::ShowGift(bool Player)
{
 if(!C->PlayerCameraManager)return;
 if(Player){const FTransform View(C->PlayerCameraManager->GetCameraRotation(),C->PlayerCameraManager->GetCameraLocation());
  // The original -21cm offset puts almost all of this 13cm stem below the
  // normal 80-degree view. Keep that legacy placement on old maps.
  const FVector Offset=HeldFlowerFramingEnabled?FVector(54,20,-5):FVector(38,17,-21);
  Gift->SetWorldTransform(FTransform(View.GetRotation(),View.TransformPosition(Offset),FVector(1)));}
 Gift->SetVisibility(true);
}
void ACourtyardContentLife::FlowerKey()
{
 const double Now=GetWorld()->GetTimeSeconds();if(!C->PlayerCameraManager)return;
 if(ReceiveGiftSince>=0)return; // Do not pick/drop or replace the shared mesh mid-handoff.
 const FVector P=C->PlayerCameraManager->GetCameraLocation(),F=C->PlayerCameraManager->GetCameraRotation().Vector();
 if(HoldingFlower){
  const FVector To=C->Emma()->GetActorLocation()-P;
  float Bearing=0,Distance=0;C->PlayerBearing(Bearing,Distance);
  if(Safe() && Distance<165 && FMath::Abs(Bearing)<55 && FVector::DotProduct(F,To.GetSafeNormal())>.45 && PlayerVisible() && Cooldowns.FindRef(TEXT("FlowerOffering"))<Now){
   if(!Emotion.IsNone()){
    Comfort(0);Cooldowns.Add(TEXT("FlowerOffering"),Now+120);
    C->GazePoi=Gift->GetComponentLocation();C->GazePoiUntil=Now+3.5;C->GazePoiHead=.25f;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT flower_offered noncontact_fallback=1 emotional=1 retained_by_player=1"));return;
   }
   if(FixEnabled && ContactValidated && BeginReceiveGift())return;
   if(Play(TEXT("FlowerOffering"),TEXT("FlowerOffering"))){Comfort(0);Cooldowns.Add(TEXT("FlowerOffering"),Now+120);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT flower_offered noncontact_fallback=1 retained_by_player=1"));}return;
  }
  FHitResult Ground;FCollisionQueryParams DropQ;DropQ.AddIgnoredActor(C->GetPawn());DropQ.AddIgnoredActor(C->Emma());
  if(GetWorld()->LineTraceSingleByChannel(Ground,P,P+F*200,ECC_Visibility,DropQ) && Ground.ImpactNormal.Z>.85f){
   HoldingFlower=false;GiftUntil=Now+90;Gift->SetWorldTransform(FTransform(FRotator(0,C->GetControlRotation().Yaw,90),Ground.ImpactPoint+FVector(0,0,2.92),FVector(1)));UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT flower_placed retention=90"));
  }return;
 }
 // A flower is only a few centimetres wide. Use a small focus cone and a real
 // visibility ray, so the tiny stem does not require pixel-perfect collision aim.
 FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(C->GetPawn());
 AActor* A=nullptr;float Best=.982f;
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  if(!It->ActorHasTag(TEXT("ContentPickableFlower")) || Picked.Contains(*It) || It->IsHidden())continue;
  const FVector V=It->GetActorLocation()-P;const float D=V.Size(),Dot=FVector::DotProduct(F,V.GetSafeNormal());
  if(D>190 || Dot<Best)continue;
  if(GetWorld()->LineTraceSingleByChannel(Hit,P,It->GetActorLocation(),ECC_Visibility,Q) && Hit.GetActor()!=*It && Hit.Distance<D-7)continue;
  A=*It;Best=Dot;
 }
 if(!A){UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT flower_pick no_visible_target=1"));return;}
 FlowerOrigin=A->GetActorLocation();Picked.Add(A,Now+240);PickedFlower=A;A->SetActorHiddenInGame(true);A->SetActorEnableCollision(false);HoldingFlower=true;ShowGift(true);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT flower_picked regrow=240"));
}
void ACourtyardContentLife::TickScenes(float Dt,double Now)
{
 if(!C->GetPawn() || !C->Emma())return;APawn* P=C->GetPawn();auto* E=C->Emma();const FVector Pos=P->GetActorLocation();const float Dist=FVector::Dist2D(Pos,E->GetActorLocation());
 if(ReunionPending && Now>25 && C->WelcomeDone && Safe()){ReunionPending=false;if(C->LastVisitHours>=72)SetEmotion(C->LastVisitHours>168?TEXT("ReunionDeep"):TEXT("Reunion"),TEXT("saved_absence"));}
 // Emma wandering off is not the player abandoning her. Remember the player's
 // last companion position and require the player to have actually left it.
 if(Dist<320){Company+=Dt;LastNear=Now;Away=0;CompanyPlayerAnchor=Pos;}
 else if(Dist>1100 && FVector::Dist2D(Pos,CompanyPlayerAnchor)>700){Away+=Dt;if(Company>480 && Away>180)SetEmotion(TEXT("Abandoned"),TEXT("player_left_company_then_three_minutes_away"));}
 else Away=0;
 if(C->LivelyBodyId!=LastBody){
  if(LastBody==TEXT("C9")){if(C->LivingPetalFailed){++PetalMisses;if(PetalMisses>=3){SetEmotion(TEXT("PetalDespair"),TEXT("third_consecutive_petal_failure"));PetalMisses=0;}}else PetalMisses=0;}
  if(C->LivelyBodyId==TEXT("D2")){if(Now-LastSneezed<12)++Sneezes;else Sneezes=1;LastSneezed=Now;if(Sneezes>=2 && Environment.IsValid() && Environment->CurrentWind>.5f)SetEmotion(TEXT("Overwhelmed"),TEXT("gust_and_two_sneezes"));}
  LastBody=C->LivelyBodyId;
 }
 const FVector ToPlayer=(Pos-E->GetActorLocation()).GetSafeNormal2D();const float Rear=FVector::DotProduct(E->GetActorForwardVector(),ToPlayer);
 const float Speed=P->GetVelocity().Size2D();
 if(PreviousDistance>175 && Dist<175 && Rear<-.65 && Speed>220 && PlayerVisible())SetEmotion(TEXT("Surprise"),TEXT("fast_rear_approach"));
 if(Environment.IsValid() && Environment->MeteorKind>=2 && Environment->MeteorBrightness>.8f && Dist<500 && C->GetControlRotation().Pitch>15 && C->LivelySkyOpen(20))SetEmotion(TEXT("Surprise"),TEXT("rare_bright_meteor"));
 if(C->GetControlRotation().Pitch>26 && Speed<8 && (C->Pose!=ECourtyardPose::Seated || SharedSkyScenesEnabled))StillSky+=Dt;else StillSky=0;
 if(SharedSkyScenesEnabled)TickSharedSky(Now);
 if(!SharedSkyScenesEnabled && !BlocksOrdinaryBehavior() && StillSky>18 && Dist>220 && Dist<650 && FMath::Abs(Pos.Z-E->GetActorLocation().Z)<100 && PlayerVisible() && SharedSkyVisible() && Cooldowns.FindRef(TEXT("SkyTogether"))<Now && Safe()){
  const float SkyYaw=C->GetControlRotation().Yaw;const FVector Side=FRotator(0,SkyYaw,0).RotateVector(FVector(0,135,0));FVector Goal=Pos+Side;Goal.Z=E->GetActorLocation().Z;
  if(Travel(Goal,SkyYaw,TEXT("SkyTogether"),TEXT("SkyTogether"))){Cooldowns.Add(TEXT("SkyTogether"),Now+360);StillSky=0;}}
 if(Now>=NextDecision && Safe() && !OwnsTravel && Emotion.IsNone()){
  NextDecision=Now+Random.FRandRange(110,220);
  const FVector Q=E->GetActorLocation();
  if(FVector::Dist2D(Q,FVector(350,1080,0))<165)Play(TEXT("WarmHands"),TEXT("HearthWarm"));
  else if(FVector::Dist2D(Q,FVector(-300,-2440,0))<210)Play(TEXT("BlossomListen"),TEXT("BlossomListen"));
  else if(C->LocoIndoors() && FVector::Dist2D(Q,FVector(-450,650,0))<180)Play(TEXT("WindowReflect"),TEXT("WindowReflect"));
  else if(Dist<380 && PlayerVisible() && Random.FRand()<.33f){
   const TArray<FVector> Places={FVector(-300,-2440,Q.Z),FVector(350,1205,Q.Z),FVector(420,-1400,Q.Z)};TArray<FVector> Choices;
   for(const auto& Goal:Places){
    // The capped ancient-tree visit remains disabled. An invitation is not a
    // reason to retry its rejected route under a new name.
    if(LeadScenesEnabled && Goal.Y<-2000 && !QuietFlowerVisitsEnabled)continue;
    const float D=FVector::Dist2D(Q,Goal);if(D>280 && D<950)Choices.Add(Goal);
   }
   if(Choices.Num()){
    const FVector Goal=Choices[Random.RandRange(0,Choices.Num()-1)];
    if(LeadScenesEnabled){
     // The current pointing clip points UP. It is not a flower/fireplace pointing clip.
     // Those destinations retain their appropriate existing arrival action, and remain
     // partial relative to the full original pointing-at-object requirement.
     const FName Arrival=Goal.Y<-2000?TEXT("BlossomListen"):Goal.Y>500?TEXT("WarmHands"):TEXT("InviteView");
     BeginLeadScene(Goal,Arrival);
    }else{
     LeadGoal=Goal;const FVector Step=Q+(LeadGoal-Q).GetClampedToMaxSize(240);
     if(Travel(Step,(LeadGoal-Q).Rotation().Yaw,TEXT("LeadInvitation"),TEXT("InviteView"))){LeadLeg=1;LeadExpires=TaskUntil=Now+150;}
    }
   }
  }else if(C->LocoIndoors() && FVector::Dist2D(Q,FVector(350,1205,0))<750 && Random.FRand()<.35f)Travel(FVector(350,1205,Q.Z),90,TEXT("HearthVisit"),TEXT("WarmHands"));
 }
 PreviousDistance=Dist;PreviousPlayer=Pos;
}
void ACourtyardContentLife::Tick(float Dt)
{
 Super::Tick(Dt);if(!Enabled || !C.IsValid())return;const double Now=GetWorld()->GetTimeSeconds();
 if(ReviewFlight){auto* A=Cast<ACharacter>(C->Emma());auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  if(I && !I->Montage_IsPlaying(ReviewFlight) && Now-ReviewFlightStart>1){A->GetCapsuleComponent()->SetCapsuleHalfHeight(ReviewCapsuleHalf);A->GetMesh()->SetRelativeLocation(ReviewMeshOffset);A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);C->LocoLifePaused=ReviewWasPaused;ContentLife::Flag(A,TEXT("AllowInteraction"),true);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT stair_review_end pos=%s"),*A->GetActorLocation().ToString());ReviewFlight=nullptr;}else return;}
 if(C->Page!=ECourtyardPage::Game && !FixEnabled)return;
 // Player-held props and completion cleanup must tick even while Emma owns stairs,
 // seating or an upstairs recovery state. They are not part of her travel scheduler.
 if(TickReceiveGift(Now)){}else if(HoldingFlower)ShowGift(true);else if(GiftUntil>0 && Now>GiftUntil){GiftUntil=0;Gift->SetVisibility(false);}
 for(auto I=Picked.CreateIterator();I;++I)if(Now>I.Value()){if(I.Key().IsValid()){I.Key()->SetActorHiddenInGame(false);I.Key()->SetActorEnableCollision(true);}I.RemoveCurrent();}
 TickFlowerScent(Now);
 TickQuietSites(Now);
 TickTerracePalm(Now);
 if(C->Page!=ECourtyardPage::Game){
  // Menus do not pause world animation. Continue only existing actions so their
  // retained phases and cleanup cannot become stale while settings are open.
  if(LeadScenesEnabled && LeadPhase>0)TickLeadScene(Now);
  TickDoorAction(Now);TickStudyBook(Now);TickFixedStairs(Dt,Now);
  if(SeatMotion || SeatApproachMotion)TickSeat(Now);
  if(SeatReadyUntil>0)SeatReadyUntil=Now+2;
  TickEmotion(Dt,Now);return;
 }
 TickReviewRoomWalk(Dt);
 if(TickStudyBook(Now)){TickEmotion(Dt,Now);return;}
 if(TickDoorAction(Now)){TickEmotion(Dt,Now);return;}
 if(ReviewFollow && C->GetPawn() && C->Emma()){const FVector To=C->Emma()->GetActorLocation()-C->GetPawn()->GetActorLocation();if(To.Size2D()>225)C->GetPawn()->AddMovementInput(To.GetSafeNormal2D(),.36f);C->SetControlRotation(FMath::RInterpTo(C->GetControlRotation(),FRotator(0,To.Rotation().Yaw,0),Dt,2));}
 if(TickFixedStairs(Dt,Now))return;
 TickSeat(Now);if(SeatMotion || SeatApproachMotion){TickEmotion(Dt,Now);return;}
 TickTravel(Now);TickFloorJourney(Now);if(FloorJourney){TickEmotion(Dt,Now);return;}
 TickNewReactions(Dt,Now);TickScenes(Dt,Now);TickEmotion(Dt,Now);
 if(!OwnsTravel && !ActiveTask.IsNone() && Now>TaskUntil)ActiveTask=NAME_None;
}
void ACourtyardContentLife::Review(int32 Event)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 if(Event>=0 && Event<4 && Environment.IsValid())Environment->TriggerMeteor(Event,true);
 if(Event==4 && Environment.IsValid())Environment->TriggerGust(true);
 const TCHAR* E[]={TEXT("Abandoned"),TEXT("Reunion"),TEXT("ReunionDeep"),TEXT("PetalDespair"),TEXT("PatAnger"),TEXT("Surprise"),TEXT("Overwhelmed"),TEXT("DoorShy"),TEXT("DoorAnger")};
 if(Event>=10 && Event<19)SetEmotion(E[Event-10],TEXT("explicit_review"),true);
 if(Event>=30 && Event<36)Comfort(Event-30);
 if(Event==40)Play(TEXT("WarmHands"),TEXT("HearthWarm"));
 if(Event==99){if(FixEnabled)ClearFixedEmotion();else{Emotion=NAME_None;Intensity=0;C->CancelFace();}}
 if(Event==200 && Environment.IsValid())C->SetControlRotation(Environment->MeteorDirection.Rotation());
 if(Event>=201 && Event<=203)ReviewMotion(Event-201);
 if(Event==210)SurfaceProbe(TEXT("StairFlightUp_r3"));
 if(Event==211)SurfaceProbe(TEXT("StairFlightDown_r3"));
 if(Event==212)SurfaceProbe(TEXT("ReceiveFlower"));
 if(Event==213)SurfaceProbe(TEXT("SitCycle_r3"));
 if(Event==214)SurfaceProbe(TEXT("Gift"));
 ReviewExtra(Event);
}
