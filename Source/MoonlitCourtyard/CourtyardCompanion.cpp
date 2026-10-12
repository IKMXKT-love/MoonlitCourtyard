#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"

namespace {
void CompanionBool(UObject* A,FName N,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,V);}
double CompanionNumber(UObject* A,FName N,double Default){if(A)if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N))if(P->IsFloatingPoint())return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A));return Default;}
void CompanionTime(UObject* A,FName N,double V){if(A)if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N))if(P->IsFloatingPoint())P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),V);}
// Longest an unobserved scheduled moment waits for the player before it happens anyway.
constexpr double MaxUnseenWait=90;
}

// True when her face is on the player's screen: in front of the camera, near enough to read, not behind a wall.
bool ACourtyardController::PlayerSeesEmma() const {
 if(Page!=ECourtyardPage::Game || !PlayerCharacter || !ViewCamera)return false;
 // The player's own eyes, never a review camera that may be the current view target.
 FVector E,F;if(!FaceFrame(E,F))return false;const FVector V=ViewCamera->GetComponentLocation();const FRotator R=GetControlRotation();
 const FVector D=E-V;if(D.Size()>1200.f || FVector::DotProduct(R.Vector(),D.GetSafeNormal())<.82f)return false;
 FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardWatching),false,PlayerCharacter);Q.AddIgnoredActor(Emma());
 return !GetWorld()->LineTraceSingleByChannel(H,V,E,ECC_Visibility,Q);
}

// Where the player is relative to her facing (positive yaw = her right) and how far, from the player's own eyes.
// Same anatomical mesh frame as the gaze layer: right=-X, forward=+Y.
bool ACourtyardController::PlayerBearing(float& Yaw,float& Distance) const {
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;if(!M || !PlayerCharacter)return false;
 const FVector V=ViewCamera?ViewCamera->GetComponentLocation():PlayerCharacter->GetActorLocation();
 const FVector Delta=V-A->GetActorLocation();Distance=Delta.Size2D();
 const FVector D=M->GetComponentTransform().InverseTransformVectorNoScale(Delta.GetSafeNormal2D());
 Yaw=FMath::RadiansToDegrees(FMath::Atan2(-D.X,D.Y));return true;
}

// She stays near the player for a while: when they rest near her (up to three minutes), or stand close and keep
// her in view (up to 150 s, then she may leave once). Only while she can face them: holding her with her back
// to the player would keep her turned away.
bool ACourtyardController::KeepingCompany() const {
 const bool Resting=StaySince>=0 && GetWorld()->GetTimeSeconds()-StaySince<180. && PlayerInFront;
 return Resting || StandingCompany;
}

// Scheduled moments prefer to happen while the player is looking; unseen ones wait, up to a limit.
// Callers reset Since to -1 once their moment actually starts.
bool ACourtyardController::SeenOrWaited(double& Since) const {
 const double Now=GetWorld()->GetTimeSeconds();
 if(PlayerWatching){Since=-1;return true;}
 if(Since<0)Since=Now;
 return Now-Since>MaxUnseenWait;
}

bool ACourtyardController::PlayCompanion(UAnimMontage* Motion,int32 Code,const TCHAR* Why){
 const bool TerracePalmEntry=Code==737 && ContentLayer.IsValid() && ContentLayer->TerraceContactEnabled;
 const bool FlowerScentEntry=Code==738 && ContentLayer.IsValid() && ContentLayer->FlowerScentEnabled;
 if(ContentLayer.IsValid() && ContentLayer->BlocksOrdinaryBehavior() && Code!=723 && !TerracePalmEntry && !FlowerScentEntry)return false;
 const bool EmotionEntry=Code==723 && ContentLayer.IsValid() && ContentLayer->BlocksOrdinaryBehavior();
 // New opt-in seated sky action uses its scene's Safe() and actual-arrival
 // qualifications. A shared-sky partner need not be within headpat reach.
 // Original entry codes and their near-player/glance rules remain unchanged.
 const bool SeatedSkyEntry=Code==736 && ContentLayer.IsValid() && ContentLayer->SeatedSkyScenesEnabled && Pose==ECourtyardPose::Seated;
 auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;const double Now=GetWorld()->GetTimeSeconds();
 if(!Motion || !I || LivingTrip || LivingBehavior>=0 || ActiveAmbient || I->IsAnyMontagePlaying() || Page!=ECourtyardPage::Game
  || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction")) || (!SeatedSkyEntry && !TerracePalmEntry && !FlowerScentEntry && !QueryBool(A,TEXT("CanHeadPat")))
  || A->GetVelocity().Size()>2.f || (!EmotionEntry && !SeatedSkyEntry && !TerracePalmEntry && !FlowerScentEntry && (Now-AmbientStillSince<5. || CompanionNumber(A,TEXT("DueTime"),Now+100)-Now<9.
  || Now-CompanionNumber(A,TEXT("LastGlanceTime"),-100.)<10.)))return false;
 CompanionBool(A,TEXT("AllowInteraction"),false);
 if(I->Montage_Play(Motion,1.f,EMontagePlayReturnType::MontageLength,0.f,false)<=0.f){CompanionBool(A,TEXT("AllowInteraction"),true);return false;}
 ActiveAmbient=Motion;AmbientActor=A;AmbientBehavior=Code;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION start code=%d why=%s seen=%d pose=%d t=%.2f"),Code,Why,PlayerWatching,(int32)Pose,Now);
 return true;
}

