#include "CourtyardController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

bool ACourtyardController::CloseFlowerContext()
{
 FVector E,F;if(!FaceFrame(E,F))return false;
 for(auto Weak:FlowerComponents)if(auto* C=Weak.Get())if(C->GetStaticMesh() && C->IsVisible()){
  auto* ISM=Cast<UInstancedStaticMeshComponent>(C);const int32 Count=ISM?ISM->GetInstanceCount():1;
  for(int32 N=0;N<Count;++N){FTransform T=C->GetComponentTransform();if(ISM && !ISM->GetInstanceTransform(N,T,true))continue;
   const auto B=C->GetStaticMesh()->GetBounds();const FVector P=T.TransformPosition(B.Origin+FVector(0,0,B.BoxExtent.Z*.65f));const FVector D=P-E;
   const float Down=FMath::RadiansToDegrees(FMath::Atan2(-D.Z,D.Size2D()));
   if(D.Size2D()<150 && D.Size2D()>45 && Down>5 && Down<42 && FVector::DotProduct(F,D.GetSafeNormal2D())>.96f && VisibleTarget(P,C->GetOwner()))return true;
  }
 }
 return false;
}
bool ACourtyardController::MoonPinchContext() const
{
 FVector E,F;if(!FaceFrame(E,F))return false;
 // The two small moons' world directions in Refinement20260927/QuietCirrus.hlsl.
 for(const FVector& Raw:{FVector(.32,-.79,.52),FVector(-.57,-.68,.46)}){
  const FVector D=Raw.GetSafeNormal();if(FVector::DotProduct(F,D.GetSafeNormal2D())>.978f && VisibleTarget(E+D*5000))return true;
 }
 return false;
}
bool ACourtyardController::StartP3(FName Id,bool Forced)
{
 FName Name;
 if(Id==TEXT("C1"))Name=TEXT("P3Prayer");
 if(Id==TEXT("C2"))Name=TEXT("P3PointMeteor");
 if(Id==TEXT("C5"))Name=TEXT("P3PinchMoon");
 if(Id==TEXT("C7"))Name=TEXT("P3SmellFlower");
 if(Id==TEXT("C8"))Name=TEXT("P3HatShake");
 if(Id==TEXT("C11"))Name=TEXT("P3Firefly");
 if(Id==TEXT("C12"))Name=TEXT("P3MothWatch");
 if(Id==TEXT("C13"))Name=P3GustHairPending?TEXT("P3WindHair"):TEXT("P3WindFace");
 if(Id==TEXT("C14"))Name=TEXT("P3ListenChime");
 if(Id==TEXT("D2"))Name=TEXT("P3Sneeze");
 if(Name.IsNone())return false;
 if(!Forced){
  if(Id==TEXT("C5") && (!PlayerWatching || Mood!=TEXT("happy") || !MoonPinchContext()))return false;
  if(Id==TEXT("C7") && !CloseFlowerContext())return false;
  if(Id==TEXT("C8") && !P3HatAttached)return false;
  if(Id==TEXT("C11") && Mood!=TEXT("calm") && Mood!=TEXT("happy"))return false;
 }
 if(!PlayLivelyHead(Id,Name))return false;
 const double Now=GetWorld()->GetTimeSeconds();P3MotionStart=Now;
 if(Id==TEXT("C11"))P3FlyVisitUntil=Now+9;
 if(Id==TEXT("C13")){P3GustHairPending=Name==TEXT("P3WindFace");P3HairUntil=P3GustHairPending?Now+19:-1;}
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY p3_motion id=%s name=%s review=%d time=%.3f"),*Id.ToString(),*Name.ToString(),Forced,Now);
 return true;
}
bool ACourtyardController::TryP3Reaction()
{
 if(LivelyStage<3 || !P3Environment || !LivelySafe(false))return false;
 const double Now=GetWorld()->GetTimeSeconds();auto* A=Emma();if(!A)return false;
 if(Now<P3SuddenUntil && StartLively(TEXT("C17"))){P3SuddenUntil=-1;return true;}
 if(Now<P3GustReplyUntil && StartLively(TEXT("C13"))){P3GustReplyUntil=-1;return true;}
 if(Now<P3ChimeUntil && LivingChime.IsValid() && FVector::Dist(A->GetActorLocation(),LivingChime->GetActorLocation())<1000 && (Mood==TEXT("happy") || Mood==TEXT("calm")) && VisibleTarget(LivingChime->GetActorLocation(),LivingChime.Get()) && StartLively(TEXT("C14"))){P3ChimeUntil=-1;return true;}
 if(P3GustHairPending && Now<P3HairUntil && LivelyOn(TEXT("C13"))){
  // This is the second phase of the same wind response, not a second random choice.
  if(PlayLivelyHead(TEXT("C13"),TEXT("P3WindHair"))){P3GustHairPending=false;P3MotionStart=Now;return true;}
 }else if(Now>=P3HairUntil)P3GustHairPending=false;
 if(Now<P3WishUntil && (Mood==TEXT("happy") || Mood==TEXT("calm")) && StartLively(TEXT("C1"))){P3WishUntil=-1;return true;}
 if(P3HatAttached && Now>P3HatNoticeAt && StartLively(TEXT("C8")))return true;
 if(Now<P3FlyVisitUntil-4 && Now-P3FlyVisitStart>2 && FVector::Dist(A->GetActorLocation()+FVector(0,0,60),P3GuidedFly)<150 && VisibleTarget(P3GuidedFly) && StartLively(TEXT("C11")))return true;
 for(const FVector& P:P3LampCenters){FVector E,F;if(!FaceFrame(E,F))break;const FVector D=P+FVector(0,0,20)-E;
  const bool Near=D.Size()<400 || (PlayerCharacter && FVector::Dist(PlayerCharacter->GetActorLocation(),P)<400);
  if(Near && D.Z>0 && D.Z/D.Size()<.38f && FVector::DotProduct(F,D.GetSafeNormal2D())>.97f && VisibleTarget(P) && StartLively(TEXT("C12")))return true;
 }
 if(Now<P3NextAmbient || !SeenOrWaited(P3Defer))return false;P3NextAmbient=Now+FMath::FRandRange(30.f,55.f);
 if(StartLively(TEXT("D2")))return true;
 if((Mood==TEXT("calm") || Mood==TEXT("happy")) && StartLively(TEXT("C7")))return true;
 if(StartLively(TEXT("C5")))return true;
 return false;
}
