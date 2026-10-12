#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

namespace { void StairInteraction(AActor* A,bool V){if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("AllowInteraction")))P->SetPropertyValue_InContainer(A,V);} }
bool ACourtyardContentLife::BeginFixedStairs(bool Down)
{
 if(!FixEnabled || FixedStairMotion || !Safe(false))return false;
 auto* A=Cast<ACharacter>(C->Emma());auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;if(!I)return false;
 const FVector Dock=Down?FVector(-102,1574,528.4):FVector(102,1530,82.4);
 if(FVector::Dist2D(Dock,A->GetActorLocation())>6 || FMath::Abs(A->GetActorLocation().Z-Dock.Z)>5 || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,90))>5)return false;
 auto* M=Motions.Find(Down?TEXT("StairsDown"):TEXT("StairsUp"));if(!M || !*M)return false;
 auto* Move=A->GetCharacterMovement();if(!Move->IsMovingOnGround())return false;
 FixedStairMotion=*M;FixedStairStart=A->GetActorLocation();FixedStairStartRotation=A->GetActorRotation();FixedStairMeshOffset=A->GetMesh()->GetRelativeLocation();FixedStairLast=FixedStairStart;
 FixedStairRootToWorld=A->GetMesh()->GetComponentQuat();FixedStairWaiting=FixedStairLateWaiting=false;FixedStairClearSince=-1;FixedStairRate=1;FixedStairLastPhase=0;
 FixedStairRecoveryTried=false;
 FixedStairOldPause=C->LocoLifePaused;FixedStairOldHold=C->ReviewBodyHold;C->LocoLifePaused=true;C->ReviewBodyHold=true;
 FixedStairOldStep=Move->MaxStepHeight;Move->MaxStepHeight=24;
 FixedStairOldOrient=Move->bOrientRotationToMovement;FixedStairOldDesired=Move->bUseControllerDesiredRotation;
 Move->bOrientRotationToMovement=false;Move->bUseControllerDesiredRotation=false;
 StairInteraction(A,false);A->SetAnimRootMotionTranslationScale(1);C->CancelFace();
 FixedStairSince=GetWorld()->GetTimeSeconds();FixedStairReport=FixedStairSince;FixedStairBlocked=0;
 if(I->Montage_Play(FixedStairMotion,1)<=0){FinishFixedStairs(false);return false;}
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_start down=%d pos=%s capsule=%.1fx%.1f mode=%d"),Down,*FixedStairStart.ToString(),A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),int(Move->MovementMode));
 return true;
}
bool ACourtyardContentLife::TickFixedStairs(float Dt,double Now)
{
 if(!FixedStairMotion)return false;
 auto* A=Cast<ACharacter>(C->Emma());auto* Mesh=A?A->GetMesh():nullptr;auto* I=Mesh?Mesh->GetAnimInstance():nullptr;
 if(!I){FinishFixedStairs(false);return false;}
 if(ReviewStairCamera){
  if(!C->BodyReviewCamera)C->BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
  const FVector Focus=A->GetActorLocation()+FVector(0,0,ReviewStairFeetCamera?-48:-16);
  const FVector Position=ReviewStairFeetCamera?Focus+FVector(Focus.X<0?-35:35,-155,115):FVector(Focus.X<0?-390:390,Focus.Y-100,Focus.Z+125);
  C->BodyReviewCamera->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
  C->BodyReviewCamera->GetCameraComponent()->SetFieldOfView(63);C->SetViewTargetWithBlend(C->BodyReviewCamera,0);
 }
 if(!I->Montage_IsActive(FixedStairMotion) && Now-FixedStairSince>.5){
  const bool Complete=FixedStairLastPhase>=FixedStairMotion->GetPlayLength()-.6f;
  // Fault recovery only: retain the actual path origin and last phase. Do not
  // restart the ascent, align the actor, or release mid-stair into flat walking.
  auto* Other=I->GetCurrentActiveMontage();
  if(!Complete)UE_LOG(LogTemp,Display,TEXT("CONTENT28 stair_loss enabled=%d tried=%d grounded=%d current=%s any_playing=%d"),StairFaultRecoveryEnabled,FixedStairRecoveryTried,A->GetCharacterMovement()->IsMovingOnGround(),*GetNameSafe(Other),I->IsAnyMontagePlaying());
  // A stopped montage can remain 'playing' during blend-out. It must not be
  // mistaken for another owner; only a different active montage blocks recovery.
  if(!Complete && StairFaultRecoveryEnabled && !FixedStairRecoveryTried && A->GetCharacterMovement()->IsMovingOnGround() && (!Other || Other==FixedStairMotion)){
   FixedStairRecoveryTried=true;
   if(I->Montage_Play(FixedStairMotion,1,EMontagePlayReturnType::MontageLength,FixedStairLastPhase,false)>0){
    FixedStairRate=0;FixedStairBlocked=0;FixedStairLast=A->GetActorLocation();
    if(FixedStairWaiting)I->Montage_Pause(FixedStairMotion);
    UE_LOG(LogTemp,Warning,TEXT("CONTENT28 stair_clip_restored phase=%.3f waiting=%d position=%s attempts=1"),FixedStairLastPhase,FixedStairWaiting,*A->GetActorLocation().ToString());
    return true;
   }
  }
  FinishFixedStairs(Complete);return false;
 }
 auto* Seq=FixedStairMotion->SlotAnimTracks.Num() && FixedStairMotion->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()?Cast<UAnimSequence>(FixedStairMotion->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()):nullptr;
 if(!Seq)return true;
 const float T=I->Montage_GetPosition(FixedStairMotion);const FTransform Root=Seq->ExtractRootMotionFromRange(0,T);
 FixedStairLastPhase=T;
 // Check the actual full character capsules ahead, independently of stair treads.
 // Pause the retained montage instead of discarding a mid-stair pose and root path.
 FCollisionQueryParams Q;Q.AddIgnoredActor(A);FCollisionObjectQueryParams Pawns;Pawns.AddObjectTypesToQuery(ECC_Pawn);
 const auto* Capsule=A->GetCapsuleComponent();
 const FCollisionShape Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius()+6,Capsule->GetScaledCapsuleHalfHeight());
 FVector Previous=A->GetActorLocation();bool PawnAhead=false;
 for(int K=1;K<=8 && !PawnAhead;++K){
  const float Future=FMath::Min(T+K*.14f,Seq->GetPlayLength()-.001f);
  const FVector Delta=FixedStairRootToWorld.RotateVector(Seq->ExtractRootMotionFromRange(0,Future).GetTranslation()-Root.GetTranslation());
  const FVector Next=A->GetActorLocation()+Delta;FHitResult H;
  PawnAhead=GetWorld()->SweepSingleByObjectType(H,Previous,Next,FQuat::Identity,Pawns,Shape,Q);Previous=Next;
 }
 const float Phase=T<13?(T-.5f)/.85f:T<22.6f?(T-13.3f)/.75f:(T-23.1f)/.85f;
 const float Fraction=FMath::Frac(FMath::Max(0.f,Phase));
 const bool Planted=Phase<0 || Fraction<.12f || Fraction>.92f;
 if(PawnAhead && !FixedStairWaiting && Planted){
  // Rate zero leaves UE's montage substep time accumulating. A real pause
  // preserves the phase without replaying the waiting time on resume.
  FixedStairWaiting=true;FixedStairClearSince=-1;FixedStairRate=0;I->Montage_Pause(FixedStairMotion);
  A->GetCharacterMovement()->StopMovementImmediately();
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_wait phase=%.3f reason=player_ahead capsule_preserved=1"),T);
 }
 if(FixedStairWaiting){
  if(FixedStairLateWaiting){
   const FVector Delta=FixedStairRootToWorld.RotateVector(Seq->ExtractRootMotionFromRange(0,FMath::Min(T+.45f,Seq->GetPlayLength()-.001f)).GetTranslation()-Root.GetTranslation());
   FHitResult H;const FVector Lift(0,0,A->GetCharacterMovement()->MaxStepHeight);
   const bool Solid=GetWorld()->SweepSingleByChannel(H,A->GetActorLocation()+Lift,A->GetActorLocation()+Delta+Lift,FQuat::Identity,ECC_Pawn,Shape,Q);
   PawnAhead|=Solid && H.ImpactNormal.Z<.65f;
  }
  if(PawnAhead)FixedStairClearSince=-1;
  else if(FixedStairClearSince<0)FixedStairClearSince=Now;
  if(FixedStairClearSince>=0 && Now-FixedStairClearSince>.65){
   FixedStairWaiting=FixedStairLateWaiting=false;FixedStairBlocked=0;FixedStairLast=A->GetActorLocation();I->Montage_Resume(FixedStairMotion);
   UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_resume phase=%.3f location=%s"),T,*A->GetActorLocation().ToString());
  }else return true;
 }
 FixedStairRate=FMath::FInterpConstantTo(FixedStairRate,1.f,Dt,2.5f);I->Montage_SetPlayRate(FixedStairMotion,FixedStairRate);
 // Walking owns the full capsule and all collision. Only visual root height follows the
 // authored support trajectory; no flying, capsule resizing, translation scaling or teleport.
 const float Desired=FixedStairStart.Z+Root.GetTranslation().Z;
 const float Offset=Desired-A->GetActorLocation().Z;
 if(FMath::Abs(Offset)>40){UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX stair_abort vertical_divergence=%.2f t=%.2f"),Offset,T);I->Montage_Stop(.35f,FixedStairMotion);FinishFixedStairs(false);return true;}
 Mesh->SetRelativeLocation(FixedStairMeshOffset+FVector(0,0,Offset));
 const float Movement=FVector::Dist2D(FixedStairLast,A->GetActorLocation());FixedStairLast=A->GetActorLocation();
 const auto Delta=Seq->ExtractRootMotionFromRange(FMath::Max(0.f,T-Dt),T).GetTranslation();
 if(T>.65 && Delta.Size2D()>.25f && Movement<Dt*2)FixedStairBlocked+=Dt;else FixedStairBlocked=0;
 if(FixedStairBlocked>.15){
  // A late obstacle keeps the current phase; never enable flat-ground wandering mid-flight.
  FixedStairWaiting=FixedStairLateWaiting=true;FixedStairClearSince=-1;FixedStairRate=0;I->Montage_Pause(FixedStairMotion);
  A->GetCharacterMovement()->StopMovementImmediately();
  UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX stair_wait phase=%.3f reason=late_obstruction"),T);return true;
 }
 if(Now>=FixedStairReport){FixedStairReport=Now+.2;
  FHitResult L,R;
  const FVector LF=Mesh->GetSocketLocation(TEXT("足首D_L")),RF=Mesh->GetSocketLocation(TEXT("足首D_R"));
  GetWorld()->LineTraceSingleByChannel(L,LF+FVector(0,0,20),LF-FVector(0,0,45),ECC_Visibility,Q);GetWorld()->LineTraceSingleByChannel(R,RF+FVector(0,0,20),RF-FVector(0,0,45),ECC_Visibility,Q);
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_frame t=%.3f actor=%s yaw=%.2f mesh_offset=%.3f mode=%d LF=%s floorL=%.3f RF=%s floorR=%.3f"),T,*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw,Offset,int(A->GetCharacterMovement()->MovementMode),*LF.ToString(),L.ImpactPoint.Z,*RF.ToString(),R.ImpactPoint.Z);
  if(ReviewStairCamera)SampleFixedStairFeet(T);
 }
 return true;
}
void ACourtyardContentLife::FinishFixedStairs(bool Arrived)
{
 if(!C.IsValid())return;auto* A=Cast<ACharacter>(C->Emma());if(!A)return;auto* Move=A->GetCharacterMovement();
 const float Residual=A->GetMesh()->GetRelativeLocation().Z-FixedStairMeshOffset.Z;
 A->GetMesh()->SetRelativeLocation(FixedStairMeshOffset);Move->MaxStepHeight=FixedStairOldStep;Move->bOrientRotationToMovement=FixedStairOldOrient;Move->bUseControllerDesiredRotation=FixedStairOldDesired;
 if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
 C->LocoLifePaused=FixedStairOldPause;C->ReviewBodyHold=FixedStairOldHold;StairInteraction(A,true);C->AmbientStillSince=GetWorld()->GetTimeSeconds();C->LocoNextDecision=GetWorld()->GetTimeSeconds()+15;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_end nominal_complete=%d pos=%s yaw=%.2f residual_cm=%.3f grounded=%d"),Arrived,*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw,Residual,Move->IsMovingOnGround());FixedStairMotion=nullptr;
 if(FloorJourney){if(Arrived)FloorJourney=FloorJourney==3?4:11;else EndFloorJourney(TEXT("stairs_obstructed"));}
}
