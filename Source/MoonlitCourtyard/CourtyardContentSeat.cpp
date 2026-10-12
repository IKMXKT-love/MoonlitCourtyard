#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
namespace {void SeatFlag(AActor* A,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("AllowInteraction")))P->SetPropertyValue_InContainer(A,V);}}
FVector ACourtyardContentLife::SeatDock() const {return ReadingSeat?FVector(410,755,82.4):FVector(-155,FixEnabled?50:82,82.4);}
FVector ACourtyardContentLife::SeatEntry() const {return ReadingSeat?FVector(490,815,82.4):FVector(-95,-30,82.4);}
float ACourtyardContentLife::SeatYaw() const {return ReadingSeat?0.f:-90.f;}
bool ACourtyardContentLife::BeginReadingVisit()
{
 if(!ReadingSeatValidated || !ReadingSeatSource || FloorJourney || OwnsTravel || !Emotion.IsNone() || !Safe() || C->Emma()->GetActorLocation().Z>160)return false;
 ReadingSeat=true;
 if(!Travel(SeatEntry(),180,TEXT("FloorReadingApproach"),TEXT("SeatApproach"))){ReadingSeat=false;NextReadingSeat=GetWorld()->GetTimeSeconds()+60;return false;}
 NextReadingSeat=GetWorld()->GetTimeSeconds()+600;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX reading_visit begin real_walk=1"));return true;
}
bool ACourtyardContentLife::BeginSeatApproach()
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!FixEnabled || !A || !Safe(false) || SeatApproachMotion || SeatMotion)return false;
 if(FVector::Dist2D(A->GetActorLocation(),SeatEntry())>6 || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,SeatYaw()+180))>8)return false;
 auto* Motion=Motions.Find(TEXT("SeatApproach"));if(!Motion || !*Motion)return false;
 SeatApproachOldPause=C->LocoLifePaused;SeatApproachOldHold=C->ReviewBodyHold;C->LocoLifePaused=true;C->ReviewBodyHold=true;SeatFlag(A,false);
 SeatApproachMotion=*Motion;SeatApproachSince=GetWorld()->GetTimeSeconds();SeatApproachBlocked=0;SeatApproachLastPhase=0;SeatApproachLast=A->GetActorLocation();A->SetAnimRootMotionTranslationScale(1);
 if(A->GetMesh()->GetAnimInstance()->Montage_Play(SeatApproachMotion)<=0){EndSeatApproach(false);return false;}
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seat_approach_start pos=%s mode=%d"),*A->GetActorLocation().ToString(),int(A->GetCharacterMovement()->MovementMode));return true;
}
void ACourtyardContentLife::EndSeatApproach(bool Arrived)
{
 auto* A=Cast<ACharacter>(C->Emma());if(!A)return;
 C->LocoLifePaused=SeatApproachOldPause;C->ReviewBodyHold=SeatApproachOldHold;SeatFlag(A,true);SeatApproachMotion=nullptr;
 if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seat_approach_end nominal=%d pos=%s yaw=%.2f"),Arrived,*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw);
 if(Arrived)SeatReadyUntil=GetWorld()->GetTimeSeconds()+2;
 else {ReadingSeat=false;ActiveTask=NAME_None;}
}
bool ACourtyardContentLife::BeginSeat()
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
 auto* Source=ReadingSeat?ReadingSeatSource.Get():SeatSource.Get();
 if(!(ReadingSeat?ReadingSeatValidated:SeatingValidated) || !Source || !I || !Safe(false))return false;
 const FVector Dock=SeatDock();
 const float D=FVector::Dist2D(A->GetActorLocation(),Dock);const float Y=FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,SeatYaw()));
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT seat_dock distance=%.3f yaw=%.3f"),D,Y);if(D>5 || Y>8)return false;
 if(C->GetPawn() && FVector::Dist2D(C->GetPawn()->GetActorLocation(),Dock)<70)return false;
 SeatOldPause=C->LocoLifePaused;SeatOldHold=C->ReviewBodyHold;C->LocoLifePaused=true;C->ReviewBodyHold=true;SeatFlag(A,false);
 SeatMotion=DuplicateObject<UAnimMontage>(Source,this);SeatMotion->CompositeSections.Empty();
 const FName Names[]={TEXT("Enter"),TEXT("Rest"),TEXT("Exit")};const float Times[]={0,2.9f,6.9f};
 for(int J=0;J<3;++J){FCompositeSection S;S.SectionName=Names[J];S.NextSectionName=J<2?Names[1]:NAME_None;S.Link(SeatMotion,Times[J]);SeatMotion->CompositeSections.Add(S);}
 if(I->Montage_Play(SeatMotion)<=0){FinishSeat();return false;}
 SeatSince=GetWorld()->GetTimeSeconds();SeatDue=TNumericLimits<double>::Max();SeatExitQueued=false;SeatLastPhase=0;SeatRecoveryTried=SeatFault=false;C->CancelFace();ActiveTask=TEXT("BenchCompany");
 ReadingWaking=false;ReadingWakeAt=-1;
 if(ReadingSeat){SeatDue=SeatSince+Random.FRandRange(65,110);ActiveTask=TEXT("ReadingDoze");
  // Face ownership is composed with this candidate clip in ComposeFixedFace.
 }
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT seat_enter runtime=1 player_seated=%d"),C->Pose==ECourtyardPose::Seated);return true;
}
void ACourtyardContentLife::FinishSeat()
{
 if(C.IsValid()){C->LocoLifePaused=SeatOldPause;C->ReviewBodyHold=SeatOldHold;SeatFlag(C->Emma(),true);C->LocoNextDecision=GetWorld()->GetTimeSeconds()+25;C->AmbientStillSince=GetWorld()->GetTimeSeconds();C->CancelFace();}
 SeatMotion=nullptr;SeatFault=false;ReadingSeat=ReadingWaking=false;ActiveTask=NAME_None;NextSeat=GetWorld()->GetTimeSeconds()+360;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT seat_exit_complete"));
}
void ACourtyardContentLife::TickSeat(double Now)
{
 if(SeatReadyUntil>0){
  if(Safe(false)){SeatReadyUntil=-1;if(!BeginSeat()){ReadingSeat=false;ActiveTask=NAME_None;UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seat_enter_rejected dock_or_occupancy=1"));}}
  else if(Now>SeatReadyUntil){SeatReadyUntil=-1;ReadingSeat=false;ActiveTask=NAME_None;UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seat_enter_rejected recovery_timeout=1"));}
  return;
 }
 if(ReviewSeatCamera && (SeatMotion || SeatApproachMotion))C->CourtyardReviewBodyView(4);
 if(SeatApproachMotion){
  auto* A=Cast<ACharacter>(C->Emma());auto* I=A->GetMesh()->GetAnimInstance();
  if(!I->Montage_IsActive(SeatApproachMotion) && Now-SeatApproachSince>1){EndSeatApproach(SeatApproachLastPhase>=SeatApproachMotion->GetPlayLength()-.5f);return;}
  const float T=I->Montage_GetPosition(SeatApproachMotion),Dt=GetWorld()->GetDeltaSeconds();const float Distance=FVector::Dist2D(A->GetActorLocation(),SeatApproachLast);SeatApproachLast=A->GetActorLocation();
  SeatApproachLastPhase=T;
  if(T>1.2 && T<7.5 && Distance<Dt*1.5)SeatApproachBlocked+=Dt;else SeatApproachBlocked=0;
  if(SeatApproachBlocked>.8){I->Montage_Stop(.3,SeatApproachMotion);EndSeatApproach(false);UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX seat_approach_obstructed t=%.2f"),T);}return;
 }
 if(SeatMotion){
  auto* A=Cast<ACharacter>(C->Emma());auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  if(SeatFault)return;
  if(!I){SeatFault=true;UE_LOG(LogTemp,Error,TEXT("CONTENT_FIX seat_fault missing_anim_instance ordinary_travel_disabled=1"));return;}
  if(!I->Montage_IsActive(SeatMotion)){
   if(SeatExitQueued && SeatLastPhase>=SeatMotion->GetPlayLength()-.5f){FinishSeat();return;}
   // Restore only this interrupted clip at its retained phase once, then exit through
   // its authored stand-up. Never declare a seated character to be standing.
   if(!SeatRecoveryTried){
    SeatRecoveryTried=true;
    if(I->Montage_Play(SeatMotion,1,EMontagePlayReturnType::MontageLength,SeatLastPhase,false)>0){
     I->Montage_SetNextSection(TEXT("Rest"),TEXT("Exit"),SeatMotion);SeatExitQueued=true;
     UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX seat_resume_interrupted phase=%.3f exit_queued=1"),SeatLastPhase);return;
    }
   }
   SeatFault=true;UE_LOG(LogTemp,Error,TEXT("CONTENT_FIX seat_fault restore_failed ordinary_travel_disabled=1"));return;
  }
  SeatLastPhase=I->Montage_GetPosition(SeatMotion);
  bool WantsExit=C->Pose!=ECourtyardPose::Seated || Now>SeatDue;
  if(ReadingSeat){
   const bool Nearby=C->GetPawn() && FVector::Dist(C->GetPawn()->GetActorLocation(),A->GetActorLocation())<200 && PlayerVisible();
   if(!ReadingWaking && Now-SeatSince>5 && (Nearby || Now>SeatDue)){
    ReadingWaking=true;ReadingWakeAt=Now;C->CancelFace();
    UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX reading_wake reason=%s"),Nearby?TEXT("player_near"):TEXT("rest_finished"));
   }
   WantsExit=ReadingWaking && Now-ReadingWakeAt>1.8;
  }
  if(!SeatExitQueued && Now-SeatSince>3 && WantsExit){
   I->Montage_SetNextSection(TEXT("Rest"),TEXT("Exit"),SeatMotion);SeatExitQueued=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT seat_exit_queued continuous_section=1"));
  }return;
 }
 if(FloorJourney || OwnsTravel || !Emotion.IsNone() || !Safe())return;
 if(ReadingSeatValidated && Now>NextReadingSeat && C->Mood==TEXT("sleepy") && Now-C->MoodChangedAt>75 && C->LocoIndoors() && C->GetPawn() && C->GetPawn()->GetDistanceTo(C->Emma())>240){BeginReadingVisit();return;}
 if(!SeatingValidated || Now<NextSeat || C->Pose!=ECourtyardPose::Seated || Now-C->PoseSince<10)return;
 if(FVector::Dist2D(C->GetPawn()->GetActorLocation(),FVector(-265,82,0))>130 || FVector::Dist2D(C->Emma()->GetActorLocation(),FVector(-155,82,0))>1000)return;
 ReadingSeat=false;
 const bool Accepted=Travel(FixEnabled?SeatEntry():FVector(-155,82,C->Emma()->GetActorLocation().Z),FixEnabled?90:-90,TEXT("BenchApproach"),FixEnabled?TEXT("SeatApproach"):TEXT("BenchSit"));
 NextSeat=Now+(Accepted?360:30);
}
