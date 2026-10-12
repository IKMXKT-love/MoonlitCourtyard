#include "CourtyardContentLife.h"
#include "CourtyardContentDoor.h"
#include "CourtyardController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"

bool ACourtyardContentLife::BeginDoorAction(ACourtyardContentDoor* Door)
{
 if(!FixEnabled || !Door || Door->Occupied || Door->InMotion() || DoorMotion || !Safe(false))return false;
 auto* A=Cast<ACharacter>(C->Emma());
 // Only the authored, measured opening dock is currently accepted. Closing and
 // other door sides require their own contact poses; never time-toggle remotely.
 const FVector Handle=Door->HandlePoint();
 const FVector Dock(Handle.X+15,Handle.Y-31,A->GetActorLocation().Z);
 if(Door->Open || Door->Bathroom || !Door->PrivacyRoom || FVector::Dist2D(A->GetActorLocation(),Dock)>1.5f || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,90.f))>1.f)return false;
 if(FVector::Dist(A->GetActorLocation()+FVector(0,0,28),Door->HandlePoint())>70)return false;
 if(!Play(TEXT("DoorLatch"),TEXT("DoorLatch")))return false;
 ActionDoor=Door;DoorWasOpen=Door->Open;DoorCommitted=false;DoorSince=GetWorld()->GetTimeSeconds();DoorLastPhase=0;
 DoorMotion=Motions.FindRef(TEXT("DoorLatch"));DoorOldPause=C->LocoLifePaused;C->LocoLifePaused=true;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX npc_door_begin name=%s opening=%d"),*Door->GetName(),!DoorWasOpen);return true;
}
bool ACourtyardContentLife::TickDoorAction(double Now)
{
 if(!DoorMotion)return false;
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
 if(!I || !ActionDoor.IsValid()){FinishDoorAction(false,TEXT("missing_owner"));return true;}
 if(I->Montage_IsActive(DoorMotion)){
  DoorLastPhase=I->Montage_GetPosition(DoorMotion);
  if(!DoorCommitted && DoorLastPhase>=4.65f){
   // The fingers have released the latch before the leaf sweeps through the opening.
   if(ActionDoor->Open!=DoorWasOpen || !ActionDoor->RequestToggle(A)){I->Montage_Stop(.35f,DoorMotion);FinishDoorAction(false,TEXT("door_changed_or_blocked"));return true;}
   DoorCommitted=true;
  }
 }else{
  if(DoorLastPhase<DoorMotion->GetPlayLength()-.5f){FinishDoorAction(false,TEXT("motion_interrupted"));return true;}
  if(DoorCommitted && !ActionDoor->InMotion()){FinishDoorAction(ActionDoor->Open!=DoorWasOpen,TEXT("motion_finished"));return true;}
 }
 if(Now-DoorSince>11){FinishDoorAction(false,TEXT("blocked_timeout"));return true;}
 return true;
}
void ACourtyardContentLife::FinishDoorAction(bool Success,const TCHAR* Reason)
{
 if(C.IsValid())C->LocoLifePaused=DoorOldPause;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX npc_door_end success=%d reason=%s phase=%.3f"),Success,Reason,DoorLastPhase);
 DoorMotion=nullptr;ActionDoor.Reset();ActiveTask=NAME_None;
}
