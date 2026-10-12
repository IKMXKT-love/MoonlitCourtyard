#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentEnvironment.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

// New opt-in seated context. The player's accepted fixed-seat capsule remains
// at its approach point: choose the meeting place from the actual viewpoint,
// but keep the capsule anchor for context cancellation. No seat or gait edits.
void ACourtyardContentLife::TickSeatedSharedSky(double Now)
{
 if(!SeatedSkyScenesEnabled || !C.IsValid() || !C->PlayerCameraManager || !C->GetPawn() || !C->Emma() ||
    OwnsTravel || BlocksOrdinaryBehavior() || !Emotion.IsNone() || C->Page!=ECourtyardPage::Game)return;
 auto* Emma=C->Emma();
 const FVector Anchor=C->GetPawn()->GetActorLocation(),E=Emma->GetActorLocation();
 FVector View=C->PlayerCameraManager->GetCameraLocation();View.Z=E.Z;
 const float Yaw=C->GetControlRotation().Yaw,Distance=FVector::Dist2D(View,E);
 if(C->Pose!=ECourtyardPose::Seated || C->GetControlRotation().Pitch<=26 || C->GetPawn()->GetVelocity().Size2D()>=8 ||
    FMath::Abs(Anchor.Z-E.Z)>=100 || Distance>650 || !PlayerVisible() || !SharedSkyVisible())return;
 if(SharedSkyWindowUntil>Now && Environment.IsValid() && Environment->MeteorBrightness>.12f &&
    Cooldowns.FindRef(TEXT("SharedSkyMeteor"))<Now && Safe()){
  const float Bearing=FMath::Abs(FMath::FindDeltaAngleDegrees(Emma->GetActorRotation().Yaw,Environment->MeteorDirection.Rotation().Yaw));
  if(Bearing<35 && Play(TEXT("InviteView"),TEXT("SharedSkyMeteor"))){Cooldowns.Add(TEXT("SharedSkyMeteor"),Now+90);UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky_meteor real_event=1 kind=%d bearing=%.1f"),Environment->MeteorKind,Bearing);return;}
 }
 if(StillSky<=18 || Cooldowns.FindRef(TEXT("SkyTogether"))>=Now || !Safe())return;
 const FVector Local=FRotator(0,Yaw,0).UnrotateVector(E-View);
 const float Heading=FMath::Abs(FMath::FindDeltaAngleDegrees(Emma->GetActorRotation().Yaw,Yaw));
 if(FMath::Abs(Local.X)<95 && FMath::Abs(Local.Y)>95 && Distance<225 && Heading<35){
  if(Play(TEXT("SkyTogether"),TEXT("SkyTogether"))){Cooldowns.Add(TEXT("SkyTogether"),Now+360);StillSky=0;SharedSkyWindowUntil=Now+45;UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky already_beside=1 distance=%.2f heading=%.2f"),Distance,Heading);}return;
 }
 FCollisionQueryParams Q(SCENE_QUERY_STAT(SeatedSkyPlace),false,Emma);
 FVector Goal=FVector::ZeroVector;float Best=TNumericLimits<float>::Max();
 for(const float Sign:{-1.f,1.f}){
  const FVector Candidate=View+FRotator(0,Yaw,0).RotateVector(FVector(0,170*Sign,0));
  FHitResult Ground;
  if(!GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector(0,0,60),Candidate-FVector(0,0,120),ECC_Visibility,Q) ||
     Ground.ImpactNormal.Z<.85f || FMath::Abs(Ground.ImpactPoint.Z-(E.Z-80))>25 ||
     GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q))continue;
  const float Cost=FVector::DistSquared2D(E,Candidate);if(Cost<Best){Best=Cost;Goal=Candidate;}
 }
 if(Best==TNumericLimits<float>::Max()){Cooldowns.Add(TEXT("SkyTogether"),Now+90);StillSky=0;UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky_skip no_clear_side=1 view=%s"),*View.ToString());return;}
 if(!Travel(Goal,Yaw,TEXT("SkyTogether"),TEXT("SkyTogether"))){Cooldowns.Add(TEXT("SkyTogether"),Now+90);StillSky=0;UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky_skip walk_unavailable=1"));return;}
 bool Clear=true;
 const float Length=C->LocoPathDist.IsEmpty()?0:C->LocoPathDist.Last();
 for(float Along=0;Clear && Along<=Length;Along+=35){
  const FVector Sample=C->LocoPointAt(FMath::Min(Along,Length));FHitResult Ground;
  Clear=GetWorld()->LineTraceSingleByChannel(Ground,Sample+FVector(0,0,60),Sample-FVector(0,0,120),ECC_Visibility,Q) &&
   Ground.ImpactNormal.Z>.85f && FMath::Abs(Ground.ImpactPoint.Z-(E.Z-80))<=25 &&
   !GetWorld()->OverlapBlockingTestByChannel(Sample,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q);
 }
 if(!Clear || Length>750){
  C->LocoEnd(false,TEXT("seated_sky_path_rejected"));ReleaseTravel();ActiveTask=ArriveAction=NAME_None;
  Cooldowns.Add(TEXT("SkyTogether"),Now+90);StillSky=0;
  UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky_skip actual_path_clear=%d length=%.1f"),Clear,Length);return;
 }
 SharedSkyGoal=Goal;SharedSkyYaw=Yaw;SharedSkyAnchor=Anchor;SharedSkyLastProgress=E;SharedSkyProgressAt=Now;
 SharedSkyTravelOwned=true;SharedSkyCancelled=false;Cooldowns.Add(TEXT("SkyTogether"),Now+360);StillSky=0;
 UE_LOG(LogTemp,Display,TEXT("CONTENT36 seated_sky_approach actual_view=%s root_anchor=%s side=%s path=%.1f real_walk=1"),*View.ToString(),*Anchor.ToString(),*Goal.ToString(),Length);
}
