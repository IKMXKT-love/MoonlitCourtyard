#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardGazeAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "SkeletalRenderPublic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace { const FVector Dock(-820,153,82.4); constexpr float Facing=-90; }
bool ACourtyardContentLife::BeginTerracePalmRest()
{
 if(!TerraceContactEnabled || !FixEnabled || !Emotion.IsNone() || TerracePalmMotion || OwnsTravel || !Safe())return false;
 auto* A=Cast<ACharacter>(C->Emma());if(!A)return false;
 const float D=FVector::Dist(A->GetActorLocation(),Dock),Y=FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,Facing));
 // A fixed authored contact cannot accept the broad ordinary observation radius.
 if(D>3.f || Y>2.f)return false;
 UAnimMontage* Motion=Motions.FindRef(TEXT("TerracePalmRest"));
 if(!Motion || !C->PlayCompanion(Motion,737,TEXT("new_terrace_palm_contact")))return false;
 if(!TerraceVisitPending || TerracePalmArrivalUntil<=0)TerracePalmOldPause=C->LocoLifePaused;
 C->LocoLifePaused=true;TerracePalmMotion=Motion;TerracePalmPhase=0;
 ActiveTask=TEXT("TerracePalmRest");TaskSince=GetWorld()->GetTimeSeconds();TaskUntil=TaskSince+Motion->GetPlayLength()+1;
 ContextFace(TEXT("TerracePalmRest"));C->LivelyStarted(TEXT("N_TerracePalmRest"),TEXT("content37_contact"),true);
 NextTerracePalmVisit=TaskSince+Random.FRandRange(240,420);
 UE_LOG(LogTemp,Display,TEXT("CONTENT37 palm_begin measured_dock_cm=%.3f yaw_error=%.3f root=%s radius=%.2f"),D,Y,*A->GetActorLocation().ToString(),A->GetCapsuleComponent()->GetScaledCapsuleRadius());return true;
}
bool ACourtyardContentLife::BeginTerracePalmVisit(double Now)
{
 // A sparse visit can depart after a completed gesture. It need not win the
 // separate five-second hand-contact hold window before it starts walking.
 if(!TerraceContactEnabled || !Safe(false) || GetWorld()->GetTimeSeconds()-C->AmbientStillSince<1 || C->Emma()->GetVelocity().Size()>2 || OwnsTravel || TerracePalmMotion || TerraceVisitPending || !Emotion.IsNone() || !ActiveTask.IsNone() || FloorJourney)return false;
 const FVector Here=C->Emma()->GetActorLocation();const float D=FVector::Dist2D(Here,Dock);
 if(FMath::Abs(Here.Z-Dock.Z)>10 || D>1900 || D<80)return false;
 if(C->GetPawn() && FVector::Dist2D(C->GetPawn()->GetActorLocation(),Dock)<150)return false;
 auto* A=Cast<ACharacter>(C->Emma());FCollisionQueryParams Q(SCENE_QUERY_STAT(TerracePalmDock),false,A);
 if(GetWorld()->OverlapBlockingTestByChannel(Dock,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-.2f),Q))return false;
 if(!Travel(Dock,Facing,TEXT("TerracePalmVisit"),NAME_None,0))return false;
 TerraceVisitPending=true;TerracePalmArrivalUntil=0;NextTerracePalmVisit=Now+Random.FRandRange(240,420);
 UE_LOG(LogTemp,Display,TEXT("CONTENT37 palm_visit actual_original_walk=1 goal=%s"),*Dock.ToString());return true;
}
void ACourtyardContentLife::FinishTerracePalm(const TCHAR* Reason)
{
 if(!TerracePalmMotion)return;
 if(C.IsValid()){
  if(EmotionOwnsPause)EmotionOldPause=TerracePalmOldPause;else C->LocoLifePaused=TerracePalmOldPause;
  C->GazePoiUntil=0;C->LocoNextDecision=FMath::Max(C->LocoNextDecision,double(GetWorld()->GetTimeSeconds()+8));
 }
 UE_LOG(LogTemp,Display,TEXT("CONTENT37 palm_end reason=%s phase=%.3f"),Reason,TerracePalmPhase);
 TerracePalmMotion=nullptr;ActiveTask=NAME_None;
}
void ACourtyardContentLife::TickTerracePalm(double Now)
{
 if(!TerraceContactEnabled)return;
 // Explicit review request waits for the existing admissible state instead of
 // cancelling old gestures or repeatedly guessing a recording timestamp.
 if(ReviewTerraceVisitUntil>0){
  if(Now>ReviewTerraceVisitUntil){ReviewTerraceVisitUntil=0;UE_LOG(LogTemp,Display,TEXT("CONTENT37 review_visit_timeout no_actual_departure=1"));}
  else if(Safe(false) && Now-C->AmbientStillSince>=1 && !OwnsTravel && !TerracePalmMotion && !TerraceVisitPending && Emotion.IsNone() && ActiveTask.IsNone() && !FloorJourney){
   ReviewTerraceVisitUntil=0;
   const bool OK=BeginTerracePalmVisit(Now);
   UE_LOG(LogTemp,Display,TEXT("CONTENT37 review_qualified_visit accepted=%d root=%s still_age=%.3f"),OK,*C->Emma()->GetActorLocation().ToString(),Now-C->AmbientStillSince);
  }
 }
 if(TerracePalmMotion){
  auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  if(!I || !I->Montage_IsActive(TerracePalmMotion)){FinishTerracePalm(TerracePalmPhase>8?TEXT("completed"):TEXT("interrupted"));return;}
  TerracePalmPhase=I->Montage_GetPosition(TerracePalmMotion);
  if(!Emotion.IsNone()){I->Montage_Stop(.3f,TerracePalmMotion);FinishTerracePalm(TEXT("emotion_priority"));return;}
  C->GazePoi=FVector(-560,-300,145);C->GazePoiUntil=Now+.25;C->GazePoiHead=.2f;return;
 }
 if(C->Page!=ECourtyardPage::Game)return;
 if(TerraceVisitPending){
  if(!Emotion.IsNone()){
   if(OwnsTravel){C->LocoStopRequested=true;return;}
   if(TerracePalmArrivalUntil>0){if(EmotionOwnsPause)EmotionOldPause=TerracePalmOldPause;else C->LocoLifePaused=TerracePalmOldPause;}
   TerraceVisitPending=false;TerracePalmArrivalUntil=0;return;
  }
  if(OwnsTravel || C->LocoState!=ACourtyardController::ELocoState::Idle)return;
  if(TerracePalmArrivalUntil<=0){TerracePalmArrivalUntil=Now+12;TerracePalmOldPause=C->LocoLifePaused;C->LocoLifePaused=true;}
  if(BeginTerracePalmRest()){TerraceVisitPending=false;TerracePalmArrivalUntil=0;return;}
  if(Now>TerracePalmArrivalUntil){UE_LOG(LogTemp,Display,TEXT("CONTENT37 palm_arrival_rejected error_cm=%.3f yaw_error=%.3f no_snap=1"),FVector::Dist(C->Emma()->GetActorLocation(),Dock),FMath::Abs(FMath::FindDeltaAngleDegrees(C->Emma()->GetActorRotation().Yaw,Facing)));C->LocoLifePaused=TerracePalmOldPause;TerraceVisitPending=false;TerracePalmArrivalUntil=0;}
  return;
 }
 if(NextTerracePalmVisit<=0){NextTerracePalmVisit=Now+Random.FRandRange(90,180);return;}
 if(Now>=NextTerracePalmVisit){NextTerracePalmVisit=Now+Random.FRandRange(35,65);if(FVector::Dist(C->Emma()->GetActorLocation(),Dock)<=3.f)BeginTerracePalmRest();else BeginTerracePalmVisit(Now);}
}
void ACourtyardContentLife::SampleTerracePalm()
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!A)return;auto* M=A->GetMesh();TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);TArray<FVector3f> P;
 for(const auto& X:V)P.Add(FVector3f(M->GetComponentTransform().TransformPosition(FVector(X.Position))));
 const FString Dir=FPaths::ProjectDir()/TEXT("Audit/Content37_20261009/Surface");IFileManager::Get().MakeDirectory(*Dir,true);
 const FString File=Dir/FString::Printf(TEXT("palm_%05d_world.bin"),FMath::RoundToInt(TerracePalmPhase*100));
 FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)P.GetData(),P.Num()*sizeof(FVector3f)),*File);
 UE_LOG(LogTemp,Display,TEXT("CONTENT37 palm_real_surface phase=%.3f vertices=%d transform=%s file=%s"),TerracePalmPhase,P.Num(),*M->GetComponentTransform().ToString(),*File);
}
void ACourtyardContentLife::FillTerracePalmFrame(FCourtyardGazeFrame& Frame) const
{
 if(!TerraceContactEnabled || !TerracePalmMotion || !C.IsValid())return;
 auto* A=Cast<ACharacter>(C->Emma());if(!A)return;auto* M=A->GetMesh();
 auto Ease=[](float X){X=FMath::Clamp(X,0.f,1.f);return X*X*X*(10+X*(-15+6*X));};
 Frame.TerracePalmWeight=Ease((TerracePalmPhase-.65f)/1.65f)*Ease((8.75f-TerracePalmPhase)/2.35f);
 // This is the fixed rail-side wrist anchor of the authored hold, not a moving
 // prop or a snapped character. Limit the correction to the measured dock range.
 const FVector FixedWorldWrist(-802,124,113);
 const FVector Offset=M->GetComponentTransform().InverseTransformPosition(FixedWorldWrist)-FVector(-18,29,113);
 if(Offset.Size()>4.f){Frame.TerracePalmWeight=0;return;}
 Frame.TerracePalmOffsetCS=Offset;
 Frame.TerracePalmYaw=FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,Facing);
 // Keep the clip's authored torso and left arm; normal eye/head gaze remains.
 Frame.TorsoYaw=Frame.TorsoPitch=0;
}
