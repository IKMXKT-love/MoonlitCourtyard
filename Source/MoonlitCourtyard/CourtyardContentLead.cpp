#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"

// New-map opt-in. The accepted stride, gesture, face and gaze owners are reused.
// The controller may report nominal arrival; this scene also checks real placement.
bool ACourtyardContentLife::BeginLeadScene(const FVector& Goal,FName Arrival)
{
 if(!LeadScenesEnabled || LeadPhase || OwnsTravel || BlocksOrdinaryBehavior() || !Safe() ||
    !C->GetPawn() || !C->Emma() || C->Pose!=ECourtyardPose::Standing || !PlayerVisible())return false;
 const double Now=GetWorld()->GetTimeSeconds();
 const FVector E=C->Emma()->GetActorLocation(),P=C->GetPawn()->GetActorLocation();
 const float Distance=FVector::Dist2D(P,E),Bearing=FMath::Abs(FMath::FindDeltaAngleDegrees(C->Emma()->GetActorRotation().Yaw,(P-E).Rotation().Yaw));
 if(Distance<140 || Distance>380 || Bearing>55 || FMath::Abs(P.Z-E.Z)>100 ||
    FVector::Dist2D(E,Goal)<280 || FVector::Dist2D(E,Goal)>950 || Cooldowns.FindRef(TEXT("LeadScene"))>=Now)return false;
 auto* Signal=C->LivelyMotion(C->Mood==TEXT("lonely")?TEXT("WaveLonely"):TEXT("WaveHappy"),nullptr);
 if(!Signal || !C->PlayCompanion(Signal,735,TEXT("content35_invitation")))return false;
 LeadGoal=Goal;LeadArrival=Arrival;LeadLeg=0;LeadPhase=1;LeadCancelled=LeadLooked=false;
 LeadExpires=Now+150;LeadWaitSince=-1;LeadHiddenSince=-1;
 OldPaused=C->LocoLifePaused;C->LocoLifePaused=true;RestoreLifePause=true;OwnsTravel=true;
 ActiveTask=TEXT("LeadSignal");TaskSince=Now;TaskUntil=LeadExpires;
 C->GazePoi=P+FVector(0,0,60);C->GazePoiHead=1;C->GazePoiUntil=Now+Signal->GetPlayLength();
 C->LivelyStarted(TEXT("N_LeadSignal"),TEXT("content_context"),true);
 Cooldowns.Add(TEXT("LeadScene"),Now+360);
 UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_signal motion=%s goal=%s arrival=%s distance=%.1f bearing=%.1f"),*Signal->GetPathName(),*Goal.ToString(),*Arrival.ToString(),Distance,Bearing);
 return true;
}

