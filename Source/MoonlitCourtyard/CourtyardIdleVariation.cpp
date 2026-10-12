#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "CourtyardGazeAnimInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

// S3 (presence task): standing postures she changes between every 20-60 s, plus a mood posture on top, so the idle
// loop does not repeat and her mood reads from 2 m by body language. Upper body only (chest, shoulders, upper arms,
// head); legs, feet and root never move. Static targets with eased blends, no periodic sway.
// Channels (degrees, positive = named direction): ChestForward, ShoulderDropL/R, ShoulderForwardL/R, ArmOutL/R,
// ArmForwardL/R, HeadDown, HeadTiltRight.
namespace {
const float IdlePostureVariants[4][PostureCount]={
 {0,0,0,0,0,0,0,0,0,0,0},          // neutral
 {3,3,3,2,2,0,0,3,3,0,3},          // relaxed: soft slouch, shoulders down, hands a little forward, head tilted
 {-3,-1,-1,-3,-3,5,5,0,0,-1,-2},   // open: chest up, shoulders back, arms slightly away from the body
 {0,5,-2,0,0,2,0,0,0,0,3}};        // weight on one side: left shoulder low, head tilted the other way
const TCHAR* IdleVariantNames[4]={TEXT("neutral"),TEXT("relaxed"),TEXT("open"),TEXT("tilt")};
void IdleMoodPosture(const FString& Mood,int32 Variant,float* Out){
 for(int I=0;I<PostureCount;++I)Out[I]=0;
 if(Mood==TEXT("lonely")){Out[ChestForward]=3;Out[ShoulderDropL]=Out[ShoulderDropR]=3;Out[ShoulderForwardL]=Out[ShoulderForwardR]=3;Out[HeadDown]=6;}
 else if(Mood==TEXT("sleepy")){Out[ChestForward]=2;Out[ShoulderDropL]=Out[ShoulderDropR]=5;Out[ArmForwardL]=Out[ArmForwardR]=4;Out[HeadDown]=7;}
 else if(Mood==TEXT("happy")){Out[ChestForward]=-2.5f;Out[ShoulderForwardL]=Out[ShoulderForwardR]=-1;Out[HeadTiltRight]=Variant%2?-4.5f:4.5f;}
}
float IdleEase(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*X*(X*(X*6-15)+10);}
}
void ACourtyardController::CourtyardIdleVariation(bool Enabled){IdleVariationEnabled=Enabled;UE_LOG(LogTemp,Display,TEXT("COURTYARD_IDLE enabled=%d"),Enabled);}
// Review only: switch to one posture now and hold it (no further random changes). -1 releases the hold.
void ACourtyardController::CourtyardReviewIdleVariant(int32 Variant)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Variant<0){IdleReviewHold=false;IdleNextChange=Now+FMath::FRandRange(20.f,60.f);UE_LOG(LogTemp,Display,TEXT("COURTYARD_IDLE review_hold=0"));return;}
 const int32 N=FMath::Clamp(Variant,0,3);
 for(int I=0;I<PostureCount;++I){IdleFrom[I]=IdleCurrent[I];IdleTarget[I]=IdlePostureVariants[N][I];}
 IdleVariant=N;IdleBlendAt=Now;IdleReviewHold=true;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_IDLE variant=%d name=%s mood=%s review_hold=1 t=%.3f"),N,IdleVariantNames[N],*Mood,Now);
}
void ACourtyardController::TickIdleVariation(float Dt,UCourtyardGazeAnimInstance* G,bool Busy)
{
 const double Now=GetWorld()->GetTimeSeconds();
 if(IdleNextChange==0)IdleNextChange=Now+FMath::FRandRange(20.f,60.f);
 if(IdleVariationEnabled&&!Busy&&!IdleReviewHold&&Now>=IdleNextChange){
  // Sleepy prefers the relaxed posture, lonely the lowered one-sided stance; otherwise any other posture.
  int32 N;do{const int32 Roll=FMath::RandRange(0,99);
   N=Mood==TEXT("sleepy")?(Roll<55?1:Roll<75?0:Roll<90?3:2):Mood==TEXT("lonely")?(Roll<35?3:Roll<65?1:Roll<85?0:2):Roll/25;}while(N==IdleVariant);
  for(int I=0;I<PostureCount;++I){IdleFrom[I]=IdleCurrent[I];IdleTarget[I]=IdlePostureVariants[N][I];}
  IdleVariant=N;IdleBlendAt=Now;IdleNextChange=Now+FMath::FRandRange(20.f,60.f);
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_IDLE variant=%d name=%s mood=%s next=%.2f t=%.3f"),N,IdleVariantNames[N],*Mood,IdleNextChange-Now,Now);
 }
 for(int I=0;I<PostureCount;++I)IdleCurrent[I]=FMath::Lerp(IdleFrom[I],IdleTarget[I],IdleEase(float((Now-IdleBlendAt)/2.5)));
 // Mood posture blends over 2.5 s whenever her mood changes (and follows the happy head-tilt side of the posture).
 float Wanted[PostureCount];IdleMoodPosture(Mood,IdleVariant,Wanted);
 bool Changed=MoodPostureFor!=Mood;for(int I=0;I<PostureCount && !Changed;++I)Changed=!FMath::IsNearlyEqual(Wanted[I],MoodPostureTarget[I]);
 if(Changed){for(int I=0;I<PostureCount;++I){MoodPostureFrom[I]=MoodPostureCurrent[I];MoodPostureTarget[I]=Wanted[I];}MoodPostureAt=Now;MoodPostureFor=Mood;
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_IDLE mood_posture=%s t=%.3f"),*Mood,Now);}
 for(int I=0;I<PostureCount;++I)MoodPostureCurrent[I]=FMath::Lerp(MoodPostureFrom[I],MoodPostureTarget[I],IdleEase(float((Now-MoodPostureAt)/2.5)));
 // Yield to montages and walking: out over 0.5 s, back over 2.5 s. A faster return (0.8 s) moved the
 // shoulders/upper arms up to ~7 deg right after a gesture had settled, reading as a second arm motion.
 const float Goal=IdleVariationEnabled&&!Busy?1.f:0.f;
 if(Goal!=IdleFadeTo){IdleFadeFrom=IdleWeight;IdleFadeTo=Goal;IdleFadeAt=Now;}
 IdleWeight=FMath::Lerp(IdleFadeFrom,IdleFadeTo,IdleEase(float((Now-IdleFadeAt)/(IdleFadeTo<IdleFadeFrom?.5:2.5))));
 for(int I=0;I<PostureCount;++I)G->Frame.IdleAngles[I]=IdleCurrent[I]+MoodPostureCurrent[I];
 if(ContentLayer.IsValid() && ContentLayer->FixEnabled)ContentLayer->ComposeFixedPosture(Dt,G->Frame.IdleAngles);
 G->Frame.IdleWeight=IdleWeight;G->Frame.IdleVariant=IdleVariant;
}
