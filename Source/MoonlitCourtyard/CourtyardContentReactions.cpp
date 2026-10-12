#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentEnvironment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

namespace {
FLivelyFacePhase CF(float Start,float End,TMap<int32,float> Values,float Fade=.5f){FLivelyFacePhase P;P.Start=Start;P.End=End;P.Values=MoveTemp(Values);P.Fade=Fade;return P;}
}
bool ACourtyardContentLife::PlayerVisible() const
{
 if(!C.IsValid() || !C->PlayerCameraManager || !C->Emma())return false;
 const auto* Emma=Cast<ACharacter>(C->Emma());const FVector From=Emma->GetMesh()->GetSocketLocation(TEXT("頭")),To=C->PlayerCameraManager->GetCameraLocation();
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ContentVisibility),false,Emma);Q.AddIgnoredActor(C->GetPawn());
 return !GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Q);
}
bool ACourtyardContentLife::SharedSkyVisible() const
{
 if(!C.IsValid() || !C->PlayerCameraManager || !C->Emma())return false;
 const FVector P=C->PlayerCameraManager->GetCameraLocation();
 const FVector Target=P+C->GetControlRotation().Vector()*5000;
 FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(C->GetPawn());Q.AddIgnoredActor(C->Emma());
 const auto* A=Cast<ACharacter>(C->Emma());if(!A)return false;
 return !GetWorld()->LineTraceSingleByChannel(H,P,Target,ECC_Visibility,Q) && !GetWorld()->LineTraceSingleByChannel(H,A->GetMesh()->GetSocketLocation(TEXT("頭")),Target,ECC_Visibility,Q);
}
void ACourtyardContentLife::ContextFace(FName Task)
{
 TArray<FLivelyFacePhase> P;float Length=7;
 if(Task==TEXT("HearthWarm") || Task==TEXT("WarmHands"))P={CF(.8,3,{{45,.32f},{18,.20f}}),CF(3.2,5.8,{{63,.28f}})};
 else if(Task==TEXT("RunnerChuckle")){P={CF(.4,2.3,{{64,.42f}}),CF(2.5,4.9,{{45,.3f},{18,.25f}})};Length=5.5f;}
 else if(Task==TEXT("RearNotice")){P=C->Mood==TEXT("happy")?TArray<FLivelyFacePhase>{CF(.1,1.2,{{57,.36f}},.2f),CF(1.5,3.8,{{45,.4f},{18,.25f}})}:TArray<FLivelyFacePhase>{CF(.1,1.2,{{57,.3f}},.2f),CF(1.5,3.8,{{66,.3f}})};Length=4.5f;}
 else if(Task==TEXT("GustSneeze")){P={CF(.5,2.4,{{35,.40f},{72,.25f}},.25f)};Length=4;}
 else if(Task==TEXT("ReceiveFlower"))P={CF(1.6,3.1,{{45,.35f},{18,.25f}}),CF(3.5,6.4,{{63,.38f}})};
 else if(Task==TEXT("FlowerOffering")){P={CF(.5,2.5,{{45,.35f},{18,.25f}}),CF(2.7,4.5,{{63,.35f}})};Length=5;}
 else if(Task==TEXT("RunnerWave")){P={CF(.9,4.9,{{45,.40f},{18,.3f}})};Length=6;}
 else P={CF(.7,5.5,{{45,.28f},{18,.15f}})};
 if(Emotion.IsNone())C->StartFace(FName(*(TEXT("ContentAction_")+Task.ToString())),P,Length,TEXT(""),3);
}
void ACourtyardContentLife::TickNewReactions(float Dt,double Now)
{
 if(BlocksOrdinaryBehavior())return;
 APawn* P=C->GetPawn();auto* E=C->Emma();if(!P || !E)return;
 const FVector To=P->GetActorLocation()-E->GetActorLocation();const float Dist=To.Size2D(),Speed=P->GetVelocity().Size2D();const bool SameFloor=FMath::Abs(To.Z)<100,Visible=SameFloor && PlayerVisible();
 if(!PendingScene.IsNone() && Now>PendingUntil)PendingScene=NAME_None;
 if(PendingScene==TEXT("GustFinish") && Now>=NextGustSneeze){SetEmotion(TEXT("Overwhelmed"),TEXT("wind_two_completed_sneezes"));PendingScene=NAME_None;}
 if(PendingScene==TEXT("MildProtest") && Safe(false)){
  if(C->StartFace(TEXT("ContentMildProtest"),{CF(0,3,{{50,.38f},{14,.18f}})},3.5f,TEXT(""),3))PendingScene=NAME_None;
 }
 if(PendingScene==TEXT("RearNotice") && (C->LivelyBodyId==TEXT("B4") || C->LivelyBodyId==TEXT("C17")))PendingScene=NAME_None; // an existing owner already responded
 if(PendingScene==TEXT("RearNotice") && Safe() && Play(TEXT("RearNotice"),TEXT("RearNotice"))){PendingScene=NAME_None;Cooldowns.Add(TEXT("RearNotice"),Now+150);}
 const float Rear=FVector::DotProduct(E->GetActorForwardVector(),To.GetSafeNormal2D());
 if(Visible && PreviousDistance>185 && Dist<185 && Speed>20 && Speed<220 && Rear<-.60 && Cooldowns.FindRef(TEXT("RearNotice"))<Now){PendingScene=TEXT("RearNotice");PendingUntil=Now+9;}

 // A short observation window, not a permanently tracking face or an every-frame random choice.
 if(Visible && Dist<360 && Speed>250){RunnerWatching+=Dt;RunnerLastNear=Now;RunnerUntil=Now+12;}
 else RunnerWatching=FMath::Max(0.,RunnerWatching-Dt*.5);
 if(RunnerWatching>2.2 && Now>=NextRunnerFace && Safe() && Play(TEXT("RunnerChuckle"),TEXT("RunnerChuckle"))){
  NextRunnerFace=Now+120;RunnerWatching=0;C->GazePoi=P->GetActorLocation()+FVector(0,0,58);C->GazePoiUntil=Now+2.5;C->GazePoiHead=.45f;
 }
 if(Visible && Dist>480 && Dist<850 && PreviousDistance<=480 && Speed>250 && Now<RunnerUntil && Now-RunnerLastNear<8 && Cooldowns.FindRef(TEXT("RunnerDeparture"))<Now){PendingScene=TEXT("RunnerDeparture");PendingUntil=Now+12;}
 if(PendingScene==TEXT("RunnerDeparture") && Safe() && Visible && Dist<950){
  PendingScene=NAME_None;Cooldowns.Add(TEXT("RunnerDeparture"),Now+300);
  if(C->Mood==TEXT("happy") || C->Mood==TEXT("lonely")){
   FVector Goal=P->GetActorLocation()-To.GetSafeNormal2D()*190;Goal.Z=E->GetActorLocation().Z;
   Travel(Goal,(P->GetActorLocation()-Goal).Rotation().Yaw,TEXT("FollowRunner"),TEXT("RunnerWave"));
  }else Play(TEXT("RunnerWave"),TEXT("RunnerWave"));
 }
 // The two sneezes belong to one real gust. No forced repeat of unrelated old idle actions.
 if(Environment.IsValid() && Environment->CurrentWind>.7f && Dist<700 && !C->LocoIndoors() && Cooldowns.FindRef(TEXT("GustSneezes"))<Now && Safe()){
  if(Play(TEXT("Sneeze"),TEXT("GustSneeze"))){GustSneezes=1;LastWindContext=Now;NextGustSneeze=Now+6;Cooldowns.Add(TEXT("GustSneezes"),Now+900);}
 }
 if(GustSneezes==1 && Now>=NextGustSneeze){
  if(Now-LastWindContext>25)GustSneezes=0;
  else if(Safe() && Play(TEXT("Sneeze"),TEXT("GustSneeze"))){GustSneezes=2;PendingScene=TEXT("GustFinish");NextGustSneeze=Now+4.6;PendingUntil=Now+9;}
 }
}
