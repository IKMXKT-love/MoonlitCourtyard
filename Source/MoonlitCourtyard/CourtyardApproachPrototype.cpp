#include "CourtyardController.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"

namespace {void SetFlag(UObject* O,FName N,bool V){if(O)if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N))P->SetPropertyValue_InContainer(O,V);}}
bool ACourtyardController::IsApproachPrototypeWorld() const
{
#if WITH_EDITOR
 return FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"))&&GetWorld()&&GetWorld()->GetMapName().EndsWith(TEXT("L_EmmaApproach_N3_20261004"));
#else
 return false;
#endif
}
FTransform ACourtyardController::ApproachPose(UAnimSequence* Seq,float Time,const FTransform& Start) const
{
 auto* A=Cast<ACharacter>(Emma());if(!A||!Seq)return Start;
 // Same composition as USkeletalMeshComponent::ConvertLocalRootMotionToWorld,
 // evaluated against a prospective start, without moving any actor.
 const FTransform MeshWorld=A->GetMesh()->GetRelativeTransform()*Start;
 const FTransform ComponentToActor=Start.GetRelativeTransform(MeshWorld);
 const FAnimExtractContext Context(0.0,true,FDeltaTimeRecord(Time),false);
 const FTransform Root=Seq->ExtractRootMotion(Context);
 FTransform End=ComponentToActor*(Root*MeshWorld);FVector P=End.GetLocation();P.Z=Start.GetLocation().Z;End.SetLocation(P);return End;
}
bool ACourtyardController::ApproachClear(UAnimSequence* Seq,float From,float To,const FTransform& Start,bool Log) const
{
 auto* A=Cast<ACharacter>(Emma());if(!A)return false;
 const auto* Cap=A->GetCapsuleComponent();FCollisionQueryParams Q(SCENE_QUERY_STAT(ApproachPrototype),false,A);
 const int Steps=FMath::Max(1,FMath::CeilToInt((To-From)/.08f));FVector Last=ApproachPose(Seq,From,Start).GetLocation();
 for(int J=0;J<=Steps;++J){const float T=FMath::Lerp(From,To,float(J)/Steps);FVector Next=ApproachPose(Seq,T,Start).GetLocation();FHitResult H;
  if(GetWorld()->SweepSingleByProfile(H,Last,Next,FQuat::Identity,TEXT("Pawn"),FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius()+3,Cap->GetScaledCapsuleHalfHeight()-2),Q)){
   if(Log)UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH blocked actor=%s component=%s phase=%.3f initial=%d point=%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),T,H.bStartPenetrating,*H.ImpactPoint.ToString());return false;}
  // Flat-ground prototype: reject gaps and steps rather than inventing foot IK.
  FHitResult Ground;if(!GetWorld()->LineTraceSingleByChannel(Ground,Next,Next-FVector(0,0,100),ECC_Visibility,Q)||FMath::Abs((Next.Z-Cap->GetScaledCapsuleHalfHeight())-Ground.ImpactPoint.Z)>5){if(Log)UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH unsupported phase=%.3f at=%s"),T,*Next.ToString());return false;}
  Last=Next;
 }return true;
}
void ACourtyardController::CourtyardReviewApproach(bool Start)
{
 if(!IsApproachPrototypeWorld())return;auto* A=Cast<ACharacter>(Emma());if(!A||!PlayerCharacter)return;auto* I=A->GetMesh()->GetAnimInstance();if(!I)return;
 if(!Start){if(ApproachActive)I->Montage_Stop(.3f,ApproachSegment%2?ApproachTurnMontage.Get():ApproachWalkMontage.Get());ApproachActive=false;ApproachPlaying=false;ApproachPaused=false;SetFlag(A,TEXT("TravelActive"),false);SetFlag(A,TEXT("AllowInteraction"),false);UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH cancelled no_snap=1"));return;}
 if(ApproachActive||I->IsAnyMontagePlaying()||Page!=ECourtyardPage::Game)return;
#if WITH_EDITOR
 ApproachStraight=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Emma/Animations/Approach20261004/A_Emma_ApproachStraight3.A_Emma_ApproachStraight3"));
 ApproachWalkMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Emma/Animations/Approach20261004/AM_Emma_ApproachStraight3.AM_Emma_ApproachStraight3"));
 ApproachTurn=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Emma/Animations/A_Emma_CompactTurn_Review.A_Emma_CompactTurn_Review"));
 ApproachTurnMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Emma/Animations/AM_Emma_CompactTurn_Review.AM_Emma_CompactTurn_Review"));
#endif
 if(!ApproachStraight||!ApproachWalkMontage||!ApproachTurn||!ApproachTurnMontage){UE_LOG(LogTemp,Error,TEXT("COURTYARD_APPROACH missing independent test assets"));return;}
 for(UAnimSequence* S : {ApproachStraight.Get(),ApproachTurn.Get()}){
  const float Len=S->GetPlayLength();const FAnimExtractContext C(0.0,true,FDeltaTimeRecord(Len),false);
  FAnimExtractContext C0(0.0),C1(static_cast<double>(Len));
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH extract asset=%s length=%.4f enabled=%d start=%s end=%s delta=%s mesh=%s"),*S->GetName(),Len,S->HasRootMotion(),*S->ExtractRootTrackTransform(C0,nullptr).ToHumanReadableString(),*S->ExtractRootTrackTransform(C1,nullptr).ToHumanReadableString(),*S->ExtractRootMotion(C).ToHumanReadableString(),*A->GetMesh()->GetRelativeTransform().ToHumanReadableString());
#if WITH_EDITOR
  S->CacheDerivedDataForCurrentPlatform();
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH prepared asset=%s start=%s end=%s delta=%s"),*S->GetName(),*S->ExtractRootTrackTransform(C0,nullptr).ToHumanReadableString(),*S->ExtractRootTrackTransform(C1,nullptr).ToHumanReadableString(),*S->ExtractRootMotion(C).ToHumanReadableString());
#endif
 }
 FTransform Test=A->GetActorTransform();const FTransform End=ApproachPose(ApproachStraight,ApproachStraight->GetPlayLength(),Test);
 const FVector ToPlayer=(PlayerCharacter->GetActorLocation()-End.GetLocation()).GetSafeNormal2D();const float Dist=FVector::Dist2D(PlayerCharacter->GetActorLocation(),End.GetLocation());
 if(Dist<135||Dist>165||FVector::DotProduct(End.GetRotation().GetForwardVector(),ToPlayer)<.99){UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH incompatible_player predicted=%s distance=%.2f facing=%.3f"),*End.GetLocation().ToString(),Dist,FVector::DotProduct(End.GetRotation().GetForwardVector(),ToPlayer));return;}
 for(int S=0;S<4;++S){auto* Seq=S%2?ApproachTurn.Get():ApproachStraight.Get();if(!ApproachClear(Seq,0,Seq->GetPlayLength(),Test,true)){UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH refused preflight_segment=%d"),S);return;}Test=ApproachPose(Seq,Seq->GetPlayLength(),Test);}
 ApproachHome=A->GetActorTransform();ApproachSegment=0;ApproachActive=true;ApproachPaused=false;ApproachPlaying=false;ApproachDue=GetWorld()->GetTimeSeconds();
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH start target_dist=%.3f predicted_return_error=%.4f start=%s"),Dist,FVector::Dist(Test.GetLocation(),ApproachHome.GetLocation()),*ApproachHome.GetLocation().ToString());
}
void ACourtyardController::TickApproachPrototype()
{
 auto* A=Cast<ACharacter>(Emma());if(!A)return;auto* I=A->GetMesh()->GetAnimInstance();if(!I)return;const double Now=GetWorld()->GetTimeSeconds();
 if(!ApproachInitialized){ApproachInitialized=true;SetFlag(A,TEXT("ActivityEnabled"),false);SetFlag(A,TEXT("GlanceEnabled"),false);SetFlag(A,TEXT("AllowInteraction"),false);UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH isolated map initialized; no automatic prototype start; E excluded"));}
 if(!ApproachActive)return;
 auto* Seq=ApproachSegment%2?ApproachTurn.Get():ApproachStraight.Get();auto* M=ApproachSegment%2?ApproachTurnMontage.Get():ApproachWalkMontage.Get();
 if(Now>=ApproachNextProbe){ApproachNextProbe=Now+.1;const FVector P=A->GetActorLocation(),L=A->GetMesh()->GetSocketLocation(TEXT("足首_L")),R=A->GetMesh()->GetSocketLocation(TEXT("足首_R"));
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH sample t=%.4f segment=%d phase=%.4f paused=%d pos=%s yaw=%.3f distance=%.3f left=%s right=%s"),Now,ApproachSegment,I->Montage_GetPosition(M),ApproachPaused,*P.ToString(),A->GetActorRotation().Yaw,FVector::Dist2D(P,PlayerCharacter->GetActorLocation()),*L.ToString(),*R.ToString());}
 if(ApproachPaused){
  if(!I->Montage_IsActive(M)){CourtyardReviewApproach(false);return;}
  const float Phase=I->Montage_GetPosition(M);if(ApproachClear(Seq,Phase,FMath::Min(Seq->GetPlayLength(),Phase+.7f),ApproachStart,false)){I->Montage_Resume(M);ApproachPaused=false;UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH resumed phase=%.3f"),Phase);}
  else if(Now-ApproachBlockedAt>12)CourtyardReviewApproach(false);return;
 }
 if(ApproachPlaying&&I->Montage_IsPlaying(M)){
  const float Phase=I->Montage_GetPosition(M);const float Error=FVector::Dist2D(A->GetActorLocation(),ApproachPose(Seq,Phase,ApproachStart).GetLocation());
  if(Error>10.f){UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH trajectory_deviation_cm=%.3f phase=%.3f; cancel at actual position"),Error,Phase);CourtyardReviewApproach(false);return;}
  if(!ApproachClear(Seq,Phase,FMath::Min(Seq->GetPlayLength(),Phase+.7f),ApproachStart,false)){I->Montage_Pause(M);ApproachPaused=true;ApproachBlockedAt=Now;UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH paused phase=%.3f"),Phase);}return;
 }
 if(ApproachPlaying){
  ApproachPlaying=false;SetFlag(A,TEXT("TravelActive"),false);SetFlag(A,TEXT("AllowInteraction"),false);
  const FVector V=PlayerCharacter->GetActorLocation()-A->GetActorLocation();
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH segment_end=%d pos=%s yaw=%.3f player_cm=%.3f facing_deg=%.3f"),ApproachSegment,*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw,V.Size2D(),FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,V.Rotation().Yaw));
  ++ApproachSegment;ApproachDue=Now+(ApproachSegment==1?3.:.35);
  if(ApproachSegment==4){ApproachActive=false;UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH finished home_error_cm=%.4f yaw_error=%.4f actual=%s"),FVector::Dist2D(A->GetActorLocation(),ApproachHome.GetLocation()),FMath::FindDeltaAngleDegrees(ApproachHome.Rotator().Yaw,A->GetActorRotation().Yaw),*A->GetActorLocation().ToString());}return;
 }
 if(Now<ApproachDue||I->IsAnyMontagePlaying())return;
 ApproachStart=A->GetActorTransform();if(!ApproachClear(Seq,0,Seq->GetPlayLength(),ApproachStart,true)){UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH segment refused; stays at actual position"));ApproachActive=false;return;}
 SetFlag(A,TEXT("AllowInteraction"),false);SetFlag(A,TEXT("TravelActive"),true);
 if(I->Montage_Play(M,1,EMontagePlayReturnType::MontageLength,0,false)<=0){CourtyardReviewApproach(false);return;}
 ApproachPlaying=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_APPROACH segment_start=%d montage=%s"),ApproachSegment,*M->GetName());
}