bool ACourtyardContentLife::BeginLeadLeg(double Now)
{
 if(!Safe(false) || !C->GetPawn() || !C->Emma())return false;
 const FVector E=C->Emma()->GetActorLocation(),P=C->GetPawn()->GetActorLocation(),To=LeadGoal-E;
 const float Distance=To.Size2D();
 if(Distance<=30){LeadPhase=3;LeadWaitSince=Now;LeadLooked=false;return true;}
 LeadLegGoal=E+To.GetSafeNormal2D()*(Distance/FMath::Max(1,FMath::CeilToInt(Distance/280.f)));LeadLegGoal.Z=E.Z;
 const float PathYaw=To.Rotation().Yaw,PlayerYaw=(P-LeadLegGoal).Rotation().Yaw;
 const float Back=FMath::FindDeltaAngleDegrees(PathYaw,PlayerYaw);
 // Reserve gaze margin for the player catching up; still require the existing55deg limit.
 // Finish the WALK with the player within the existing 55-deg gaze range.
 // This changes the run-in path, not the root/body of a stationary pose.
 const float Turn=FMath::Sign(Back)*FMath::Max(0.f,FMath::Abs(Back)-25.f);
 if(FMath::Abs(Turn)>155){UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_leg_rejected reason=turn_outside_candidate_range turn=%.1f"),Turn);return false;}
 LeadLegYaw=FMath::UnwindDegrees(PathYaw+Turn);
 if(!C->LocoStartWalk(LeadLegGoal,LeadLegYaw,true,TEXT("LeadSceneLeg"),14))return false;
 // Sample the path actually selected by the old walk controller, not only endpoints.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(LeadScenePath),false,C->Emma());Q.AddIgnoredActor(C->GetPawn());
 bool Clear=true;
 const float Length=C->LocoPathDist.Num()?C->LocoPathDist.Last():0;
 for(float Along=0;Along<=Length && Clear;Along+=35){
  const FVector Site=C->LocoPointAt(Along);FHitResult Ground;
  Clear=GetWorld()->LineTraceSingleByChannel(Ground,Site+FVector(0,0,55),Site-FVector(0,0,120),ECC_Visibility,Q) &&
    Ground.ImpactNormal.Z>.85 && FMath::Abs(Ground.ImpactPoint.Z-(E.Z-80))<25 &&
    !GetWorld()->OverlapBlockingTestByChannel(Site,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q);
 }
 LeadPhase=2;ActiveTask=TEXT("LeadSceneLeg");TaskSince=Now;TaskUntil=LeadExpires;
 LeadLastProgress=E;LeadProgressAt=Now;LeadPlayerAnchor=P;LeadWaitSince=-1;LeadLooked=false;
 C->GazePoiUntil=Now;++LeadLeg;
 UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_leg start=%d goal=%s yaw=%.1f path_length=%.1f path_clear=%d"),LeadLeg,*LeadLegGoal.ToString(),LeadLegYaw,Length,Clear);
 if(!Clear || Length>750)CancelLeadScene(TEXT("selected_path_not_qualified"));
 return true;
}

void ACourtyardContentLife::CancelLeadScene(const TCHAR* Reason)
{
 if(!LeadPhase || LeadCancelled)return;
 LeadCancelled=true;ArriveAction=NAME_None;
 C->GazePoiUntil=GetWorld()->GetTimeSeconds();
 if(C->LocoState!=ACourtyardController::ELocoState::Idle)C->LocoStopRequested=true;
 UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_cancel reason=%s phase=%d leg=%d"),Reason,LeadPhase,LeadLeg);
}

void ACourtyardContentLife::EndLeadScene(const TCHAR* Reason)
{
 const int32 FinishedLegs=LeadLeg;
 LeadPhase=0;LeadCancelled=false;LeadLooked=false;LeadHiddenSince=-1;LeadWaitSince=-1;
 ReleaseTravel();ActiveTask=ArriveAction=NAME_None;
 C->GazePoiUntil=GetWorld()->GetTimeSeconds();
 NextDecision=FMath::Max(NextDecision,double(GetWorld()->GetTimeSeconds()+180));
 UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_end reason=%s legs=%d pause_restored=%d real_idle=%d"),Reason,FinishedLegs,!RestoreLifePause,C->LocoState==ACourtyardController::ELocoState::Idle);
}

void ACourtyardContentLife::TickLeadScene(double Now)
{
 if(!C.IsValid() || !C->Emma() || !C->GetPawn()){LeadPhase=0;ReleaseTravel();return;}
 auto* E=Cast<ACharacter>(C->Emma());auto* I=E?E->GetMesh()->GetAnimInstance():nullptr;
 if(!I){CancelLeadScene(TEXT("missing_animation_owner"));return;}
 const bool Moving=C->LocoState!=ACourtyardController::ELocoState::Idle;
 const FVector P=C->GetPawn()->GetActorLocation(),Here=E->GetActorLocation();
 const float Dist=FVector::Dist2D(P,Here);
 if(!PlayerVisible()){if(LeadHiddenSince<0)LeadHiddenSince=Now;}else LeadHiddenSince=-1;
 if(!LeadCancelled && (Now>LeadExpires || Dist>750 || FMath::Abs(P.Z-Here.Z)>100 ||
    !Emotion.IsNone() || C->Page!=ECourtyardPage::Game || C->Pose!=ECourtyardPose::Standing ||
    (LeadHiddenSince>=0 && Now-LeadHiddenSince>3)))CancelLeadScene(TEXT("company_context_lost"));
 if(LeadCancelled){if(!Moving && !I->IsAnyMontagePlaying())EndLeadScene(TEXT("cancelled_after_stop"));return;}
 if(LeadPhase==1){
  if(I->IsAnyMontagePlaying() || !Safe(false))return;
  if(!BeginLeadLeg(Now))EndLeadScene(TEXT("first_leg_unavailable"));return;
 }
 if(LeadPhase==2){
  if(Moving){
   // A blocked capsule must not keep stepping until a multi-second timeout.
   // Use the accepted controller's existing .35s blend-out emergency exit;
   // no prop displacement, endpoint snap, new gait or avoidance framework.
   FCollisionQueryParams Q(SCENE_QUERY_STAT(LeadAheadObstacle),false,E);
   FVector From=Here;
   for(float Ahead=30;Ahead<=150;Ahead+=30){
    const FVector To=C->LocoPointAt(C->LocoProgress+Ahead);FHitResult Hit;
    if(GetWorld()->SweepSingleByChannel(Hit,From,To,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(24),Q)){
     UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_obstacle actor=%s hit_distance=%.2f anticipatory_exit=1"),*GetNameSafe(Hit.GetActor()),Hit.Distance);
     CancelLeadScene(TEXT("route_obstructed"));C->LocoEnd(false,TEXT("lead_near_obstacle"));return;
    }
    From=To;
   }
   if(FVector::Dist2D(Here,LeadLastProgress)>8){LeadLastProgress=Here;LeadProgressAt=Now;}
   else if(Now-LeadProgressAt>4)CancelLeadScene(TEXT("no_motion_progress"));
   if(C->LocoStopRequested)CancelLeadScene(TEXT("walk_stop_requested"));
   return;
  }
  if(Now-TaskSince<1 || I->IsAnyMontagePlaying())return;
  const float Error=FVector::Dist2D(Here,LeadLegGoal),Yaw=FMath::Abs(FMath::FindDeltaAngleDegrees(E->GetActorRotation().Yaw,LeadLegYaw));
  if(!C->LocoEndArrived || Error>30 || Yaw>25){CancelLeadScene(TEXT("actual_leg_arrival_failed"));return;}
  LeadPhase=3;LeadWaitSince=Now;LeadLooked=false;ActiveTask=TEXT("LeadWait");
  UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_wait leg=%d distance=%.1f goal_error=%.2f yaw_error=%.2f player_progress=%.1f"),LeadLeg,Dist,Error,Yaw,FVector::Dist2D(P,LeadPlayerAnchor));
 }
 if(LeadPhase==3){
  const float Bearing=FMath::Abs(FMath::FindDeltaAngleDegrees(E->GetActorRotation().Yaw,(P-Here).Rotation().Yaw));
  // Existing additive head55deg + eye12deg (AnimNode_CourtyardGaze.cpp:91-92).
  // Reserve4deg eye margin; do not change either actual control limit.
  const float GazeRange=(C->GazeLayerEnabled?55.f:35.f)+8.f;
  if(!LeadLooked && PlayerVisible() && Bearing<GazeRange){
   LeadLooked=true;LeadLookUntil=Now+2.4;C->GazePoi=P+FVector(0,0,60);C->GazePoiHead=1;C->GazePoiUntil=LeadLookUntil;
   UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_checkback leg=%d bounded_gaze=1 bearing=%.1f"),LeadLeg,Bearing);
  }
  if(Now-LeadWaitSince>18){EndLeadScene(FVector::Dist2D(P,LeadPlayerAnchor)>65 && !LeadLooked?TEXT("lookback_unavailable"):TEXT("no_follow"));return;}
  if(I->IsAnyMontagePlaying() || C->PendingLivelyPat){CancelLeadScene(TEXT("player_interaction_took_over"));return;}
  const bool Followed=Dist<330 && FVector::Dist2D(P,LeadPlayerAnchor)>65 && PlayerVisible();
  if(!Followed || !LeadLooked || Now-LeadWaitSince<2.5 || !Safe())return;
  if(FVector::Dist2D(Here,LeadGoal)>30){if(!BeginLeadLeg(Now))EndLeadScene(TEXT("next_leg_unavailable"));return;}
  // Each existing destination action remains honest about its intent. CountStars
  // only points into the sky; flower/fireplace visits are not claimed as hand contact.
  C->GazePoiUntil=Now;
  if(Play(LeadArrival,TEXT("LeadArrival"))){LeadPhase=4;TaskUntil=LeadExpires;UE_LOG(LogTemp,Display,TEXT("CONTENT35 lead_destination action=%s actual_error=%.2f"),*LeadArrival.ToString(),FVector::Dist2D(Here,LeadGoal));}
 }
 if(LeadPhase==4 && Now-TaskSince>1 && !I->IsAnyMontagePlaying())EndLeadScene(TEXT("destination_action_finished"));
}
