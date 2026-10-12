#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentEnvironment.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

// This scene owns only its new travel transaction. The accepted locomotion and
// the pre-existing stargazing response keep their original motion and timers.
bool ACourtyardContentLife::SharedSkyContext() const
{
 if(!C.IsValid() || !C->GetPawn() || !C->Emma() || !C->PlayerCameraManager || C->Page!=ECourtyardPage::Game)return false;
 const FVector P=C->GetPawn()->GetActorLocation(),E=C->Emma()->GetActorLocation();
 return (C->Pose==ECourtyardPose::Standing || C->Pose==ECourtyardPose::Seated) &&
  C->GetControlRotation().Pitch>26 && C->GetPawn()->GetVelocity().Size2D()<8 &&
  FVector::Dist2D(P,E)<650 && FMath::Abs(P.Z-E.Z)<100 &&
  FVector::Dist2D(P,SharedSkyAnchor)<75 &&
  FMath::Abs(FMath::FindDeltaAngleDegrees(SharedSkyYaw,C->GetControlRotation().Yaw))<30 &&
  PlayerVisible() && SharedSkyVisible() && Emotion.IsNone();
}

bool ACourtyardContentLife::GuardSharedSkyTravel(double Now)
{
 auto* E=C->Emma();
 const bool Moving=C->LocoState!=ACourtyardController::ELocoState::Idle;
 const TCHAR* Cancel=nullptr;
 if(!SharedSkyCancelled && !SharedSkyContext())Cancel=TEXT("player_context_changed");
 if(Moving && !SharedSkyCancelled){
  if(FVector::Dist2D(E->GetActorLocation(),SharedSkyLastProgress)>8){SharedSkyLastProgress=E->GetActorLocation();SharedSkyProgressAt=Now;}
  else if(Now-SharedSkyProgressAt>4)Cancel=TEXT("no_motion_progress");
 }
 if(!Moving && !SharedSkyCancelled && Now-TaskSince>1){
  const float Error=FVector::Dist2D(E->GetActorLocation(),SharedSkyGoal);
  const float YawError=FMath::Abs(FMath::FindDeltaAngleDegrees(E->GetActorRotation().Yaw,SharedSkyYaw));
  if(!C->LocoEndArrived || Error>30 || YawError>25)Cancel=TEXT("actual_arrival_not_qualified");
 }
 if(Cancel && !SharedSkyCancelled){
  SharedSkyCancelled=true;ArriveAction=NAME_None;C->LocoStopRequested=Moving;
  UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_cancel reason=%s position=%s goal_error=%.2f"),Cancel,*E->GetActorLocation().ToString(),FVector::Dist2D(E->GetActorLocation(),SharedSkyGoal));
 }
 if(!Moving && SharedSkyCancelled){
  ReleaseTravel();ActiveTask=ArriveAction=NAME_None;StillSky=0;NextDecision=FMath::Max(NextDecision,Now+120);
  UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_released actual_stop=1 interaction_owner_restored=1"));return true;
 }
 return SharedSkyCancelled;
}

void ACourtyardContentLife::TickSharedSky(double Now)
{
 if(SeatedSkyScenesEnabled && C.IsValid() && C->Pose==ECourtyardPose::Seated){TickSeatedSharedSky(Now);return;}
 if(OwnsTravel || BlocksOrdinaryBehavior() || !Emotion.IsNone() || !C->GetPawn() || !C->Emma())return;
 const FVector P=C->GetPawn()->GetActorLocation(),E=C->Emma()->GetActorLocation();
 const float Distance=FVector::Dist2D(P,E),Yaw=C->GetControlRotation().Yaw;
 const bool Looking=(C->Pose==ECourtyardPose::Standing || C->Pose==ECourtyardPose::Seated) &&
  C->GetControlRotation().Pitch>26 && C->GetPawn()->GetVelocity().Size2D()<8 && FMath::Abs(P.Z-E.Z)<100;
 if(!Looking || Distance>650 || !PlayerVisible() || !SharedSkyVisible())return;
 if(SharedSkyWindowUntil>Now && Environment.IsValid() && Environment->MeteorBrightness>.12f &&
  Cooldowns.FindRef(TEXT("SharedSkyMeteor"))<Now && Safe()){
  const float Bearing=FMath::Abs(FMath::FindDeltaAngleDegrees(C->Emma()->GetActorRotation().Yaw,Environment->MeteorDirection.Rotation().Yaw));
  if(Bearing<35 && Play(TEXT("InviteView"),TEXT("SharedSkyMeteor"))){Cooldowns.Add(TEXT("SharedSkyMeteor"),Now+90);UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_meteor_response real_event=1 kind=%d bearing=%.1f"),Environment->MeteorKind,Bearing);return;}
 }
 if(StillSky<=18 || Distance<110 || Cooldowns.FindRef(TEXT("SkyTogether"))>=Now || !Safe())return;
 const FVector Local=FRotator(0,Yaw,0).UnrotateVector(E-P);
 const float Heading=FMath::Abs(FMath::FindDeltaAngleDegrees(C->Emma()->GetActorRotation().Yaw,Yaw));
 if(FMath::Abs(Local.X)<95 && FMath::Abs(Local.Y)>95 && Distance<225 && Heading<35){
  if(Play(TEXT("SkyTogether"),TEXT("SkyTogether"))){Cooldowns.Add(TEXT("SkyTogether"),Now+360);StillSky=0;SharedSkyWindowUntil=Now+45;UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_shared already_beside=1 distance=%.2f heading=%.2f"),Distance,Heading);}return;
 }
 FCollisionQueryParams Q(SCENE_QUERY_STAT(SharedSkyPlace),false,C->Emma());Q.AddIgnoredActor(C->GetPawn());
 FVector Goal=FVector::ZeroVector;float Best=TNumericLimits<float>::Max();
 for(const float Sign:{-1.f,1.f}){
  FVector Candidate=P+FRotator(0,Yaw,0).RotateVector(FVector(0,135*Sign,0));Candidate.Z=E.Z;
  FHitResult Ground;
  const bool Floor=GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector(0,0,60),Candidate-FVector(0,0,120),ECC_Visibility,Q);
  if(!Floor || Ground.ImpactNormal.Z<.85f || FMath::Abs(Ground.ImpactPoint.Z-(E.Z-80))>25)continue;
  if(GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q))continue;
  const float Cost=FVector::DistSquared2D(E,Candidate);if(Cost<Best){Best=Cost;Goal=Candidate;}
 }
 if(Best==TNumericLimits<float>::Max()){Cooldowns.Add(TEXT("SkyTogether"),Now+90);StillSky=0;UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_skipped no_clear_side=1"));return;}
 if(Travel(Goal,Yaw,TEXT("SkyTogether"),TEXT("SkyTogether"))){
  SharedSkyGoal=Goal;SharedSkyYaw=Yaw;SharedSkyAnchor=P;SharedSkyLastProgress=E;SharedSkyProgressAt=Now;
  SharedSkyTravelOwned=true;SharedSkyCancelled=false;Cooldowns.Add(TEXT("SkyTogether"),Now+360);StillSky=0;
  UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_approach real_walk=1 selected_side=%s player=%s"),*Goal.ToString(),*P.ToString());
 }else{Cooldowns.Add(TEXT("SkyTogether"),Now+90);StillSky=0;UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_skipped walk_not_available=1"));}
}