// Called from the ambient scheduler once she is idle and the player is not already gazing at her.
bool ACourtyardController::TryCompanionReaction(){
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;if(!M || !PlayerCharacter)return false;
 const double Now=GetWorld()->GetTimeSeconds();
 const float Distance=FVector::Dist2D(PlayerCharacter->GetActorLocation(),A->GetActorLocation());
 // The player lies back or keeps looking up at the sky: she looks up with them, if her sky is open.
 // Supine P2 uses its own player-then-sky sequence. Standing sky sharing stays intact.
 if(!(LivelyStage>=2 && LivelyOn(TEXT("B6")) && Pose==ECourtyardPose::Supine) && Now>=NextSkyShare && LookUpSince>=0 && Now-LookUpSince>2.5 && Distance<900.f){
  FVector E,F;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardSkyShare),false,A);
  if(FaceFrame(E,F) && !GetWorld()->LineTraceSingleByChannel(H,E,E+(F*.9703f+FVector(0,0,.2419f))*5000.f,ECC_Visibility,Q)
   && !GetWorld()->LineTraceSingleByChannel(H,E,E+FVector(0,0,600),ECC_Visibility,Q) && PlayCompanion(SkyWatch,60,TEXT("player_stargazing"))){
   NextSkyShare=Now+FMath::FRandRange(100.f,160.f);return true;
  }
 }
 // The player sits near her and she is in view: a quiet, contented moment together.
 if(Pose==ECourtyardPose::Seated && Now>=NextSeatQuiet && Now-PoseSince>8 && Distance<360.f && PlayerWatching && AttentionSince<0
  && PlayCompanion(QuietEnjoy,61,TEXT("seated_together"))){NextSeatQuiet=Now+FMath::FRandRange(120.f,180.f);return true;}
 // The player crouches nearby: she looks down at the flowers in front of her too.
 if(Pose==ECourtyardPose::Crouched && Now>=NextCrouchFlower && Now-PoseSince>2 && Distance<450.f){
  const int32 Flower=FlowerContext();
  if(Flower!=INDEX_NONE && TryAmbientBehavior(Flower)){
   NextCrouchFlower=Now+FMath::FRandRange(90.f,140.f);AmbientDefer=-1;
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION start code=%d why=player_crouched seen=%d t=%.2f"),Flower,PlayerWatching,Now);return true;
  }
 }
 return false;
}

// Review only: aim the player's own view at her eyes, offset sideways by YawOffset degrees.
void ACourtyardController::CourtyardReviewLookAtEmma(float YawOffset){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || Page!=ECourtyardPage::Game)return;
 FVector E,F,V;FRotator R;if(!FaceFrame(E,F))return;GetPlayerViewPoint(V,R);
 FRotator Aim=(E-V).Rotation();Aim.Yaw+=YawOffset;Aim.Roll=0;SetControlRotation(Aim);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION review_look offset=%.1f yaw=%.1f pitch=%.1f distance=%.1f"),YawOffset,Aim.Yaw,Aim.Pitch,(E-V).Size());
}

// Review only: show one of her morph targets (index into the mesh's own list) at Value; -1 clears.
// UE 5.7 component entries override same-name animation curves; hold neutral pose for isolated review.
void ACourtyardController::CourtyardReviewMorph(int32 Index,float Value){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    FaceProbe=0;FaceManualPaused=true;CancelFace();ClearLivelyFace();
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;if(!M || !M->GetSkeletalMeshAsset())return;
 const auto& Targets=M->GetSkeletalMeshAsset()->GetMorphTargets();
 M->ClearMorphTargets();
 FName Name=NAME_None;
 if(Targets.IsValidIndex(Index) && Targets[Index]){Name=Targets[Index]->GetFName();M->SetMorphTarget(Name,FMath::Clamp(Value,0.f,1.f),false);}
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_MORPH index=%d name=%s value=%.2f count=%d"),Index,*Name.ToString(),Value,Targets.Num());
}

