#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentEnvironment.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
namespace {
void QuietDue(AActor* A,double Time){if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),TEXT("DueTime")))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),Time);}
double QuietNumber(AActor* A,const TCHAR* Name,double Default){if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),Name))return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A));return Default;}
}

// An occasional deliberate visit, separate from the old roaming owner. Use its
// existing root-motion walk and approach heading, never a stationary yaw snap.
bool ACourtyardContentLife::BeginQuietVisit(double Now,FName RequestedSite)
{
 if(!QuietVisitsEnabled || !QuietSitesEnabled || !FixEnabled || !Safe() || OwnsTravel || FloorJourney || QuietMotion || !Emotion.IsNone() || !ActiveTask.IsNone())return false;
 const FVector Here=C->Emma()->GetActorLocation();
 if(FMath::Abs(Here.Z-82.4f)>12)return false;
 struct FVisit {FName Site;FVector Goal;float Yaw;};
 TArray<FVisit> Visits={
  {TEXT("PondWatch"),FVector(0,-1000,82.4),-90},
  {TEXT("PondWatch"),FVector(0,-1820,82.4),90},
  {TEXT("PondWatch"),FVector(420,-1410,82.4),180},
  {TEXT("PondWatch"),FVector(-420,-1410,82.4),0},
  {TEXT("TerracePause"),FVector(0,260,82.4),-90}
 };
 if(FlowerBedPauseEnabled){
  Visits.Add({TEXT("FlowerBedPause"),FlowerBedPauseDock,float((FlowerBedPauseFocus-FlowerBedPauseDock).Rotation().Yaw)});
 }
 if(HearthPauseEnabled){
  Visits.Add({TEXT("HearthWarmPause"),HearthPauseDock,float((HearthPauseFocus-HearthPauseDock).Rotation().Yaw)});
 }
 if(WindowPauseEnabled){
  Visits.Add({TEXT("WindowPause"),WindowPauseDock,float((WindowPauseFocus-WindowPauseDock).Rotation().Yaw)});
 }
 if(QuietFlowerVisitsEnabled && Environment.IsValid()){
  const FVector Goal=Environment->TreeCenter+FVector(430,150,82.4);
  const FVector Focus=Environment->TreeCenter+FVector(170,60,115);
  Visits.Add({TEXT("BlossomPause"),Goal,float((Focus-Goal).Rotation().Yaw)});
 }
 TArray<int32> Available;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(QuietVisitSpace),false,C->Emma());
 for(int32 K=0;K<Visits.Num();++K){
  const auto& V=Visits[K];const float D=FVector::Dist2D(Here,V.Goal);
  if(!RequestedSite.IsNone() && V.Site!=RequestedSite)continue;
  if(D<250 || D>2600 || Cooldowns.FindRef(V.Site)>Now)continue;
  if(C->GetPawn() && FVector::Dist2D(C->GetPawn()->GetActorLocation(),V.Goal)<160)continue;
  if(GetWorld()->OverlapBlockingTestByChannel(V.Goal,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q))continue;
  Available.Add(K);
 }
 if(!Available.Num())return false;
 // Prefer a different kind of pause when both kinds are currently possible.
 TArray<int32> Different;for(int32 K:Available)if(Visits[K].Site!=LastQuietVisitSite)Different.Add(K);
 const auto& Choices=Different.Num()?Different:Available;
 const FVisit& V=Visits[Choices[Random.RandRange(0,Choices.Num()-1)]];
 if(!Travel(V.Goal,V.Yaw,TEXT("QuietVisit"),NAME_None,0))return false;
 QuietVisitPending=true;QuietVisitGoal=V.Goal;QuietVisitSite=V.Site;QuietArrivalUntil=0;
 QuietLastProgress=Here;QuietProgressAt=Now;
 UE_LOG(LogTemp,Display,TEXT("CONTENT30 quiet_visit_start site=%s goal=%s facing=%.1f t=%.2f"),*V.Site.ToString(),*V.Goal.ToString(),V.Yaw,Now);
 return true;
}
void ACourtyardContentLife::FinishQuietVisit(const TCHAR* Reason)
{
 if(!QuietVisitPending)return;
 if(QuietArrivalOwnsPause && C.IsValid()){
  if(EmotionOwnsPause)EmotionOldPause=QuietArrivalOldPause;else C->LocoLifePaused=QuietArrivalOldPause;
  C->LocoNextDecision=FMath::Max(C->LocoNextDecision,double(GetWorld()->GetTimeSeconds()+8));
 }
 UE_LOG(LogTemp,Display,TEXT("CONTENT30 quiet_visit_end site=%s reason=%s position=%s"),*QuietVisitSite.ToString(),Reason,C.IsValid()&&C->Emma()?*C->Emma()->GetActorLocation().ToString():TEXT("missing"));
 QuietVisitPending=false;QuietArrivalOwnsPause=false;QuietArrivalUntil=0;
}

