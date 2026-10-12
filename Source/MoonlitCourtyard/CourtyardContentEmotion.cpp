#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentEnvironment.h"
#include "CourtyardGazeAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

namespace {
float Ease(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);}
}

void ACourtyardContentLife::BeginFixedEmotion()
{
 if(!C.IsValid())return;
 if(OwnsTravel){C->LocoStopRequested=true;ReleaseTravel();ArriveAction=NAME_None;}
 if(!EmotionOwnsPause){EmotionOldPause=FloorJourney?FloorOldPause:C->LocoLifePaused;EmotionOwnsPause=true;}
 C->LocoLifePaused=true;
 if(C->LocoState!=ACourtyardController::ELocoState::Idle)C->LocoStopRequested=true;
 C->CancelFace();PendingScene=NAME_None;EmotionBodyStarted=false;
 PostureEmotion=Emotion;
 EmotionBodyAfter=GetWorld()->GetTimeSeconds()+.4;EmotionFaceReportAt=0;
 // Finish an accepted player response; only blend out an ordinary idle gesture.
 if(C->ActiveAmbient && C->LivingBehavior<0 && !C->LivelyResponseActive){
  auto* A=Cast<ACharacter>(C->Emma());auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  if(I)I->Montage_Stop(.75f,C->ActiveAmbient);
 }
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX emotion_owner begin=%s old_mood=%s locomotion_stop_requested=%d"),*Emotion.ToString(),*C->Mood,C->LocoStopRequested);
}

void ACourtyardContentLife::ClearFixedEmotion()
{
 if(C.IsValid()){
  if(EmotionOwnsPause)C->LocoLifePaused=FloorJourney?true:EmotionOldPause;
  C->LocoNextDecision=FMath::Max(C->LocoNextDecision,double(GetWorld()->GetTimeSeconds()+12));
  C->CancelFace();
 }
 EmotionOwnsPause=false;EmotionBodyStarted=false;Emotion=NAME_None;Intensity=TargetIntensity=0;
}

