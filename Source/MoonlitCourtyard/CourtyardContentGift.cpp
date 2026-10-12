#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

bool ACourtyardContentLife::BeginReceiveGift()
{
 if(!FixEnabled || !HoldingFlower || ReceiveGiftSince>=0 || FloorJourney || !Safe()){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_reject fix=%d holding=%d since=%.2f floor=%d safe=%d"),FixEnabled,HoldingFlower,ReceiveGiftSince,FloorJourney,Safe());return false;}
 const bool Wear=(ReviewGiftWear || (GiftWearValidated && !WornFlower->IsVisible() && Random.FRand()<.35f));
 const FName Clip=Wear?FName(TEXT("ReceivePin")):FName(TEXT("ReceiveFlower"));
 if(!Play(Clip,TEXT("GiftReceive"))){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_play_reject motion=%s can_pat=%d speed=%.3f still=%.3f"),*GetNameSafe(Motions.FindRef(Clip)),C->QueryBool(C->Emma(),TEXT("CanHeadPat")),C->Emma()->GetVelocity().Size(),GetWorld()->GetTimeSeconds()-C->AmbientStillSince);return false;}
 GiftOfferStart=Gift->GetComponentTransform();ReceiveGiftSince=GetWorld()->GetTimeSeconds();GiftContactLogged=false;
 ReceiveGiftMotion=Motions.FindRef(Clip);GiftReturning=false;GiftWearActive=Wear;GiftPinned=false;ReviewGiftWear=false;
 HoldingFlower=false;Cooldowns.Add(TEXT("FlowerOffering"),ReceiveGiftSince+180);Comfort(0);
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX flower_receive_begin physical_candidate=1"));return true;
}
bool ACourtyardContentLife::TickReceiveGift(double Now)
{
 if(ReceiveGiftSince<0 || !C.IsValid())return false;
 auto* A=Cast<ACharacter>(C->Emma());if(!A)return false;
 auto* Anim=A->GetMesh()->GetAnimInstance();
 if(!Anim || !ReceiveGiftMotion){FinishReceiveGift(true,TEXT("missing_animation"));return true;}
 const float T=Anim->Montage_GetPosition(ReceiveGiftMotion);
 if(!Anim->Montage_IsActive(ReceiveGiftMotion)){
  FinishReceiveGift(!GiftPinned,TEXT("animation_finished_or_cancelled"));return true;
 }
 // Calibrated against the actual rendered flower and CPU-skinned hand, 2.8--4.8 s.
 const FTransform Local(FQuat(-.4808738,-.4457518,-.5322890,-.5354757),FVector(.010442,-8.496605,4.298348),FVector(.75));
 const FTransform Target=Local*A->GetMesh()->GetSocketTransform(TEXT("手首_R"));
 const float U=FMath::SmoothStep(0.f,1.f,(T-1.15f)/1.5f);
 FTransform Pose;Pose.Blend(GiftOfferStart,Target,U);
 if(U<1)Pose.AddToTranslation(FVector(0,0,5*FMath::Sin(PI*U)));
 // The accepted open-palm clip has no grip/carry or pinning ending. Let the
 // player take the admired flower back before the authored hand lowers.
 // Keep this temporary return explicit, rather than attach it forever to idle.
 if(!GiftWearActive && T>=4.35f && C->PlayerCameraManager){
  if(!GiftReturning){GiftReturning=true;GiftReturnStart=Gift->GetComponentTransform();UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_return_begin t=%.3f"),T);}
  const FTransform View(C->PlayerCameraManager->GetCameraRotation(),C->PlayerCameraManager->GetCameraLocation());
  const FTransform PlayerHand(View.GetRotation(),View.TransformPosition(FVector(38,17,-21)),FVector(1));
  const float Return=FMath::SmoothStep(0.f,1.f,(T-4.35f)/1.10f);
  Pose.Blend(GiftReturnStart,PlayerHand,Return);
  if(Return>=1){FinishReceiveGift(true,TEXT("returned_to_player"));return true;}
 }
 if(GiftWearActive && T>=8.2f && !GiftPinned){
  WornFlower->SetStaticMesh(Gift->GetStaticMesh());
  for(int32 K=0;K<Gift->GetNumMaterials();++K)WornFlower->SetMaterial(K,Gift->GetMaterial(K));
  WornFlower->SetWorldTransform(Pose);
  WornFlower->AttachToComponent(A->GetMesh(),FAttachmentTransformRules::KeepWorldTransform,TEXT("頭"));
  WornFlower->SetVisibility(true);GiftPinned=true;
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_pin phase=%.3f world=%s head_relative=%s"),T,*Pose.ToString(),*WornFlower->GetRelativeTransform().ToString());
 }
 Gift->SetWorldTransform(Pose);Gift->SetVisibility(!GiftPinned);
 if(GiftWearActive && T>=13.6f){FinishReceiveGift(false,TEXT("pinned_and_released"));return true;}
 if(T>=2.8 && !GiftContactLogged){GiftContactLogged=true;UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX flower_contact t=%.3f object=%s hand=%s"),T,*Gift->GetComponentLocation().ToString(),*A->GetMesh()->GetSocketLocation(TEXT("手首_R")).ToString());}
 // Wearing/retaining the flower still has a separate gate, not claimed here.
 return true;
}

void ACourtyardContentLife::FinishReceiveGift(bool Returned,const TCHAR* Reason)
{
 if(ReceiveGiftSince<0)return;
 ReceiveGiftSince=-1;ReceiveGiftMotion=nullptr;GiftReturning=false;GiftContactLogged=false;GiftWearActive=false;
 GiftUntil=0;HoldingFlower=Returned;
 if(Returned && C.IsValid())ShowGift(true);else Gift->SetVisibility(false);
 if(ActiveTask==TEXT("GiftReceive"))ActiveTask=NAME_None;
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_finish returned=%d reason=%s receive_state_cleared=1"),Returned,Reason);
}