// Non-contact alternatives only. Reuse the existing relaxed observation clip;
// no hand reaches are added or counted as touching the water or the balustrade.
bool ACourtyardContentLife::BeginQuietSite()
{
 if(!QuietSitesEnabled || !FixEnabled || !Safe() || OwnsTravel || FloorJourney || !Emotion.IsNone() || QuietMotion)return false;
 const FVector P=C->Emma()->GetActorLocation();
 if(FMath::Abs(P.Z-82.4f)>12)return false;
 FName Site;FVector Focus;
 const float WaterDistance=FVector::Dist2D(P,FVector(0,-1410,0));
 if(WaterDistance>335 && WaterDistance<470){Site=TEXT("PondWatch");Focus=FVector(0,-1410,105);}
 else if(FVector::Dist2D(P,FVector(0,260,0))<95){Site=TEXT("TerracePause");Focus=FVector(0,-550,145);}
 else if(FlowerBedPauseEnabled && FVector::Dist2D(P,FlowerBedPauseDock)<95){Site=TEXT("FlowerBedPause");Focus=FlowerBedPauseFocus;}
 else if(HearthPauseEnabled && FVector::Dist2D(P,HearthPauseDock)<95){Site=TEXT("HearthWarmPause");Focus=HearthPauseFocus;}
 else if(WindowPauseEnabled && FVector::Dist2D(P,WindowPauseDock)<95){Site=TEXT("WindowPause");Focus=WindowPauseFocus;}
 else if(QuietFlowerVisitsEnabled && Environment.IsValid() && FVector::Dist2D(P,Environment->TreeCenter+FVector(430,150,82.4))<95){
  Site=TEXT("BlossomPause");Focus=Environment->TreeCenter+FVector(170,60,115);
 }
 else return false;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Cooldowns.FindRef(Site)>Now || FVector::DotProduct(C->Emma()->GetActorForwardVector(),(Focus-P).GetSafeNormal2D())<.82f)return false;
 auto* A=Cast<ACharacter>(C->Emma());const FVector Head=A->GetMesh()->GetSocketLocation(TEXT("頭"));
 FCollisionQueryParams Q(SCENE_QUERY_STAT(QuietSiteVisibility),false,A);FHitResult Hit;
 if(GetWorld()->LineTraceSingleByChannel(Hit,Head,Focus,ECC_Visibility,Q) && Hit.Distance<(Focus-Head).Size()-65)return false;
 const FName Motion=Site==TEXT("HearthWarmPause")?TEXT("WarmHands"):Site==TEXT("PondWatch")?TEXT("PondObserve"):Site==TEXT("FlowerBedPause")?TEXT("FlowerBedObserve"):Site==TEXT("BlossomPause")?TEXT("BlossomListen"):TEXT("WindowReflect");
 if(!Play(Motion,Site))return false;
 QuietSite=Site;QuietFocus=Focus;QuietMotion=Motions.FindRef(Motion);QuietOldPause=C->LocoLifePaused;C->LocoLifePaused=true;
 Cooldowns.Add(Site,Now+Random.FRandRange(210,360));
 UE_LOG(LogTemp,Display,TEXT("CONTENT28 quiet_site_start site=%s position=%s no_contact=1"),*Site.ToString(),*P.ToString());return true;
}
void ACourtyardContentLife::TickQuietSites(double Now)
{
 if(QuietVisitPending){
  if(!C.IsValid() || !C->Emma() || !Emotion.IsNone()){FinishQuietVisit(TEXT("emotion_or_missing_owner"));return;}
  if(OwnsTravel){
   if(C->LocoState!=ACourtyardController::ELocoState::Idle){
    const FVector Here=C->Emma()->GetActorLocation();
    bool PlayerBlocks=false;
    if(C->GetPawn() && C->GetPawn()->GetVelocity().Size2D()<60){
     for(float S=20;S<=110 && !PlayerBlocks;S+=15)
      PlayerBlocks=FVector::Dist2D(C->LocoPointAt(C->LocoProgress+S),C->GetPawn()->GetActorLocation())<38;
    }
    if(FVector::Dist2D(Here,QuietLastProgress)>3){QuietLastProgress=Here;QuietProgressAt=Now;}
    const bool Stalled=Now-TaskSince>3 && Now-QuietProgressAt>1.5 && C->Emma()->GetVelocity().Size2D()<5;
    if((PlayerBlocks || Stalled) && !C->LocoStopRequested){
     C->LocoStopRequested=true;
     UE_LOG(LogTemp,Display,TEXT("CONTENT30 quiet_visit_stop reason=%s"),PlayerBlocks?TEXT("player_on_path"):TEXT("no_progress"));
    }
    return;
   }
   if(Now-TaskSince<1)return;
   const bool Arrived=C->LocoEndArrived && FVector::Dist2D(C->Emma()->GetActorLocation(),QuietVisitGoal)<30;
   ReleaseTravel();ActiveTask=ArriveAction=NAME_None;
   if(!Arrived){FinishQuietVisit(TEXT("interrupted_or_short"));return;}
   QuietArrivalOldPause=C->LocoLifePaused;QuietArrivalOwnsPause=true;C->LocoLifePaused=true;
   QuietArrivalUntil=Now+14;
   // PlayCompanion also checks the blueprint's departure time. Our deliberate
   // stay owns this deadline; the old roaming scheduler resumes afterward.
   QuietDue(C->Emma(),Now+30);QuietReportAt=Now+5;
   UE_LOG(LogTemp,Display,TEXT("CONTENT30 quiet_visit_arrived site=%s position=%s yaw=%.1f"),*QuietVisitSite.ToString(),*C->Emma()->GetActorLocation().ToString(),C->Emma()->GetActorRotation().Yaw);
  }
  if(QuietArrivalUntil>0){
   if(Now>QuietArrivalUntil){FinishQuietVisit(TEXT("observation_qualification_expired"));return;}
   // Yield to a real headpat or other action and retry within this finite settle
   // window. Releasing our pause before Play transfers the old state correctly.
   if(C->Page==ECourtyardPage::Game && Safe()){
    C->LocoLifePaused=QuietArrivalOldPause;
    if(BeginQuietSite()){
     LastQuietVisitSite=QuietVisitSite;QuietArrivalOwnsPause=false;
     FinishQuietVisit(TEXT("observation_started"));return;
    }
    C->LocoLifePaused=true;
   }
   if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) && Now>=QuietReportAt){
    QuietReportAt=Now+4;
    auto* A=Cast<ACharacter>(C->Emma());auto* I=A->GetMesh()->GetAnimInstance();
    double Due=0;if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),TEXT("DueTime")))Due=P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A));
    const FVector Focus=QuietVisitSite==TEXT("PondWatch")?FVector(0,-1410,105):QuietVisitSite==TEXT("FlowerBedPause")?FlowerBedPauseFocus:QuietVisitSite==TEXT("HearthWarmPause")?HearthPauseFocus:QuietVisitSite==TEXT("WindowPause")?WindowPauseFocus:QuietVisitSite==TEXT("BlossomPause") && Environment.IsValid()?Environment->TreeCenter+FVector(170,60,115):FVector(0,-550,145);
    const FVector Head=A->GetMesh()->GetSocketLocation(TEXT("頭"));FCollisionQueryParams Q(SCENE_QUERY_STAT(QuietVisitProbe),false,A);FHitResult H;
    const bool Hit=GetWorld()->LineTraceSingleByChannel(H,Head,Focus,ECC_Visibility,Q);
    UE_LOG(LogTemp,Display,TEXT("CONTENT30 quiet_qualification safe=%d can_pat=%d montage=%s due_left=%.2f still=%.2f glance_age=%.2f dot=%.3f visibility_hit=%s hit_distance=%.1f target_distance=%.1f"),Safe(),C->QueryBool(A,TEXT("CanHeadPat")),*GetNameSafe(I?I->GetCurrentActiveMontage():nullptr),Due-Now,Now-C->AmbientStillSince,Now-QuietNumber(A,TEXT("LastGlanceTime"),-100),FVector::DotProduct(A->GetActorForwardVector(),(Focus-A->GetActorLocation()).GetSafeNormal2D()),Hit?*GetNameSafe(H.GetActor()):TEXT("none"),H.Distance,(Focus-Head).Size());
   }
  }else FinishQuietVisit(TEXT("travel_cancelled"));
  return;
 }
 if(QuietMotion){
  auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  if(I && I->Montage_IsActive(QuietMotion) && Emotion.IsNone()){
   C->GazePoi=QuietFocus;C->GazePoiUntil=Now+.25;C->GazePoiHead=.30f;return;
  }
  if(C.IsValid()){
   // Emotion ownership may have arrived during this quiet pause; restore its
   // saved base state without releasing the emotion's own movement lock.
   if(EmotionOwnsPause)EmotionOldPause=QuietOldPause;else C->LocoLifePaused=QuietOldPause;
   C->GazePoiUntil=0;C->LocoNextDecision=FMath::Max(C->LocoNextDecision,Now+8);
  }
  UE_LOG(LogTemp,Display,TEXT("CONTENT28 quiet_site_end site=%s emotion=%s"),*QuietSite.ToString(),*Emotion.ToString());
  QuietMotion=nullptr;QuietSite=NAME_None;return;
 }
 if(!QuietSitesEnabled || C->Page!=ECourtyardPage::Game)return;
 if(WindowPauseEnabled && ReviewWindowUntil>0){
  if(Now>ReviewWindowUntil){ReviewWindowUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT44 window_review_timeout"));}
  else if(Safe() && !OwnsTravel && !FloorJourney && Emotion.IsNone() && ActiveTask.IsNone()){
   ReviewWindowUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT44 window_review_request accepted=%d one_attempt=1 no_contact=1"),BeginQuietVisit(Now,TEXT("WindowPause")));
  }
  return;
 }
 if(HearthPauseEnabled && ReviewHearthUntil>0){
  if(Now>ReviewHearthUntil){ReviewHearthUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT43 hearth_review_timeout"));}
  else if(Safe() && !OwnsTravel && !FloorJourney && Emotion.IsNone() && ActiveTask.IsNone()){
   ReviewHearthUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT43 hearth_review_request accepted=%d one_attempt=1 no_contact=1"),BeginQuietVisit(Now,TEXT("HearthWarmPause")));
  }
  return;
 }
 if(FlowerBedPauseEnabled && ReviewFlowerBedUntil>0){
  if(Now>ReviewFlowerBedUntil){ReviewFlowerBedUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT42 flower_pause_review_timeout"));}
  else if(Safe() && !OwnsTravel && !FloorJourney && Emotion.IsNone() && ActiveTask.IsNone()){
   ReviewFlowerBedUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT42 flower_pause_review_request accepted=%d one_attempt=1 no_contact=1"),BeginQuietVisit(Now,TEXT("FlowerBedPause")));
  }
  return;
 }
 if(QuietVisitsEnabled){
  if(NextQuietVisit<=0)NextQuietVisit=Now+Random.FRandRange(90,150);
  if(Now>=NextQuietVisit){
   NextQuietVisit=Now+Random.FRandRange(20,40);
   if(BeginQuietVisit(Now)){NextQuietVisit=Now+Random.FRandRange(180,330);return;}
  }
 }
 if(NextQuietSite<=0){NextQuietSite=Now+Random.FRandRange(30,55);return;}
 if(Now<NextQuietSite)return;
 NextQuietSite=Now+Random.FRandRange(35,65);
 BeginQuietSite();
}

bool ACourtyardContentLife::BeginStudyBook()
{
 if(!FixEnabled || StudyMotion || !Safe())return false;
 auto* A=Cast<ACharacter>(C->Emma());
 if(FVector::Dist(A->GetActorLocation(),FVector(995,890,528.4))>3 || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,180))>2)return false;
 for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("Content25StudyBook"))){StudyBook=*It;break;}
 for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("Content25StudyPage"))){StudyPage=*It;break;}
 if(!StudyBook.IsValid() || !Play(TEXT("StudyBook"),TEXT("StudyBook")))return false;
 StudyMotion=Motions.FindRef(TEXT("StudyBook"));StudyRest=StudyBook->GetActorTransform();StudySince=GetWorld()->GetTimeSeconds();StudyPhase=0;StudyTransferred=false;StudyOldPause=C->LocoLifePaused;C->LocoLifePaused=true;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX study_book_begin"));return true;
}
bool ACourtyardContentLife::TickStudyBook(double Now)
{
 if(!StudyMotion)return false;
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
 if(!I || !StudyBook.IsValid()){FinishStudyBook(TEXT("missing_owner"));return true;}
 if(!I->Montage_IsActive(StudyMotion)){FinishStudyBook(StudyPhase>StudyMotion->GetPlayLength()-.6f?TEXT("returned"):TEXT("interrupted"));return true;}
 StudyPhase=I->Montage_GetPosition(StudyMotion);
 // Book stays on the real desk while hands approach. During the lift its position
 // follows the authored left-hand translation; the desk remains the return anchor.
 if(StudyPhase>=2.4f && StudyPhase<=15.5f){
  StudyTransferred=true;
  const FVector LocalW=A->GetMesh()->GetComponentTransform().InverseTransformPosition(A->GetMesh()->GetSocketLocation(TEXT("手首_L")));
  FTransform Pose=StudyRest;Pose.SetLocation(StudyRest.GetLocation()+A->GetMesh()->GetComponentTransform().TransformVectorNoScale(LocalW-FVector(12,24,88.2)));
  StudyBook->SetActorTransform(Pose,false);
 }else if(StudyTransferred){StudyBook->SetActorTransform(StudyRest,false);StudyTransferred=false;}
 if(StudyPage.IsValid()){
  const float U=FMath::Clamp((StudyPhase-5.8f)/2.2f,0.f,1.f);
  const float Ease=U*U*U*(10-15*U+6*U*U),Angle=PI*Ease;
  const FTransform Local(FQuat(FVector::YAxisVector,Angle),FVector(-4.4f*FMath::Cos(Angle),0,2.18f+4.4f*FMath::Sin(Angle)),FVector(.088f,.135f,.00015f));
  StudyPage->SetActorTransform(Local*StudyBook->GetActorTransform(),false);
  StudyPage->SetActorHiddenInGame(StudyPhase<5.8f || StudyPhase>8.5f);
 }
 return true;
}
void ACourtyardContentLife::FinishStudyBook(const TCHAR* Reason)
{
 if(StudyBook.IsValid())StudyBook->SetActorTransform(StudyRest,false);
 if(StudyPage.IsValid())StudyPage->SetActorHiddenInGame(true);
 if(C.IsValid())C->LocoLifePaused=StudyOldPause;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX study_book_end reason=%s phase=%.3f"),Reason,StudyPhase);
 StudyMotion=nullptr;StudyBook.Reset();ActiveTask=NAME_None;
}
