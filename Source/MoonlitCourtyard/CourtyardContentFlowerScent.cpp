#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardGazeAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"

bool ACourtyardContentLife::BeginFlowerScentVisit(double Now)
{
 if(!FlowerScentEnabled || !FixEnabled || !Safe() || OwnsTravel || FlowerVisitPending || FlowerScentMotion || FloorJourney || !ActiveTask.IsNone() || !Emotion.IsNone())return false;
 auto* A=Cast<ACharacter>(C->Emma());const float D=FVector::Dist2D(A->GetActorLocation(),FlowerScentDock);
 if(D<80 || D>2200 || FMath::Abs(A->GetActorLocation().Z-FlowerScentDock.Z)>10 || FVector::Dist2D(C->GetPawn()->GetActorLocation(),FlowerScentDock)<130)return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(FlowerVisitSpace),false,A);
 if(GetWorld()->OverlapBlockingTestByChannel(FlowerScentDock,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-.2f),Q))return false;
 if(!Travel(FlowerScentDock,FlowerScentYaw,TEXT("FlowerScentVisit"),NAME_None,0))return false;
 FlowerVisitPending=true;FlowerArrivalUntil=0;FlowerLastProgress=A->GetActorLocation();FlowerProgressAt=Now;
 NextFlowerScent=Now+Random.FRandRange(300,480);
 UE_LOG(LogTemp,Display,TEXT("CONTENT39 flower_visit_begin accepted_original_walk=1 goal=%s"),*FlowerScentDock.ToString());return true;
}
bool ACourtyardContentLife::BeginFlowerScent()
{
 if(!FlowerScentEnabled || FlowerScentMotion || OwnsTravel || !Emotion.IsNone() || !Safe())return false;
 auto* A=Cast<ACharacter>(C->Emma());const float D=FVector::Dist(A->GetActorLocation(),FlowerScentDock),Y=FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,FlowerScentYaw));
 if(D>3 || Y>2)return false;
 UAnimMontage* Motion=Motions.FindRef(TEXT("FlowerScent"));if(!Motion || !C->PlayCompanion(Motion,738,TEXT("physical_flower_scent")))return false;
 if(!FlowerOwnsPause){FlowerOldPause=C->LocoLifePaused;FlowerOwnsPause=true;}C->LocoLifePaused=true;
 FlowerScentMotion=Motion;FlowerScentPhase=0;ActiveTask=TEXT("FlowerScent");TaskSince=GetWorld()->GetTimeSeconds();TaskUntil=TaskSince+Motion->GetPlayLength()+1;
 C->LivelyStarted(TEXT("N_FlowerScent"),TEXT("content39_physical_contact"),true);
 UE_LOG(LogTemp,Display,TEXT("CONTENT39 flower_begin actual_error_cm=%.3f yaw_error=%.3f no_snap=1"),D,Y);return true;
}
void ACourtyardContentLife::FinishFlowerScent(const TCHAR* Reason)
{
 if(!FlowerVisitPending && !FlowerScentMotion && !FlowerOwnsPause)return;
 if(FlowerOwnsPause && C.IsValid()){if(EmotionOwnsPause)EmotionOldPause=FlowerOldPause;else C->LocoLifePaused=FlowerOldPause;C->LocoNextDecision=FMath::Max(C->LocoNextDecision,double(GetWorld()->GetTimeSeconds()+8));}
 UE_LOG(LogTemp,Display,TEXT("CONTENT39 flower_end reason=%s phase=%.3f root=%s"),Reason,FlowerScentPhase,C.IsValid()&&C->Emma()?*C->Emma()->GetActorLocation().ToString():TEXT("missing"));
 FlowerScentMotion=nullptr;FlowerVisitPending=false;FlowerOwnsPause=false;FlowerArrivalUntil=0;ActiveTask=NAME_None;
}
void ACourtyardContentLife::TickFlowerScent(double Now)
{
 if(!FlowerScentEnabled || !C.IsValid() || !C->Emma())return;
 auto* A=Cast<ACharacter>(C->Emma());auto* I=A->GetMesh()->GetAnimInstance();
 if(FlowerScentMotion){
  if(!I || !I->Montage_IsActive(FlowerScentMotion)){FinishFlowerScent(FlowerScentPhase>9?TEXT("completed"):TEXT("interrupted"));return;}
  FlowerScentPhase=I->Montage_GetPosition(FlowerScentMotion);
  if(!Emotion.IsNone()){I->Montage_Stop(.3f,FlowerScentMotion);FinishFlowerScent(TEXT("emotion_priority"));}return;
 }
 if(FlowerVisitPending){
  if(OwnsTravel && C->LocoState!=ACourtyardController::ELocoState::Idle){
   bool Blocked=false;for(float S=20;S<=110 && !Blocked;S+=15)Blocked=FVector::Dist2D(C->LocoPointAt(C->LocoProgress+S),C->GetPawn()->GetActorLocation())<38;
   if(FVector::Dist2D(A->GetActorLocation(),FlowerLastProgress)>3){FlowerLastProgress=A->GetActorLocation();FlowerProgressAt=Now;}
   if((Blocked || Now-FlowerProgressAt>1.5 || !Emotion.IsNone()) && !C->LocoStopRequested){C->LocoStopRequested=true;UE_LOG(LogTemp,Display,TEXT("CONTENT39 flower_travel_stop player_blocked=%d no_progress_seconds=%.3f"),Blocked,Now-FlowerProgressAt);}return;
  }
  if(OwnsTravel){const bool Arrived=C->LocoEndArrived;ReleaseTravel();ActiveTask=ArriveAction=NAME_None;if(!Arrived){FinishFlowerScent(TEXT("travel_cancelled"));return;}FlowerOldPause=C->LocoLifePaused;FlowerOwnsPause=true;C->LocoLifePaused=true;FlowerArrivalUntil=Now+12;}
  if(!Emotion.IsNone()){FinishFlowerScent(TEXT("emotion_priority"));return;}
  if(BeginFlowerScent())return;
  if(FlowerArrivalUntil>0 && Now>FlowerArrivalUntil){UE_LOG(LogTemp,Display,TEXT("CONTENT39 flower_arrival_rejected error_cm=%.3f yaw_error=%.3f no_snap=1"),FVector::Dist(A->GetActorLocation(),FlowerScentDock),FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,FlowerScentYaw)));FinishFlowerScent(TEXT("contact_qualification_expired"));}return;
 }
 if(C->Page!=ECourtyardPage::Game)return;
 if(ReviewFlowerVisitUntil>0){if(Now>ReviewFlowerVisitUntil){ReviewFlowerVisitUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT39 review_visit_timeout"));}else if(Safe() && !OwnsTravel && !FlowerVisitPending && !FlowerScentMotion && !FloorJourney && ActiveTask.IsNone() && Emotion.IsNone()){ReviewFlowerVisitUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT39 qualified_review_visit accepted=%d single_attempt=1"),BeginFlowerScentVisit(Now));}return;}
 if(NextFlowerScent<=0){NextFlowerScent=Now+Random.FRandRange(120,240);return;}
 if(Now>=NextFlowerScent){NextFlowerScent=Now+Random.FRandRange(60,120);if(FVector::Dist(A->GetActorLocation(),FlowerScentDock)<=3){if(BeginFlowerScent())NextFlowerScent=Now+Random.FRandRange(300,480);}else BeginFlowerScentVisit(Now);}
}
void ACourtyardContentLife::FillFlowerScentFrame(FCourtyardGazeFrame& Frame) const
{
 if(!FlowerScentEnabled || !FlowerScentMotion || !C.IsValid())return;
 auto* A=Cast<ACharacter>(C->Emma());auto* M=A->GetMesh();
 auto Ease=[](float X){X=FMath::Clamp(X,0.f,1.f);return X*X*X*(10+X*(-15+6*X));};
 Frame.TerracePalmWeight=Ease((FlowerScentPhase-1.1f)/2.45f)*Ease((8.7f-FlowerScentPhase)/1.8f);
 // Same existing bounded two-bone hand correction; the plant, feet and actor
 // remain fixed. Only the3cm/2degree-qualified new flower scene can use it.
 const FTransform Dock(FRotator(0,FlowerScentYaw-90,0),FlowerScentDock-FVector(0,0,82.4));
 const FVector Target=Dock.TransformPosition(FVector(-2,28,117));
 const FVector Offset=M->GetComponentTransform().InverseTransformPosition(Target)-FVector(-2,28,117);
 if(Offset.Size()>4){Frame.TerracePalmWeight=0;return;}Frame.TerracePalmOffsetCS=Offset;Frame.TerracePalmYaw=FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,FlowerScentYaw);
 Frame.TorsoYaw=Frame.TorsoPitch=Frame.HeadWeight=Frame.EyeWeight=0;Frame.Additive=false;
}