void ACourtyardController::TickCompanion(){
 const double Now=GetWorld()->GetTimeSeconds();
 if(Pose!=LastPose){
  LastPose=Pose;PoseSince=Now;
  // She notices the player settling onto the bench and turns to them soon after.
  if(Pose==ECourtyardPose::Seated){NextCompanion=FMath::Min(NextCompanion,Now+3.);UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION player_seated t=%.2f"),Now);}
 }
 if(Now<NextWatchCheck)return;NextWatchCheck=Now+.2;
 const bool Seen=PlayerSeesEmma();
 if(Seen!=PlayerWatching)UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION watching=%d t=%.2f"),Seen,Now);
 PlayerWatching=Seen;
 if(Seen){if(WatchedSince<0)WatchedSince=Now;}else WatchedSince=-1;
 const bool Up=Page==ECourtyardPage::Game && !PoseTransition && (Pose==ECourtyardPose::Supine || FRotator::NormalizeAxis(GetControlRotation().Pitch)>25.f);
 if(Up){if(LookUpSince<0)LookUpSince=Now;}else LookUpSince=-1;
 auto* A=Cast<ACharacter>(Emma());
 const bool Resting=A && PlayerCharacter && Pose!=ECourtyardPose::Standing && !PoseTransition
  && FVector::Dist2D(PlayerCharacter->GetActorLocation(),A->GetActorLocation())<900.f;
 if(Resting){if(StaySince<0){StaySince=Now;UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION player_resting_nearby pose=%d t=%.2f"),(int32)Pose,Now);}}else StaySince=-1;
 // Presence (S2): who is near her, and on which side.
 float BearingYaw=0,BearingDistance=0;const bool Bearing=PlayerBearing(BearingYaw,BearingDistance);
 PlayerInFront=Bearing && FMath::Abs(BearingYaw)<=100.f;
 if(Seen)LastWatchedAt=Now;
 const bool Traveling=A && BPBool(A,TEXT("TravelActive"));
 if(Traveling){
  if(CompanySince>=0 || CompanyCapped)UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION standing_company reset=travel t=%.2f"),Now);
  CompanySince=-1;CompanyCapped=false;DepartLookDone=false;
 }
 // Short pauses between walk segments are not a stop: company only begins once she has stood still for 1 s.
 if(Traveling || (A && A->GetVelocity().Size()>2.f))CompanionMovedAt=Now;
 const bool Settled=Now-CompanionMovedAt>=1.;
 const bool StandingNear=Bearing && Pose==ECourtyardPose::Standing && !PoseTransition && BearingDistance<400.f && PlayerInFront && Now-LastWatchedAt<10.;
 if(StandingNear && !CompanyCapped && !Traveling && (CompanySince>=0 || Settled)){
  if(CompanySince<0){CompanySince=Now;UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION standing_company begin distance=%.0f yaw=%.0f t=%.2f"),BearingDistance,BearingYaw,Now);}
  if(Now-CompanySince>=150.){
   CompanyCapped=true;
   // A trip held back by the company should not leave the very moment the hold ends.
   NextReading=FMath::Max(NextReading,Now+FMath::FRandRange(15.,45.));
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION standing_company cap=150 next_trip_in=%.0f t=%.2f"),NextReading-Now,Now);
  }
 }else if(!StandingNear && CompanySince>=0 && !CompanyCapped){CompanySince=-1;UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION standing_company end t=%.2f"),Now);}
 StandingCompany=CompanySince>=0 && !CompanyCapped;
 // The player is close behind her and keeps her company there: staying would keep her back turned.
 const bool Behind=Bearing && BearingDistance<600.f && FMath::Abs(BearingYaw)>110.f && (Resting || Now-LastWatchedAt<10.);
 if(Behind){if(BehindSince<0)BehindSince=Now;}else BehindSince=-1;
 // Her own walks between the three stops also wait until the player can see her leave.
 if(!A || LivingTrip || ActiveAmbient || !BPBool(A,TEXT("ActivityEnabled")) || Traveling || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction"))){
  if(Traveling)RouteDefer=-1;return;
 }
 const double Due=CompanionNumber(A,TEXT("DueTime"),Now+100);
 // While the player keeps her company she stays (for a while), keeping room for her reactions to them.
 if(KeepingCompany() && Due-Now<9.5 && Due-Now>-3.){
  CompanionTime(A,TEXT("DueTime"),Now+30.);UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION route_stays_with_player standing=%d t=%.2f"),StandingCompany,Now);
 }
 // Close behind her for a while: her next walk comes sooner, so she does not stand with her back to them.
 else if(BehindSince>=0 && Now-BehindSince>12. && Now-LastBehindNudge>40. && Due-Now>4.){
  CompanionTime(A,TEXT("DueTime"),Now+3.);LastBehindNudge=Now;
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION route_sooner player_behind yaw=%.0f distance=%.0f t=%.2f"),BearingYaw,BearingDistance,Now);
 }
 // About to leave with the player close in front: she looks at them and smiles first.
 else if(Due-Now<1. && Due-Now>-3. && !DepartLookDone && PlayerInFront && BearingDistance<600.f && GazeEnabled){
  DepartLookDone=true;CompanionTime(A,TEXT("DueTime"),Now+1.6);GazeDepartLookUntil=Now+1.6;
  FLivelyFacePhase Smile;Smile.Start=.1f;Smile.End=1.4f;Smile.Fade=.25f;Smile.Values={{45,.3f},{18,.3f}};StartFace(TEXT("GazeDepart"),{Smile},1.6f,TEXT(""),1);  // no generic #24 (expression remap batch 3)
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION depart_look yaw=%.0f distance=%.0f t=%.2f"),BearingYaw,BearingDistance,Now);
 }
 else if(Due-Now<1. && Due-Now>-3. && !SeenOrWaited(RouteDefer)){
  CompanionTime(A,TEXT("DueTime"),Now+2.5);
  if(Now-RouteDefer<.25)UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION route_waits_for_player t=%.2f"),Now);
 }
}
