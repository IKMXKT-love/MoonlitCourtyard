#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

bool ACourtyardContentLife::BeginFloorJourney()
{
 if(!FixEnabled || !StairsValidated || FloorJourney || OwnsTravel || !Safe() || !Emotion.IsNone())return false;
 if(C->Emma()->GetActorLocation().Z>160)return false;
 FloorOldPause=C->LocoLifePaused;C->LocoLifePaused=true;
 if(!Travel(FVector(102,1530,82.4),90,TEXT("FloorEntry"),TEXT("FloorUpDock"))){C->LocoLifePaused=FloorOldPause;return false;}
 FloorJourney=1;FloorDue=GetWorld()->GetTimeSeconds()+110;
 FloorFailures=0;FloorResumePhase=0;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_journey begin normal_walk=1"));return true;
}
void ACourtyardContentLife::EndFloorJourney(const TCHAR* Reason)
{
 if(FMath::Abs(C->Emma()->GetActorLocation().Z-528.4)<6 && FloorJourney!=3 && FloorJourney!=10){
  ReleaseTravel();ArriveAction=ActiveTask=NAME_None;C->LocoLifePaused=true;
  FloorResumePhase=(FloorJourney==3 || FloorJourney==4 || FloorJourney==5)?4:6;
  ++FloorFailures;FloorJourney=FloorFailures>=2?14:FloorResumePhase;
  FloorDue=GetWorld()->GetTimeSeconds()+Random.FRandRange(20,35);
  FloorWaitPlayerPosition=C->GetPawn()?C->GetPawn()->GetActorLocation():FVector::ZeroVector;
  if(FloorJourney==14)UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX floor_wait_for_scene_change failures=%d timed_retry_disabled=1"),FloorFailures);
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_journey upstairs_wait reason=%s return_after=%.2f"),Reason,FloorDue);return;
 }
 if(FloorJourney==3 || FloorJourney==10 || C->Emma()->GetActorLocation().Z>160){
  // Never release a stranded character into ordinary flat-ground travel.
  ReleaseTravel();ArriveAction=ActiveTask=NAME_None;C->LocoLifePaused=true;FloorJourney=15;
  UE_LOG(LogTemp,Error,TEXT("CONTENT_FIX floor_recovery_required reason=%s position=%s ordinary_travel_disabled=1"),Reason,*C->Emma()->GetActorLocation().ToString());return;
 }
 ReleaseTravel();ArriveAction=ActiveTask=NAME_None;C->LocoLifePaused=FloorOldPause;FloorJourney=0;NextFloorVisit=GetWorld()->GetTimeSeconds()+Random.FRandRange(300,600);
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_journey end=%s position=%s"),Reason,*C->Emma()->GetActorLocation().ToString());
}
void ACourtyardContentLife::TickFloorJourney(double Now)
{
 if(!FixEnabled || !StairsValidated)return;
 if(!FloorJourney){
  if(NextFloorVisit<=0)NextFloorVisit=Now+Random.FRandRange(180,300);
  if(Now>NextFloorVisit && Safe() && C->LocoIndoors() && C->Emma()->GetActorLocation().Z<160){NextFloorVisit=Now+180;BeginFloorJourney();}
  return;
 }
 if(OwnsTravel || FixedStairMotion)return;
 if(!Emotion.IsNone())return;
 if(FloorJourney==15)return; // An unexpected loss of the retained stair clip is a safe-stop fault.
 if(FloorJourney==14){
  // A blocked room is not a reason to replay the same failed route indefinitely.
  // Only a real change in the nearby player's position permits another attempt.
  if(!C->GetPawn() || Now<FloorDue || FVector::Dist2D(C->GetPawn()->GetActorLocation(),FloorWaitPlayerPosition)<180)return;
  FloorWaitPlayerPosition=C->GetPawn()->GetActorLocation();FloorDue=Now+30;
  FloorJourney=FloorResumePhase;UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_retry_after_scene_change failures=%d"),FloorFailures);
 }
 if(FloorJourney==13){EndFloorJourney(TEXT("returned"));return;}
 if((FloorJourney==4 || FloorJourney==6) && Now<FloorDue)return;
 if(!Safe())return;
 if(FloorJourney==1 || FloorJourney==5 || FloorJourney==8 || FloorJourney==12){EndFloorJourney(TEXT("walk_interrupted"));return;}
 if(FloorJourney==2 || FloorJourney==9){
  const bool Down=FloorJourney==9;
  if(BeginFixedStairs(Down))FloorJourney=Down?10:3;else EndFloorJourney(TEXT("stair_dock_not_ready"));return;
 }
 if(FloorJourney==4){
  if(Travel(FVector(-400,1200,528.4),-90,TEXT("FloorGallery"),TEXT("FloorRest")))FloorJourney=5;else EndFloorJourney(TEXT("gallery_unreachable"));return;
 }
 if(FloorJourney==6){
  if(Travel(FVector(-102,1574,528.4),90,TEXT("FloorReturn"),TEXT("FloorDownDock")))FloorJourney=8;else EndFloorJourney(TEXT("down_dock_unreachable"));return;
 }
 if(FloorJourney==11){
  if(Travel(FVector(102,1240,82.4),-90,TEXT("FloorExit"),TEXT("FloorExit")))FloorJourney=12;else EndFloorJourney(TEXT("exit_unreachable"));
 }
}