void ACourtyardContentLife::TickFixedEmotion(float Dt,double Now)
{
 if(!C.IsValid() || !C->GetPawn() || !C->Emma())return;
 Pats.RemoveAll([Now](double T){return Now-T>75;});
 if(PatPending>0 && Now>=PatPending){if(Now-LastPat>=9.5 && Pats.Num()==1)Comfort(2);PatPending=-1;}
 if(Emotion.IsNone()){if(EmotionOwnsPause)ClearFixedEmotion();return;}
 const float Dist=FVector::Dist(C->GetPawn()->GetActorLocation(),C->Emma()->GetActorLocation());
 const bool Visible=PlayerVisible();
 const bool Angry=Emotion==TEXT("PatAnger") || Emotion==TEXT("DoorAnger");
 // Nearby inactivity alone is not a comforting visit. The player must actually
 // attend to her, rather than face away or leave a settings page open nearby.
 const auto* A=Cast<ACharacter>(C->Emma());
 const FVector Face=A?A->GetMesh()->GetSocketLocation(TEXT("頭")):C->Emma()->GetActorLocation()+FVector(0,0,65);
 const bool InGame=C->Page==ECourtyardPage::Game;
 const bool LookingAtHer=C->PlayerCameraManager &&
  FVector::DotProduct(C->PlayerCameraManager->GetCameraRotation().Vector(),(Face-C->PlayerCameraManager->GetCameraLocation()).GetSafeNormal())>.65f;
 // Deliberately sitting beside her is quiet company too. The real completed F
 // state, proximity and visibility establish it without forcing eye contact.
 const bool QuietSeatedCompany=C->Pose==ECourtyardPose::Seated && !C->PoseTransition && !C->IsSeatBusy();
 const bool Attending=InGame && (LookingAtHer || QuietSeatedCompany);
 if(Dist<250 && Visible && Attending && C->GetPawn()->GetVelocity().Size()<30)NearComfort+=Dt;else NearComfort=FMath::Max(0.,NearComfort-Dt);
 // Distance is not itself comfort: leaving someone who already feels abandoned cannot help.
 if(Angry && Dist>450)SpaceComfort+=Dt;else SpaceComfort=0;
 if(NearComfort>28)Comfort(1);if(Angry && SpaceComfort>45)Comfort(4);
 if(InGame && Dist<350 && Visible && C->GetControlRotation().Pitch>23 && SharedSkyVisible()){
  ScenicComfort+=Dt;if(ScenicComfort>16)Comfort(3);
  if(Environment.IsValid() && Environment->MeteorBrightness>.3f)Comfort(5);
 }else ScenicComfort=0;
 int Count=0;for(int N=0;N<6;++N)if(ComfortMask&(1<<N))++Count;
 const float Age=Now-EmotionSince;
 float Hold=8,End=45;
 if(Emotion==TEXT("Surprise")){Hold=1.8f;End=8;}
 else if(Emotion==TEXT("Overwhelmed")){Hold=3;End=13;}
 else if(Emotion==TEXT("PetalDespair")){Hold=6;End=30;}
 else if(Emotion==TEXT("DoorShy")){Hold=5;End=24;}
 else if(Emotion==TEXT("Reunion")){Hold=5;End=22;}
 else if(Emotion==TEXT("ReunionDeep")){Hold=9;End=30;}
 else if(Angry){Hold=25;End=100;}
 else if(Emotion==TEXT("Abandoned")){Hold=90;End=300;}
 const float Natural=1-Ease((Age-Hold)/(End-Hold));
 TargetIntensity=FMath::Min(TargetIntensity,Natural);
 if(Count>0 && (Emotion!=TEXT("Abandoned") || Count>=3))TargetIntensity=FMath::Max(0.f,TargetIntensity-Dt*(Angry?.018f:.026f));
 // Finite natural recovery remains possible without making the player perform a checklist.
 Intensity=FMath::FInterpConstantTo(Intensity,TargetIntensity,Dt,End<=45?.24f:.065f);
 if(Now>=EmotionReportAt){EmotionReportAt=Now+5;UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX emotion_state type=%s age=%.2f intensity=%.3f target=%.3f comforts=%d max_seconds=%.0f"),*Emotion.ToString(),Age,Intensity,TargetIntensity,Count,End);}
 if(Intensity<.012f){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX recovered type=%s age=%.2f mood=%s"),*Emotion.ToString(),Age,*C->Mood);ClearFixedEmotion();return;}
 // One expressive body phrase per event, not a loop of the same outburst.
 if(!EmotionBodyStarted && Now>=EmotionBodyAfter && Safe(false)){
  FName Body=Angry?TEXT("EmotionAngry"):Emotion==TEXT("Surprise")?TEXT("EmotionSurprise"):
   Emotion==TEXT("Overwhelmed") || Emotion==TEXT("DoorShy")?TEXT("EmotionShy"):
   Emotion==TEXT("Reunion") || Emotion==TEXT("ReunionDeep")?TEXT("EmotionReunion"):TEXT("EmotionSad");
  if(Age<End-3 && Play(Body,Body))EmotionBodyStarted=true;
 }
}

void ACourtyardContentLife::ComposeFixedPosture(float Dt,float* Angles)
{
 EmotionPostureBlend=FMath::FInterpConstantTo(EmotionPostureBlend,Emotion.IsNone()?0.f:FMath::Min(1.f,Intensity*2),Dt,1.4f);
 if(EmotionPostureBlend<=0)return;
 float P[PostureCount]={0};
 if(PostureEmotion==TEXT("PatAnger") || PostureEmotion==TEXT("DoorAnger")){
  P[ChestForward]=1.5f;P[ShoulderForwardL]=P[ShoulderForwardR]=3;P[ArmOutL]=P[ArmOutR]=2;P[HeadDown]=5;
 }else if(PostureEmotion==TEXT("Abandoned") || PostureEmotion==TEXT("PetalDespair")){
  P[ChestForward]=4;P[ShoulderDropL]=P[ShoulderDropR]=4;P[ShoulderForwardL]=P[ShoulderForwardR]=3;P[HeadDown]=7;
 }else if(PostureEmotion==TEXT("Overwhelmed") || PostureEmotion==TEXT("DoorShy")){
  P[ChestForward]=2;P[ShoulderDropL]=P[ShoulderDropR]=2;P[HeadDown]=5;P[HeadTiltRight]=2;
 }else if(PostureEmotion==TEXT("Reunion") || PostureEmotion==TEXT("ReunionDeep")){
  P[ChestForward]=-1;P[HeadTiltRight]=2;
 }
 for(int N=0;N<PostureCount;++N)Angles[N]=FMath::Lerp(Angles[N],P[N],Ease(EmotionPostureBlend));
}

void ACourtyardContentLife::ComposeFixedFace(TMap<FName,float>& Target)
{
 if(!FixEnabled || !C.IsValid())return;
 auto* A=Cast<ACharacter>(C->Emma());auto* M=A?A->GetMesh():nullptr;
 auto* I=M?M->GetAnimInstance():nullptr;auto* S=M?M->GetSkeletalMeshAsset():nullptr;if(!I || !S)return;
 const auto& Morphs=S->GetMorphTargets();const double Now=GetWorld()->GetTimeSeconds();
 auto Name=[&](int N)->FName{return Morphs.IsValidIndex(N-1)?Morphs[N-1]->GetFName():NAME_None;};
 auto Anim=[&](int N){return I->GetCurveValue(Name(N));};
 if(GiftWearActive && ReceiveGiftMotion && Emotion.IsNone()){
  const float T=I->Montage_GetPosition(ReceiveGiftMotion);
  const float Thanks=Ease((T-2.5f)/.8f)*Ease((13.4f-T)/1.3f);
  const float Shy=Ease((T-8.5f)/.8f)*Ease((12.2f-T)/1.2f);
  Target.Add(Name(39),0);Target.Add(Name(40),.12f*Thanks);
  Target.Add(Name(18),.23f*Thanks);Target.Add(Name(45),.18f*Thanks);Target.Add(Name(63),.20f*Shy);
 }
 if(ReadingSeat && SeatMotion && Emotion.IsNone()){
  // This seated clip has no authored face. Ordinary face phrases yield to any
  // montage, so the candidate seating owner supplies only its doze/wake eyes.
  float W=Ease(float((Now-SeatSince-1)/2.));
  if(ReadingWaking)W*=1-Ease(float((Now-ReadingWakeAt)/1.6));
  Target.Add(Name(25),FMath::Max(Anim(25),.94f*W));
  Target.Add(Name(18),.03f*W);Target.Add(Name(39),0.f);
 }
 UAnimMontage* Current=I->GetCurrentActiveMontage();
 const bool Daydream=(Current && Current->GetName().Contains(TEXT("Daydream"))) || C->LivelyBodyId==TEXT("A3") || C->LivelyFaceId==TEXT("A3");
 if(Daydream && Emotion.IsNone()){
  // The source clips stay intact. This exact action no longer erases the eye highlight.
  Target.Add(Name(39),0.f);Target.Add(Name(40),.08f);
  Target.Add(Name(18),0.f);Target.Add(Name(45),0.f);Target.Add(Name(7),FMath::Min(.16f,Anim(7)));
  // The unmodified face has an upturned resting mouth; a small authored lower-mouth
  // channel makes absorbed/lonely thought neutral rather than a fixed smile.
  Target.Add(Name(21),C->Mood==TEXT("happy")?.08f:C->Mood==TEXT("lonely")?.20f:.12f);
  if(C->Mood==TEXT("sleepy"))Target.Add(Name(25),FMath::Max(Anim(25),.16f));
  if(C->Mood==TEXT("lonely"))Target.Add(Name(52),.20f);
 }
 if(Emotion.IsNone())return;
 const float Age=Now-EmotionSince;
 const float Gate=Ease(Age/.55f)*Ease(Intensity/.15f);
 const float Strength=FMath::Clamp(Intensity,.15f,1.f);
 TMap<int32,float> Face;
 if(Emotion==TEXT("PatAnger") || Emotion==TEXT("DoorAnger")){
  // A short protest can open the mouth; the sustained state is a closed, lowered mouth.
  // Holding the open phoneme for the whole event reads as cheerful conversation.
  const float Protest=Ease((Age-.55f)/.25f)*(1-Ease((Age-1.15f)/.35f));
  Face={{50,.92f*Strength},{46,1.f*Strength},{48,.18f*Strength},{34,.22f*Strength},{21,.78f*Strength},{16,.65f*Strength},{14,.38f*Strength*Protest}};
 }
 else if(Emotion==TEXT("Surprise"))Face={{30,.72f*Strength},{47,.55f*Strength},{13,.46f*Strength},{37,.12f*Strength}};
 else if(Emotion==TEXT("Overwhelmed") || Emotion==TEXT("DoorShy"))Face={{66,.88f*Strength},{72,.28f*Strength}};
 else if(Emotion==TEXT("Abandoned"))Face={{60,.82f*Strength},{69,.75f*Strength},{70,.35f*Strength}};
 else if(Emotion==TEXT("PetalDespair"))Face={{59,.92f*Strength},{70,.55f*Strength}};
 else if(Emotion==TEXT("ReunionDeep")){
  const float Phase=Age<7?1-Ease((Age-5.5f)/1.5f):Ease((Age-7)/1.3f);
  Face={{Age<7?59:64,(Age<7?.8f:.72f)*Strength*Phase},{69,.65f*Strength}};
 }else if(Emotion==TEXT("Reunion"))Face={{64,.65f*Strength},{69,.65f*Strength}};
 else Face={{53,.85f*Strength},{69,.45f*Strength}};
 // Preserve biological blinks only where the chosen expression leaves the eyes open.
 const bool Closed=Face.Contains(64)||Face.Contains(66);
 const bool Wide=Emotion==TEXT("Surprise");
 const float Blink=FMath::Max(Anim(25),Target.FindRef(Name(25)));
 if(!Closed && !Wide){Face.Add(25,Blink);for(int N:{30,32,34,35,50,52,53,54,55,56,57,58,59,60,61,62,65,67,68})if(float* V=Face.Find(N))*V*=1-Blink;}
 // This is the sole final composition owner during an event. Suppress authored idle
 // and mood channels too; max() with the old smile would reintroduce the defect.
 for(int N=1;N<=72;++N)if(Morphs.IsValidIndex(N-1)){
  const FName Key=Name(N);const float Existing=Target.Contains(Key)?Target[Key]:Anim(N);
  Target.Add(Key,FMath::Lerp(Existing,Face.FindRef(N),Gate));
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) && Now>=EmotionFaceReportAt){
  EmotionFaceReportAt=Now+1;
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX face_owner emotion=%s gate=%.3f smile=%.3f angry=%.3f despair=%.3f surprise=%.3f tears=%.3f montage=%s"),*Emotion.ToString(),Gate,Target.FindRef(Name(18)),Target.FindRef(Name(51)),Target.FindRef(Name(60)),Target.FindRef(Name(57)),Target.FindRef(Name(69)),*GetNameSafe(Current));
 }
}
