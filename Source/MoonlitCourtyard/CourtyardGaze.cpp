#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "CourtyardGazeAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"

namespace {
float Smooth(float Current,float Target,float Dt,float Speed){return FMath::Lerp(Current,Target,1-FMath::Exp(-Speed*FMath::Min(Dt,.1f)));}
float Ease(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
struct FGazePolicy{const TCHAR* Prefix;float Head,Eye;int32 Category;};
// 0 idle, 1 player reaction, 2 elsewhere, 3 absorbed, 4 physical/default, 5 departing, 6 locomotion.
FGazePolicy Policy(FString Name,bool Authored,bool Walking)
{
 if(Walking)return {TEXT(""),0,1,6};if(!Authored)return {TEXT(""),1,1,0};
 Name.RemoveFromStart(TEXT("AM_Emma_"));Name.RemoveFromStart(TEXT("Lively_"));
 // Candidate-only duplicates retain the gaze policy of their unchanged body source.
 if(Name==TEXT("AM_DaydreamFixed"))return {TEXT("AM_DaydreamFixed"),0,0,3};
 if(Name==TEXT("AM_EmotionSad"))Name=TEXT("DaydreamHead");
 if(Name==TEXT("AM_EmotionSurprise"))Name=TEXT("StartleBody");
 if(Name==TEXT("AM_EmotionShy"))Name=TEXT("ShyAway");
 if(Name==TEXT("AM_EmotionReunion"))Name=TEXT("Welcome");
 static const FGazePolicy Rules[]={
  {TEXT("Response"),1,1,1},{TEXT("HeadPatSmile"),1,1,1},{TEXT("Chuckle"),1,1,1},{TEXT("Proud"),1,1,1},
  {TEXT("Mumble"),1,1,1},{TEXT("SuppressYawn"),1,1,1},{TEXT("PoutHead"),1,1,1},{TEXT("ShyAway"),1,1,1},
  {TEXT("Found"),1,1,1},{TEXT("HighFive"),1,1,1},{TEXT("Wave"),1,1,1},{TEXT("BodyWave"),1,1,1},
  {TEXT("Welcome"),1,1,1},{TEXT("Reunion"),1,1,1},{TEXT("ByeWave"),1,1,1},{TEXT("GoodnightWave"),1,1,1},{TEXT("LonelyWave"),1,1,1},{TEXT("ThankfulGreeting"),1,1,1},
  {TEXT("SkyGaze"),0,0,2},{TEXT("SkyWatch"),0,0,2},{TEXT("MeteorNotice"),0,0,2},{TEXT("AncientGaze"),0,0,2},{TEXT("CheckPlayerSky"),0,0,2},
  {TEXT("CountStars"),0,0,2},{TEXT("CloudWatch"),0,0,2},{TEXT("P3MothWatch"),0,0,2},{TEXT("Doze"),0,0,2},
  {TEXT("P3Sneeze"),0,0,2},{TEXT("StartleBody"),0,0,2},{TEXT("SmallStartle"),0,0,2},{TEXT("P3Prayer"),0,0,2},{TEXT("P3Firefly"),0,0,2},
  {TEXT("HumHead"),.5f,.5f,3},{TEXT("MusicNod"),.5f,.5f,3},{TEXT("DaydreamHead"),.5f,.5f,3},{TEXT("P3WindFace"),.5f,.5f,3},
  {TEXT("Depart"),0,1,5}};
 for(const auto& R:Rules)if(Name.StartsWith(R.Prefix))return R;
 return {TEXT(""),.5f,1,4};
}
}
void ACourtyardController::CourtyardGazeLayer(int32 Mode)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 GazeLayerEnabled=Mode!=0;UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE layer=%d"),GazeLayerEnabled);
}
void ACourtyardController::CourtyardGaze(bool Enabled)
{
 GazeEnabled=Enabled;UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE enabled=%d"),Enabled);
}
void ACourtyardController::CourtyardReviewGazeMode(int32 Mode)
{
 if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"))){GazeReviewZero=Mode==0;UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE zero_passthrough=%d"),GazeReviewZero);}
}
void ACourtyardController::TickGaze(float Dt)
{
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;auto* Main=M?M->GetAnimInstance():nullptr;if(!Main)return;
 const double Now=GetWorld()->GetTimeSeconds();const bool Review=FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"));
 if(!GazeInstance.IsValid()&&!GazeFailed){
  auto* Class=GazeClass.LoadSynchronous();if(!Class){GazeFailed=true;UE_LOG(LogTemp,Warning,TEXT("COURTYARD_GAZE missing new postprocess asset; original animation kept"));return;}
  M->SetOverridePostProcessAnimBP(Class,false);M->InitializeAnimScriptInstance(false);
  auto* New=Cast<UCourtyardGazeAnimInstance>(M->GetPostProcessInstance());
  if(!New||!New->GetLinkedInputPoseNode()||Main!=M->GetAnimInstance()){
   GazeFailed=true;M->SetDisablePostProcessBlueprint(true);UE_LOG(LogTemp,Error,TEXT("COURTYARD_GAZE attach refused main_same=%d input=%d"),Main==M->GetAnimInstance(),New&&New->GetLinkedInputPoseNode());return;}
  GazeInstance=New;GazeOriginalGlance=BPBool(A,TEXT("GlanceEnabled"));GazeNextSaccade=Now+1.2;GazeNextAway=Now+8;
  // Mesh tick follows controller decisions, so fades see a montage started this frame.
  M->AddTickPrerequisiteActor(this);
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE attached main_same=1 input=1 original=%s post=%s"),*Main->GetClass()->GetName(),*New->GetClass()->GetName());
 }
 auto* G=GazeInstance.Get();if(!G)return;
 if(GazeEnabled){if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("GlanceEnabled")))P->SetPropertyValue_InContainer(A,false);GazeOwnsGlance=true;}
 else if(GazeOwnsGlance&&GazeHeadWeight<.001f&&GazeEyeWeight<.001f&&GazeApplied.Size()<.001){if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("GlanceEnabled")))P->SetPropertyValue_InContainer(A,GazeOriginalGlance);GazeOwnsGlance=false;}
 const FVector Head=M->GetSocketLocation(TEXT("頭"));FVector V;FRotator View;GetPlayerViewPoint(V,View);
 // Locomotion rework: a short look at a point (where she is going, something she passes, flowers at a pause) replaces the player.
 const bool Poi=GazeLayerEnabled&&Now<GazePoiUntil;if(Poi)V=GazePoi;
 const FVector D=M->GetComponentTransform().InverseTransformVectorNoScale((V-Head).GetSafeNormal());
 // Confirmed anatomical frame: right=-X, forward=+Y, up=+Z. No eye-to-head vector inference.
 const float RawYaw=FMath::RadiansToDegrees(FMath::Atan2(-D.X,D.Y));const float RawPitch=FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(D.Z,-1.,1.)));
 bool Visible=false,FarVisible=false;const float Distance=FVector::Dist(V,Head);
 if(Distance>45&&Distance<800){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(GazeVisibility),false,A);
  const bool Clear=!GetWorld()->LineTraceSingleByChannel(H,Head,V,ECC_Visibility,Q)||H.GetActor()==PlayerCharacter;Visible=Clear&&Distance<420;FarVisible=Clear&&Distance>=420;}
 if(Poi){Visible=true;FarVisible=false;GazeAwayUntil=FMath::Min(GazeAwayUntil,Now);}
 auto* Montage=Main->GetCurrentActiveMontage();
 if(Montage)GazeLastMontage=Montage->GetName();else if(!Main->IsAnyMontagePlaying())GazeLastMontage.Empty();
 const FString Name=GazeLastMontage; // The previous intent remains valid during its blend-out tail.
 const bool Walking=BPBool(A,TEXT("TravelActive"))||A->GetVelocity().Size()>2|| (Montage&&(Name.Contains(TEXT("Walk"))||Name.Contains(TEXT("Arc"))||Name.Contains(TEXT("Loco"))));
 const bool Authored=Main->IsAnyMontagePlaying()&&!Walking;
 const bool Nearby=GazeEnabled&&!GazeReviewZero&&Visible&&Page==ECourtyardPage::Game;
 // Presence (S2). Walking start/arrival edges, a look before leaving, and glances across the courtyard.
 if(Walking&&!GazeWasWalking)GazeWalkStarted=Now;
 GazeWasWalking=Walking;
 const bool DepartLook=GazeLayerEnabled&&Now<GazeDepartLookUntil;
 if(DepartLook)GazeAwayUntil=FMath::Min(GazeAwayUntil,Now);
 // A real arrival at a stop (ArrivalTime, set once her walk ends with the next decision >9 s away), not the short
 // pauses between walk segments: she looks for the player once.
 if(GazeLayerEnabled&&ArrivalTime!=GazeHandledArrival&&Now-ArrivalTime<2.5&&!Walking&&!Main->IsAnyMontagePlaying()){
  if(Nearby&&FMath::Abs(RawYaw)>100.f){GazeHandledArrival=ArrivalTime;GazeNextRear=FMath::Min(GazeNextRear,Now+.4);UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE arrival player_behind yaw=%.0f t=%.2f"),RawYaw,Now);}
  else if(Nearby){
   FLivelyFacePhase ArrivalFace;ArrivalFace.Start=0;ArrivalFace.End=.7f;ArrivalFace.Fade=.2f;ArrivalFace.Values={{45,.35f},{18,.25f}};  // P1 gentle 'there you are' (expression remap batch 3)
   if(StartFace(TEXT("GazeArrival"),{ArrivalFace},.9f,TEXT(""),1)){GazeHandledArrival=ArrivalTime;GazeNoticeAt=Now;GazeAwayUntil=FMath::Min(GazeAwayUntil,Now);UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE arrival notice yaw=%.0f t=%.2f"),RawYaw,Now);}
  }
 }
 if(GazeLayerEnabled&&GazeEnabled&&!GazeReviewZero&&FarVisible&&!Walking&&!Main->IsAnyMontagePlaying()&&Page==ECourtyardPage::Game&&FMath::Abs(RawYaw)<=100.f&&Now>=GazeNextDistant&&Now>=GazeDistantUntil){
  GazeDistantUntil=Now+FMath::FRandRange(1.5f,2.f);GazeNextDistant=Now+FMath::FRandRange(15.f,25.f);
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE distant_glance distance=%.0f yaw=%.0f t=%.2f"),Distance,RawYaw,Now);
 }
 const bool Distant=FarVisible&&Now<GazeDistantUntil&&!Walking;
 // Eye micro-movement (2026-10-05): with the eyes now turning with the head they looked fixed, so the eyes get their
 // own small life. Micro-saccades around the current fixation, an occasional new fixation on the player's face
 // (between eyes and mouth), and a rare short glance aside that returns. Intervals and sizes are random; mood sets
 // the pace (happy livelier, sleepy slower and smaller, lonely fixates a little lower).
 if(Now>=GazeNextSaccade){
  const bool Sleepy=Mood==TEXT("sleepy"),Lonely=Mood==TEXT("lonely");
  const float Pace=Mood==TEXT("happy")?.8f:Sleepy?1.5f:Lonely?1.25f:1.f,Size=Sleepy?.8f:1.f;const float Roll=FMath::FRand();
  if(GazeSaccadeReturn){GazeSaccade=GazeFixation;GazeSaccadeReturn=false;GazeNextSaccade=Now+FMath::FRandRange(.3f,.9f)*Pace;}
  else if(Roll<.55f){GazeSaccade=GazeFixation+FVector2D(FMath::FRandRange(-1.2f,1.2f),FMath::FRandRange(-.8f,.8f))*Size;GazeNextSaccade=Now+FMath::FRandRange(.35f,1.3f)*Pace;}
  else if(Roll<.88f){GazeFixation=FVector2D(FMath::FRandRange(-2.2f,2.2f),FMath::FRand()<.5f?FMath::FRandRange(-3.f,-1.5f):FMath::FRandRange(-.3f,1.f))*Size;
   if(Lonely)GazeFixation.Y-=1.f;GazeSaccade=GazeFixation;GazeNextSaccade=Now+FMath::FRandRange(.6f,1.8f)*Pace;}
  else{GazeSaccade=GazeFixation+FVector2D((FMath::RandBool()?1:-1)*FMath::FRandRange(3.5f,6.f),FMath::FRandRange(-2.f,1.5f))*Size;GazeSaccadeReturn=true;GazeNextSaccade=Now+FMath::FRandRange(.3f,.7f);}
 }
 // No random look-away while she is greeting or answering the player (category 1): it read as looking past the player
 // at the end of the welcome. The aversion waits until the gesture is over.
 if(Policy(Name,Authored,Walking).Category==1){GazeAwayUntil=FMath::Min(GazeAwayUntil,Now);if(Now>=GazeNextAway)GazeNextAway=Now+FMath::FRandRange(2.f,4.f);}
 if(Now>=GazeNextAway){GazeAwayUntil=Now+FMath::FRandRange(.8f,2.5f);GazeAwaySign=FMath::RandBool()?1:-1;
  const int Direction=FMath::RandRange(0,4);const float Amplitude=FMath::FRandRange(8.f,20.f);
  FHitResult Roof;FCollisionQueryParams RoofQuery(SCENE_QUERY_STAT(GazeSkyOpening),false,A);
  const bool Outside=!GetWorld()->LineTraceSingleByChannel(Roof,Head,Head+FVector(0,0,1200),ECC_Visibility,RoofQuery);
  GazeAwayOffset=FVector2D(GazeAwaySign*Amplitude,Direction<2?0.f:Direction==4&&Outside?FMath::FRandRange(5.f,10.f):-FMath::FRandRange(4.f,9.f));
  GazeNextAway=Now+(Mood==TEXT("happy")?FMath::FRandRange(10.f,12.f):Mood==TEXT("lonely")?FMath::FRandRange(6.f,8.f):FMath::FRandRange(7.f,11.f));}
 // Rear glimpses are rare bounded acknowledgements, never a lock through the back of the head.
 const bool Rear=FMath::Abs(RawYaw)>(GazeLayerEnabled?100.f:80.f);
 if(Rear && GazeRearEntered<0){GazeRearEntered=Now;GazeRearSide=RawYaw<0?-1.f:1.f;}
 if(!Rear)GazeRearEntered=-1;
 if(Rear&&Nearby&&Now>=GazeNextRear){GazeRearUntil=Now+FMath::FRandRange(1.5f,2.5f);GazeNextRear=Now+FMath::FRandRange(10.f,18.f);}
 const bool RearGlance=Rear&&Now<GazeRearUntil;
 const bool RearHold=GazeLayerEnabled&&Rear&&Now-GazeRearEntered<1.25;
 const bool Away=Now<GazeAwayUntil;
 const float MaxYaw=GazeLayerEnabled?55.f:35.f;
 float Y=FMath::Clamp(Rear?GazeRearSide*MaxYaw:RawYaw,-MaxYaw,MaxYaw),P=FMath::Clamp(RawPitch,-15.f,15.f);
 if(Away&&!Rear){Y=FMath::Clamp(Y+(GazeLayerEnabled?GazeAwayOffset.X:GazeAwaySign*14),-MaxYaw,MaxYaw);P=FMath::Clamp(P+(GazeLayerEnabled?GazeAwayOffset.Y:-5),-15.f,15.f);}
 if(GazeLayerEnabled&&Walking&&Now-GazeWalkStarted<3.)Y=FMath::Clamp(Y,-50.f,50.f);
 // Head and eyes turn together (2026-10-05). The head used to wait 0.4 s and smooth at less than half the eye speed,
 // so the eyes sat at their 12 deg limit for ~1 s before the head followed. Now the head starts at once, and the eyes
 // slow to the head's speed in proportion to the head weight: eye-only cases (head weight 0: walking, departing,
 // rear) keep the original fast eyes, and the small saccades keep their own fast speed.
 const float HeadSpeed=Mood==TEXT("sleepy")?3.2f:5.5f,EyeFast=Mood==TEXT("sleepy")?7.f:13.f;
 const float EyeSpeed=FMath::Lerp(EyeFast,HeadSpeed,FMath::Clamp(GazeHeadWeight,0.f,1.f));
 GazeHeadStage.X=Smooth(GazeHeadStage.X,Y,Dt,HeadSpeed);GazeHeadStage.Y=Smooth(GazeHeadStage.Y,P,Dt,HeadSpeed);
 GazeHead.X=Smooth(GazeHead.X,GazeHeadStage.X,Dt,HeadSpeed);GazeHead.Y=Smooth(GazeHead.Y,GazeHeadStage.Y,Dt,HeadSpeed);
 const FVector2D ET(Rear?GazeRearSide*67.f:Away?Y:RawYaw, Away?P:RawPitch);
 GazeEyeStage.X=Smooth(GazeEyeStage.X,ET.X,Dt,EyeSpeed);GazeEyeStage.Y=Smooth(GazeEyeStage.Y,ET.Y,Dt,EyeSpeed);
 // Saccades are quick jumps (~80 ms), not glides; a slow ~0.3 deg drift keeps the eyes from ever sitting perfectly still.
 const float SaccadeSpeed=Mood==TEXT("sleepy")?20.f:28.f;
 const FVector2D Micro=GazeSaccadeNow;
 GazeSaccadeNow.X=Smooth(GazeSaccadeNow.X,GazeSaccade.X,Dt,SaccadeSpeed);GazeSaccadeNow.Y=Smooth(GazeSaccadeNow.Y,GazeSaccade.Y,Dt,SaccadeSpeed);
 const FVector2D Drift(.35f*FMath::PerlinNoise1D(float(Now)*.6f),.25f*FMath::PerlinNoise1D(float(Now)*.5f+37.f));
 GazeEye.X=Smooth(GazeEye.X-Micro.X-GazeEyeDrift.X,GazeEyeStage.X,Dt,EyeSpeed)+GazeSaccadeNow.X+Drift.X;GazeEye.Y=Smooth(GazeEye.Y-Micro.Y-GazeEyeDrift.Y,GazeEyeStage.Y,Dt,EyeSpeed)+GazeSaccadeNow.Y+Drift.Y;
 GazeEyeDrift=Drift;
 const auto Rule=Policy(Name,Authored,Walking);
 // First steps of a walk: a small head turn (at most ~25 deg) toward a player in front of or beside her.
 const bool WalkLook=GazeLayerEnabled&&Rule.Category==6&&Now-GazeWalkStarted<3.&&FMath::Abs(RawYaw)<90.f;
 const float RuleHead=Poi?GazePoiHead:WalkLook?.5f:Rule.Head;
 // Opening greeting (2026-10-07): until it has played, the layer stays out. Turning her head and chest to the player
 // beforehand made her already face the player, so the authored "turn toward you while raising the hand" lost its turn
 // and the arm read as a stiff sideways reach. The 10-03 greeting the user liked had no gaze layer at all. The layer
 // returns at 4.0 s, while the authored head still faces the player, so it keeps her eyes on the player as her body
 // turns back (no look-away-then-back after the greeting).
 const bool OpeningPlaying=ActiveAmbient&&(AmbientBehavior==40||AmbientBehavior==41)&&Main->Montage_IsActive(ActiveAmbient);
 const bool OpeningHold=!WelcomeDone||(OpeningPlaying&&Main->Montage_GetPosition(ActiveAmbient)<4.f);
 const bool Engaged=(Nearby||Distant)&&!OpeningHold;
 const float HW=GazeLayerEnabled?(Engaged&&(!Rear||RearGlance||RearHold)?RuleHead:0):(Nearby&&!OpeningHold&&!Walking&&!Authored&&(!Rear||RearGlance)?1:0);
 const float EW=GazeLayerEnabled?(Engaged&&(!Rear||RearGlance||RearHold)?Rule.Eye:0):(Nearby&&!OpeningHold&&!Authored&&!Rear?1:0);
 auto Fade=[&](float Goal,float& Weight,float& From,float& To,double& At){if(Goal!=To){From=Weight;To=Goal;At=Now;}
  const float Duration=To<From?(GazeLayerEnabled?(Rear?1.f:.7f):.3f):.65f;
  Weight=FMath::Lerp(From,To,Ease(float((Now-At)/Duration)));};
 Fade(HW,GazeHeadWeight,GazeHeadFrom,GazeHeadTo,GazeHeadAt);Fade(EW,GazeEyeWeight,GazeEyeFrom,GazeEyeTo,GazeEyeAt);
 G->Frame={float(GazeHead.X),float(GazeHead.Y),float(GazeEye.X),float(GazeEye.Y),GazeHeadWeight,GazeEyeWeight,Now,Review,Authored,Walking};
 // Limit the final weighted correction, not just targets: target motion and fades share one budget.
 FVector2D Goal=GazeHead*GazeHeadWeight;
 // Intentional acknowledgement bypasses the pursuit delay, but shares the final speed budget.
 if(GazeLayerEnabled && Now-GazeNoticeAt<.8){const float T=(Now-GazeNoticeAt)/.8;Goal.Y-=3.f*Ease(T<.5f?T*2:(1-T)*2)*GazeHeadWeight;}
 // While the opening greeting plays the correction follows the weight exactly, so it cancels the turn compensation
 // (whose authored head already faces the player) instead of lagging behind it and dipping the head.
 if(OpeningPlaying)GazeApplied=Goal;
 FVector2D Step=(Goal-GazeApplied)*(1-FMath::Exp(-7.f*FMath::Min(Dt,.1f)));
 const double Limit=75.f*FMath::Min(Dt,.1f);if(Step.Size()>Limit)Step*=Limit/Step.Size();GazeApplied+=Step;
 if(Goal.IsNearlyZero() && GazeApplied.Size()<.0001)GazeApplied=FVector2D::ZeroVector;
 auto& Frame=G->Frame;Frame.Additive=GazeLayerEnabled;
 Frame.TorsoYaw=FMath::Sign(GazeApplied.X)*FMath::Clamp((FMath::Abs(GazeApplied.X)-30.)*.8,0.,20.);
 Frame.TorsoPitch=FMath::Clamp(GazeApplied.Y*.18,-3.,3.);
 Frame.AppliedYaw=GazeApplied.X-Frame.TorsoYaw;Frame.AppliedPitch=GazeApplied.Y-Frame.TorsoPitch;
 Frame.TurnCompensation=Authored&&(Name.Contains(TEXT("WelcomeTurnWave"))||Name.Contains(TEXT("Reunion"))||Name.Contains(TEXT("WelcomeLonely"))||Name.Contains(TEXT("WelcomeShy")))?1.f:0.f;
 Frame.TurnKeepYaw=0;
 if(Frame.TurnCompensation>0&&Montage&&Name.Contains(TEXT("WelcomeShy"))){
  // The shy welcome glances ~10 deg away (to her right) at 1.9-3.4 s (Tools/ue_presence_author_s4.py extras).
  auto S=[](float X){X=FMath::Clamp(X,0.f,1.f);return X*X*X*(10+X*(-15+6*X));};const float T=Main->Montage_GetPosition(Montage);
  Frame.TurnKeepYaw=10.f*S((T-1.9f)/.4f)*(1-S((T-2.9f)/.5f));
 }
 Frame.Distance=Distance;Frame.RawYaw=RawYaw;Frame.RawPitch=RawPitch;Frame.Visible=Visible;Frame.Category=Rule.Category;
 TickIdleVariation(Dt,G,Authored||Walking||Page!=ECourtyardPage::Game);
 if(ContentLayer.IsValid())ContentLayer->FillTerracePalmFrame(Frame);
 if(ContentLayer.IsValid())ContentLayer->FillFlowerScentFrame(Frame);
 if(Review&&G->Probe){FScopeLock Lock(&G->Probe->Mutex);auto& B=*G->Probe;if(B.Time>GazeLastProbe){GazeLastProbe=B.Time;
  FString Q;for(int32 I=0;I<4;++I){const auto& R=B.Output[I];Q+=FString::Printf(TEXT(",%.7f,%.7f,%.7f,%.7f"),R.X,R.Y,R.Z,R.W);}
  float Delta=0;for(int32 I=0;I<4;++I)Delta=FMath::Max(Delta,float(FMath::RadiansToDegrees(B.Input[I].AngularDistance(B.Output[I]))));
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE_SAMPLE %.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.4f,%d,%d%s,%.3f,%.3f,%.3f,%d,%d"),B.Time,B.HeadYaw,B.HeadPitch,B.EyeDegrees,B.HeadWeight,B.EyeWeight,B.FootDelta,Delta,B.Authored,B.Walking,*Q,B.Distance,B.RawYaw,B.RawPitch,B.Visible,B.Category);
  FString IQ;float IDelta=0;for(int J=0;J<5;++J){auto R=B.IdleOutput[J];IQ+=FString::Printf(TEXT(",%.7f,%.7f,%.7f,%.7f"),R.X,R.Y,R.Z,R.W);IDelta=FMath::Max(IDelta,float(FMath::RadiansToDegrees(B.IdleInput[J].AngularDistance(R))));}
  FString PA;for(int J=0;J<PostureCount;++J)PA+=FString::Printf(TEXT(",%.3f"),B.IdleAngles[J]);
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_IDLE_SAMPLE %.4f,%d,%.4f,%.6f,%.5f,%d%s%s"),B.Time,B.IdleVariant,B.IdleWeight,B.FootDelta,IDelta,B.Authored||B.Walking,*PA,*IQ);
 }}
}
