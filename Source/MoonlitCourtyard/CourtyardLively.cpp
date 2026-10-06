#include "CourtyardController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"

namespace {
double Num(UObject* O,FName N,double F=0){if(O)if(auto* P=FindFProperty<FNumericProperty>(O->GetClass(),N))if(P->IsFloatingPoint())return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(O));return F;}
FLivelyFacePhase Phase(float A,float B,std::initializer_list<TPair<int32,float>> V,float Fade=.35f){FLivelyFacePhase P;P.Start=A;P.End=B;P.Fade=Fade;for(const auto& I:V)P.Values.Add(I);return P;}
}
void ACourtyardController::CourtyardPresenceGestures(bool Enabled)
{
    PresenceGesturesEnabled=Enabled;UE_LOG(LogTemp,Display,TEXT("COURTYARD_PRESENCE gestures=%d"),Enabled);
}
void ACourtyardController::CourtyardLivelyTune(bool Enabled)
{
    if(ActiveAmbient || LivelyResponseActive || PendingLivelyPat || !LivelyBodyId.IsNone()){UE_LOG(LogTemp,Display,TEXT("COURTYARD_TUNE rejected=busy"));return;}
    LivelyTuneEnabled=Enabled;FaceBaselineFrom=FaceBaseline;FaceBaselineChanged=GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_TUNE enabled=%d time=%.3f"),Enabled,GetWorld()->GetTimeSeconds());
}
void ACourtyardController::InitializeLively()
{
    if(!LivelyEnabled || LivelyRules.Num())return;
    const TCHAR* Names[]={TEXT("心情底色"),TEXT("眨眼节奏"),TEXT("发呆"),TEXT("偷笑"),TEXT("哼歌"),TEXT("自言自语"),TEXT("憋哈欠"),
      TEXT("欢迎回来"),TEXT("被盯着看"),TEXT("摸头回应"),TEXT("被吓到"),TEXT("看你乱跳"),TEXT("你躺在附近"),TEXT("招呼后跑开"),TEXT("小脾气"),TEXT("捉迷藏"),TEXT("击掌邀请"),TEXT("被冷落"),TEXT("一起看花"),TEXT("道别"),
      TEXT("流星许愿"),TEXT("长尾流星"),TEXT("看极光"),TEXT("数星星"),TEXT("捏月亮"),TEXT("看云"),TEXT("闻花"),TEXT("帽上花瓣"),TEXT("接花瓣结局"),TEXT("古樱仰望"),TEXT("萤火虫"),TEXT("飞蛾"),TEXT("风中理发"),TEXT("听风铃"),TEXT("风铃太响"),TEXT("音乐"),TEXT("突然声响"),TEXT("喷泉倒影"),TEXT("水花"),TEXT("壁炉暖手"),TEXT("窗边望月"),TEXT("找书"),TEXT("琴键"),
      TEXT("打瞌睡"),TEXT("喷嚏"),TEXT("伸展表情"),TEXT("哈欠流泪"),TEXT("搓手哈气"),TEXT("重逢"),TEXT("得意")};
    const TSet<FName> P1={TEXT("A1"),TEXT("A2"),TEXT("A3"),TEXT("A4"),TEXT("A5"),TEXT("A6"),TEXT("A7"),TEXT("B1"),TEXT("B3"),TEXT("B5"),TEXT("B7"),TEXT("B11"),TEXT("B12"),TEXT("C9"),TEXT("C15"),TEXT("D3"),TEXT("D4"),TEXT("D6"),TEXT("D7")};
    const TSet<FName> P2={TEXT("B2"),TEXT("B4"),TEXT("B6"),TEXT("B8"),TEXT("B9"),TEXT("B10"),TEXT("B13"),TEXT("C3"),TEXT("C4"),TEXT("C6"),TEXT("C10"),TEXT("C16"),TEXT("C17"),TEXT("D1")};
    // C2/C5: third candidates retain a wrist discontinuity around 2.05/2.15s.
    // C1/C11 explicitly accepted for default scheduling in EMMA_PRESENCE_TASK (existing P3Prayer2/P3Firefly2).
    // C7: no positive flower location. C8: final hat placement still not readable.
    // Keep assets and review overrides, but exclude these from normal scheduling.
    const TSet<FName> P3={TEXT("C1"),TEXT("C11"),TEXT("C12"),TEXT("C13"),TEXT("C14"),TEXT("D2")};
    int32 Index=0;
    for(int32 Group=0;Group<4;++Group){const int32 Count=Group==0?7:Group==1?13:Group==2?23:7;
        for(int32 N=1;N<=Count;++N){FLivelyRule R;R.Id=FName(*FString::Printf(TEXT("%c%d"),TCHAR('A'+Group),N));R.Name=Names[Index++];R.Enabled=P1.Contains(R.Id) || (LivelyStage>=2 && P2.Contains(R.Id)) || (LivelyStage>=3 && P3.Contains(R.Id));LivelyRules.Add(R);}
    }
    for(auto& R:LivelyRules){
        if(R.Id==TEXT("C1") || R.Id==TEXT("C5") || R.Id==TEXT("C8"))R.CooldownMin=R.CooldownMax=600;
        if(R.Id==TEXT("C2"))R.CooldownMin=R.CooldownMax=60;
        if(R.Id==TEXT("C7")){R.CooldownMin=240;R.CooldownMax=360;}
        if(R.Id==TEXT("C11")){R.CooldownMin=360;R.CooldownMax=600;}
        if(R.Id==TEXT("C12"))R.CooldownMin=R.CooldownMax=300;
        if(R.Id==TEXT("D2")){R.CooldownMin=720;R.CooldownMax=1200;R.Due=FMath::FRandRange(720.f,1200.f);}
        if(R.Id==TEXT("A4")){R.CooldownMin=120;R.CooldownMax=200;}
        if(R.Id==TEXT("A5")){R.CooldownMin=150;R.CooldownMax=240;}
        if(R.Id==TEXT("A7")){R.CooldownMin=80;R.CooldownMax=140;}
        if(R.Id==TEXT("B5") || R.Id==TEXT("D7"))R.CooldownMin=R.CooldownMax=120;
        if(R.Id==TEXT("B7"))R.CooldownMin=R.CooldownMax=600;
        if(R.Id==TEXT("B11"))R.CooldownMin=R.CooldownMax=180;
        if(R.Id==TEXT("B4") || R.Id==TEXT("C17"))R.CooldownMin=R.CooldownMax=60;
        if(R.Id==TEXT("B9"))R.CooldownMin=R.CooldownMax=20;
        if(R.Id==TEXT("B10")){R.CooldownMin=240;R.CooldownMax=360;}
        if(R.Id==TEXT("B13"))R.CooldownMin=R.CooldownMax=600;
        if(R.Id==TEXT("C3"))R.CooldownMin=R.CooldownMax=300; // S5: was 100 s; it became the most frequent behaviour
        if(R.Id==TEXT("C4")){R.CooldownMin=480;R.CooldownMax=720;R.Due=FMath::FRandRange(480.f,720.f);}
        if(R.Id==TEXT("C6")){R.CooldownMin=240;R.CooldownMax=360;R.Due=FMath::FRandRange(240.f,360.f);}
        if(R.Id==TEXT("C16"))R.CooldownMin=R.CooldownMax=180;
        if(R.Id==TEXT("D1")){R.CooldownMin=360;R.CooldownMax=600;R.Due=FMath::FRandRange(360.f,600.f);}
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY catalog index=%d id=%s name=%s enabled=%d"),int32(&R-LivelyRules.GetData())+1,*R.Id.ToString(),*R.Name,R.Enabled);
    }
}
bool ACourtyardController::LivelyOn(FName Id) const
{
    if(!LivelyEnabled)return false;for(const auto& R:LivelyRules)if(R.Id==Id)return R.Enabled;return false;
}
bool ACourtyardController::LivelyReady(FName Id,bool Forced) const
{
    if(!LivelyOn(Id))return false;
    // She can be indoors now (locomotion rework): looking up at the sky, meteors, fireflies, moths and wind belong outside.
    if(LocoOn && const_cast<ACourtyardController*>(this)->LocoIndoors())for(const TCHAR* Sky:{TEXT("C1"),TEXT("C3"),TEXT("C4"),TEXT("C6"),TEXT("C10"),TEXT("C11"),TEXT("C12"),TEXT("C13"),TEXT("B6")})if(Id==Sky)return false;
    if(!Forced && LivelyHistory.Num() && LivelyHistory.Last()==Id)return false;
    for(const auto& R:LivelyRules)if(R.Id==Id)return Forced || GetWorld()->GetTimeSeconds()>=R.Due;
    return false;
}
bool ACourtyardController::LivelySafe(bool Body) const
{
    auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;const double Now=GetWorld()->GetTimeSeconds();
    return I && Page==ECourtyardPage::Game && !PoseTransition && !IsSeatBusy() && !LivingTrip && LivingBehavior<0 && !ActiveAmbient
        && !I->IsAnyMontagePlaying() && !BPBool(A,TEXT("TravelActive")) && !BPBool(A,TEXT("Interrupted")) && BPBool(A,TEXT("AllowInteraction"))
        && QueryBool(A,TEXT("CanHeadPat")) && A->GetVelocity().Size()<2 && Now-AmbientStillSince>=5
        && Num(A,TEXT("DueTime"),Now+100)-Now>=9 && Now-Num(A,TEXT("LastGlanceTime"),-100)>=10 && !PendingLivelyPat;
}
void ACourtyardController::LivelyStarted(FName Id,const TCHAR* Reason,bool Body)
{
    if(!LivelyEnabled)return;
    const double Now=GetWorld()->GetTimeSeconds();for(auto& R:LivelyRules)if(R.Id==Id)R.Due=Now+FMath::FRandRange(R.CooldownMin,R.CooldownMax);
    LivelyHistory.Add(Id);if(LivelyHistory.Num()>8)LivelyHistory.RemoveAt(0);
    if(Body){LivelyBodyId=Id;CancelFace();LivelyWitnessed=false;LivelyWitnessMotionStart=Now;LivelyWitnessReply=NAME_None;}
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY start id=%s mood=%s seen=%d reason=%s slot=%s time=%.3f"),*Id.ToString(),*Mood,PlayerWatching,Reason,Body?TEXT("body"):TEXT("face"),Now);
}
void ACourtyardController::LivelyFinished(FName Id,const TCHAR* Reason)
{
    if(Id.IsNone() || !LivelyEnabled)return;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY end id=%s reason=%s time=%.3f"),*Id.ToString(),Reason,GetWorld()->GetTimeSeconds());
}
UAnimMontage* ACourtyardController::LivelyMotion(FName Name,UAnimMontage* Fallback) const
{
    if(LivelyEnabled && LivelyTuneEnabled && PresenceGesturesEnabled)if(const auto* New=PresenceMotions.Find(Name))if(*New)return New->Get();
    if(LivelyEnabled && LivelyTuneEnabled)if(const auto* New=TunedMotions.Find(Name))if(*New)return New->Get();
    const auto* P=LivelyMotions.Find(Name);return LivelyEnabled && P && *P?P->Get():Fallback;
}
UAnimMontage* ACourtyardController::LivelyBodyVariant(int32 V,UAnimMontage* Fallback,bool Forced)
{
    if(!LivelyEnabled)return Fallback;
    if(V==0 && LivelyReady(TEXT("B1"),Forced)){return LivelyMotion(Mood==TEXT("lonely")?TEXT("WaveLonely"):FMath::FRand()<.35f?TEXT("WaveShy"):TEXT("WaveHappy"),Fallback);}
    if(V==1 && LivelyReady(TEXT("D3"),Forced) && Mood!=TEXT("happy"))return LivelyMotion(TEXT("StretchMood"),Fallback);
    if(V==2 && LivelyReady(TEXT("D4"),Forced) && Mood==TEXT("sleepy"))return LivelyMotion(TEXT("YawnMood"),Fallback);
    return Fallback;
}
void ACourtyardController::LivelyBodyBegan(int32 V)
{
    if(!LivelyEnabled)return;
    FName Id=V==0?FName(TEXT("B1")):V==1?FName(TEXT("D3")):V==2?FName(TEXT("D4")):NAME_None;
    const bool Used=V==0?(ActiveAmbient==LivelyMotion(TEXT("WaveHappy"),nullptr) || ActiveAmbient==LivelyMotion(TEXT("WaveLonely"),nullptr) || ActiveAmbient==LivelyMotion(TEXT("WaveShy"),nullptr)):
        V==1?ActiveAmbient==LivelyMotion(TEXT("StretchMood"),nullptr):V==2?ActiveAmbient==LivelyMotion(TEXT("YawnMood"),nullptr):false;
    if(!Id.IsNone() && LivelyOn(Id) && Used)LivelyStarted(Id,TEXT("existing_motion"),true);
    if(V==0)MoodEvent(TEXT("eye"));
}
void ACourtyardController::LivelyLivingBegan(int32 V)
{
    if(V==0 && LivelyOn(TEXT("C9")) && (LivingPetalFailed || ActiveAmbient==LivelyMotion(TEXT("PetalSuccess"),nullptr)))LivelyStarted(TEXT("C9"),LivingPetalFailed?TEXT("petal_miss"):TEXT("petal_success"),true);
    if(V==1 && ActiveAmbient==LivelyMotion(TEXT("ChimeStartled"),nullptr))LivelyStarted(TEXT("C15"),TEXT("chime"),true);
}
void ACourtyardController::LivelyBodyEnded()
{
    if(!LivelyEnabled)return;
    const FName Id=LivelyBodyId;LivelyBodyId=NAME_None;
    if(Id==TEXT("B1") || Id==TEXT("D6") || AmbientBehavior==40 || AmbientBehavior==41)GreetingEndedAt=GetWorld()->GetTimeSeconds();
    if((Id==TEXT("C9") && !LivingPetalFailed) || Id==TEXT("C5"))LastSuccessAt=GetWorld()->GetTimeSeconds();
    if(Id==TEXT("D4") || Id==TEXT("D1"))MoodEvent(TEXT("rest"));
    if((Id==TEXT("D1") || Id==TEXT("D2")) && LivelyWitnessed && !PendingLivelyPat && !LivelyResponseActive){LivelyWitnessReply=Id;LivelyWitnessReplyUntil=GetWorld()->GetTimeSeconds()+2;}
    LivelyFinished(Id,TEXT("montage_finished"));
}
bool ACourtyardController::SelectLivelyResponse()
{
    if(!LivelyOn(TEXT("B3")))return false;
    int32 PendingCount=1;const double Now=GetWorld()->GetTimeSeconds();for(double T:MoodPatTimes)if(Now-T<60)++PendingCount;
    const FName Name=P3HatAttached?FName(TEXT("Hat")):PendingPatSource==TEXT("B8")?FName(TEXT("Pout")):PendingCount>=4?FName(TEXT("Protest")):Mood==TEXT("lonely")?FName(TEXT("Lonely")):Mood==TEXT("happy")?FName(TEXT("Happy")):FMath::FRand()<.4f?FName(TEXT("Shy")):NAME_None;
    if(Name.IsNone())return false;
    const auto* P=LivelyResponses.Find(Name);if(LivelyTuneEnabled)if(const auto* New=TunedResponses.Find(Name))if(*New)P=New;auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Emma/Animations/AM_Emma_HeadPatSmile"));
    if(!P || !*P || !Montage || Montage->SlotAnimTracks.Num()!=1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()!=1 || !FMath::IsNearlyEqual(Montage->GetPlayLength(),(*P)->GetPlayLength(),.01f))return false;
    Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].SetAnimReference(P->Get(),false);LastResponse=10+(Name==TEXT("Happy")?0:Name==TEXT("Shy")?1:Name==TEXT("Lonely")?2:Name==TEXT("Pout")?4:Name==TEXT("Hat")?5:3);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_RESPONSE lively=%s asset=%s"),*Name.ToString(),*(*P)->GetName());return true;
}
void ACourtyardController::ExecutePatResponse()
{
    LivelyWitnessReply=NAME_None;LivelyWitnessed=false;CancelFace();SelectResponse();PendingPatSource=NAME_None;ExecuteLegacy(EKeys::E);
    auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
    auto* Pat=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Emma/Animations/AM_Emma_HeadPatSmile"));
    const bool Accepted=I && Pat && I->Montage_IsPlaying(Pat);
    if(Accepted){DropHatPetal(true);P3HatDue=-1;P3GustHairPending=false;P3WishPending=false;P3WishUntil=-1;P3FlyVisitUntil=-1;P2PoutUntil=-1;LastBodyPat=GetWorld()->GetTimeSeconds();MoodEvent(TEXT("pat"));if(LivelyOn(TEXT("B3"))){LivelyResponseActive=true;LivelyStarted(TEXT("B3"),TEXT("E_montage_started"),false);}}
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_RESPONSE actual_started=%d time=%.3f"),Accepted,GetWorld()->GetTimeSeconds());
}
bool ACourtyardController::LivelyCanYieldToPat() const
{
    return LivelyEnabled && !LivelyBodyId.IsNone() && ActiveAmbient && AmbientBehavior!=40 && AmbientBehavior!=41 && LivelyBodyId!=TEXT("D6")
        && (!LivingTrip || AtAncientArrival()) && LivingBehavior<0 && !BPBool(Emma(),TEXT("TravelActive")) && !BPBool(Emma(),TEXT("Interrupted"));
}
bool ACourtyardController::TryLivelyPat()
{
    if(!LivelyEnabled)return false;
    if(PendingLivelyPat)return true;
    if(!LivelyCanYieldToPat() || !CanRespond())return false;
    auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;if(!I)return false;
    PendingPatSource=LivelyBodyId;PendingLivelyPat=true;PendingLivelyPatUntil=GetWorld()->GetTimeSeconds()+1.2;CancelFace();I->Montage_Stop(.35f,ActiveAmbient);return true;
}
bool ACourtyardController::StartLively(FName Id,bool Forced)
{
    if(!LivelyReady(Id,Forced) || !LivelySafe(false))return false;
    if(LivelyTuneEnabled){
        if(Id==TEXT("A4"))return PlayLivelyHead(Id,TEXT("Chuckle"));
        if(Id==TEXT("A6"))return PlayLivelyHead(Id,TEXT("Mumble"));
        if(Id==TEXT("A7"))return PlayLivelyHead(Id,TEXT("SuppressYawn"));
        if(Id==TEXT("D7"))return PlayLivelyHead(Id,TEXT("Proud"));
        if(Id==TEXT("B7")){FVector Eyes,Front,V;FRotator R;if(!FaceFrame(Eyes,Front))return false;GetPlayerViewPoint(V,R);
            const FVector D=(V-Eyes).GetSafeNormal2D();const float Y=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Front,D).Z,FVector::DotProduct(Front,D)));
            if(!Forced && (FMath::Abs(Y)>40 || !VisibleTarget(V,PlayerCharacter)))return false;
            return PlayLivelyHead(Id,Y>=0?TEXT("DepartLeft"):TEXT("DepartRight"));}
    }
    if(LivelyStage>=3){const TSet<FName> P3={TEXT("C1"),TEXT("C2"),TEXT("C5"),TEXT("C7"),TEXT("C8"),TEXT("C11"),TEXT("C12"),TEXT("C13"),TEXT("C14"),TEXT("D2")};if(P3.Contains(Id))return StartP3(Id,Forced);}
    if(LivelyStage>=2){
        const TSet<FName> HeadIds={TEXT("A3"),TEXT("A5"),TEXT("B2"),TEXT("B4"),TEXT("B6"),TEXT("B8"),TEXT("B9"),TEXT("B10"),TEXT("B11"),TEXT("B13"),TEXT("C3"),TEXT("C4"),TEXT("C6"),TEXT("C16"),TEXT("C17"),TEXT("D1")};
        if(HeadIds.Contains(Id))return StartP2(Id,Forced);
        if(Id==TEXT("C10") && Forced)return PlayLivelyHead(Id,TEXT("AncientGaze"));
    }
    if(Id==TEXT("A1")){FaceBaselineMood.Empty();return true;}
    if(Id==TEXT("A2")){NextExtraBlink=GetWorld()->GetTimeSeconds();return true;}
    if(Id==TEXT("A3"))return StartFace(Id,{Phase(0,5.5,{{39,.8f},{7,.6f}})},5.5f);
    if(Id==TEXT("A4"))return StartFace(Id,{Phase(0,2,{{FMath::RandBool()?19:20,.5f}}),Phase(1.7,3.3,{{17,.6f}}),Phase(3,5.5,{{24,.7f},{54,.4f}})},5.5f,TEXT("fufu"));
    if(Id==TEXT("A5"))return StartFace(Id,{Phase(0,8,{{63,.6f}})},8,TEXT("hum"));
    if(Id==TEXT("A6"))return StartFace(Id,{Phase(0,3.5,{{16,.18f}})},3.5f,TEXT("mumble"));
    if(Id==TEXT("A7"))return StartFace(Id,{Phase(0,1.2,{{2,.5f}}),Phase(.9,1.6,{{11,.5f}},.15f),Phase(1.5,4,{{25,.6f},{69,.6f}})},4);
    if(Id==TEXT("B7"))return StartFace(Id,{Phase(0,.8,{{41,.4f}},.15f),Phase(.7,3.8,{{22,.4f}})},3.8f,TEXT(""),3);
    if(Id==TEXT("B11"))return StartFace(Id,{Phase(0,2.5,{{32,.4f},{52,.4f}}),Phase(2.1,4.5,{{53,.5f}})},4.5f,TEXT(""),2);
    if(Id==TEXT("D7"))return StartFace(Id,{Phase(0,3,{{17,.7f},{15,.8f},{1,.3f}})},3);
    if(Id==TEXT("B5")){
        if(PlayCompanion(LivelyMotion(TEXT("ClapJump"),nullptr),205,TEXT("player_jumps"))){LivelyStarted(Id,Forced?TEXT("review"):TEXT("three_jumps"),true);PlayerJumps.Empty();return true;}
        return false;
    }
    if(Id==TEXT("D3") || Id==TEXT("D4") || Id==TEXT("B1"))return StartBody(Id==TEXT("D3")?1:Id==TEXT("D4")?2:0,Forced);
    if(Id==TEXT("B12")){int32 V=FlowerContext();return V>=0 && TryAmbientBehavior(V);}
    if(Id==TEXT("C9"))return StartLiving(0,Forced);
    if(Id==TEXT("C15")){LivelyForceChime=Forced;const bool Ok=StartLiving(1,Forced);LivelyForceChime=false;return Ok;}
    return false;
}
void ACourtyardController::TickLively(float Dt)
{
    if(!LivelyEnabled)return;InitializeLively();const double Now=GetWorld()->GetTimeSeconds();
    if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"))){
        auto* EmmaCharacter=Cast<ACharacter>(Emma());auto* Inst=EmmaCharacter&&EmmaCharacter->GetMesh()?EmmaCharacter->GetMesh()->GetAnimInstance():nullptr;
        int32 Count=0;FString Names;
        if(Inst)for(auto* Item:Inst->MontageInstances)if(Item && Item->IsActive() && Item->IsPlaying()){
            ++Count;Names+=Item->Montage->GetName()+TEXT(";");
        }
        if(Names!=ReviewSlotNames){ReviewSlotNames=Names;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY slots active=%d names=%s time=%.3f"),Count,*Names,Now);}
        if(Count>1 && !ReviewOverlapping)++ReviewOverlapEpisodes;ReviewOverlapping=Count>1;
        const bool Near=EmmaCharacter && PlayerCharacter && FVector::Dist(EmmaCharacter->GetActorLocation(),PlayerCharacter->GetActorLocation())<=600;
        if(Near && Count==0 && LivelyFaceId.IsNone() && !BPBool(EmmaCharacter,TEXT("TravelActive"))){if(ReviewNearIdleSince<0)ReviewNearIdleSince=Now;ReviewMaxNearIdle=FMath::Max(ReviewMaxNearIdle,Now-ReviewNearIdleSince);}else ReviewNearIdleSince=-1;
    }
    // The animation ends before the existing Blueprint's interaction cooldown. Report
    // restoration only once that authority also permits another response.
    if(LivelyResponseActive){auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;if(Now-LastBodyPat>.5 && (!I || !I->IsAnyMontagePlaying()) && BPBool(A,TEXT("AllowInteraction")) && QueryBool(A,TEXT("CanHeadPat"))){LivelyResponseActive=false;LivelyFinished(TEXT("B3"),TEXT("response_restored"));}}
    if(PendingLivelyPat){
        if(!ActiveAmbient && CanRespond()){PendingLivelyPat=false;ExecutePatResponse();}
        else if(Now>PendingLivelyPatUntil){PendingLivelyPat=false;PendingPatSource=NAME_None;}
    }
    if(PlayerCharacter){const bool Falling=PlayerCharacter->GetCharacterMovement()->IsFalling();if(Falling && !PlayerWasFalling && PlayerCharacter->GetVelocity().Z>30)PlayerJumps.Add(Now);PlayerWasFalling=Falling;PlayerJumps.RemoveAll([Now](double T){return Now-T>5;});}
    if(Now<LivelyCheckAt)return;LivelyCheckAt=Now+.2;
    if(LivelyFaceId==TEXT("A3") && PlayerWatching && WatchedSince>FaceStarted && FaceCancel<0){
        StartFace(TEXT("A3"),{Phase(0,.65,{{30,.6f},{72,.8f}},.12f)},.8f,TEXT(""),3);
    }
    SampleLivelyEvents(Now);
    if((LivelyBodyId==TEXT("D1") || LivelyBodyId==TEXT("D2")) && PlayerWatching && Now-LivelyWitnessMotionStart>=3.8 && Now-LivelyWitnessMotionStart<=7.2)LivelyWitnessed=true;
    if(!LivelyWitnessReply.IsNone() && !ActiveAmbient){
        const FName Reply=LivelyWitnessReply;LivelyWitnessReply=NAME_None;
        auto* C=Cast<ACharacter>(Emma());auto* I=C&&C->GetMesh()?C->GetMesh()->GetAnimInstance():nullptr;
        if(Now<LivelyWitnessReplyUntil && LivelyOn(Reply) && I && !I->IsAnyMontagePlaying() && !PendingLivelyPat && !LivelyResponseActive && Page==ECourtyardPage::Game && !PoseTransition && !IsSeatBusy() && !LivingTrip && !BPBool(C,TEXT("TravelActive"))){
            StartFace(Reply,{Phase(0,1.3,{{55,.6f},{33,.4f}}),Phase(1.2,2.7,{{24,.6f}})},2.9f,Reply==TEXT("D2")?TEXT("ehehe"):TEXT(""),2);
            UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY witnessed_reply id=%s time=%.3f"),*Reply.ToString(),Now);
        }
    }
    if(LivelyOn(TEXT("C16")) && Now<P2MusicStoppedUntil && !ActiveAmbient && !PendingLivelyPat && !LivelyResponseActive && LivelyFaceId.IsNone()){
        auto* C=Cast<ACharacter>(Emma());auto* I=C&&C->GetMesh()?C->GetMesh()->GetAnimInstance():nullptr;
        if(I && !I->IsAnyMontagePlaying() && Page==ECourtyardPage::Game && !PoseTransition && !IsSeatBusy() && !LivingTrip && !BPBool(C,TEXT("TravelActive"))){
            P2MusicStoppedUntil=-1;StartFace(TEXT("C16"),{Phase(0,1.3,{{41,.5f},{7,.5f}})},1.5f);
        }
    }
    // A short facial wake-up follows the completed blend, not the five-second
    // rest gate for starting a new body action. A player response wins.
    if(P2WakePending && !ActiveAmbient){
        P2WakePending=false;auto* C=Cast<ACharacter>(Emma());auto* I=C&&C->GetMesh()?C->GetMesh()->GetAnimInstance():nullptr;
        if(I && !I->IsAnyMontagePlaying() && !PendingLivelyPat && !LivelyResponseActive && Page==ECourtyardPage::Game && !LivingTrip && !BPBool(C,TEXT("TravelActive")))
            StartFace(TEXT("A3"),{Phase(0,.65,{{30,.6f},{72,.8f}},.12f)},.8f,TEXT(""),3);
    }
    if(!LivelySafe(false))return;
    if(TryP2Reaction())return;
    auto* A=Emma();const float Distance=PlayerCharacter&&A?FVector::Dist(PlayerCharacter->GetActorLocation(),A->GetActorLocation()):99999;
    const bool Visible=PlayerCharacter && ViewCamera && VisibleTarget(ViewCamera->GetComponentLocation(),PlayerCharacter);
    FVector Eyes,Front;const bool InFront=FaceFrame(Eyes,Front) && FVector::DotProduct(Front.GetSafeNormal2D(),(PlayerCharacter->GetActorLocation()-Eyes).GetSafeNormal2D())>.17f;
    if(PlayerJumps.Num()>=3 && Distance<=400 && Visible && InFront && StartLively(TEXT("B5")))return;
    if(Now-GreetingEndedAt<6 && Distance>600 && StartLively(TEXT("B7")))return;
    if(Distance<=500 && Now-LastPlayerSeen>60 && StartLively(TEXT("B11")))return;
    if(TryP3Reaction())return;
    if(Now-LastSuccessAt<9 && Mood==TEXT("happy") && StartLively(TEXT("D7")))return;
    if(!LivelyFaceId.IsNone() || Now<LivelyFaceDue || !SeenOrWaited(LivelyDefer))return;
    LivelyFaceDue=Now+FMath::FRandRange(18.f,35.f);TArray<FName> Bag;
    if(Mood==TEXT("happy")){Bag={TEXT("A4"),TEXT("A5")};if(PlayerWatching)Bag.Add(TEXT("A4"));}
    else if(Mood==TEXT("sleepy")){if(Now<BodyCooldown[2])Bag.Add(TEXT("A7"));if(Now-AmbientStillSince>=20 && Now-LastBodyPat>=30)Bag.Add(TEXT("A3"));}
    else if(Mood==TEXT("calm")){Bag.Add(TEXT("A5"));if(Now-AmbientStillSince>=20 && Now-LastBodyPat>=30)Bag.Add(TEXT("A3"));if(FlowerContext()>=0 || Now-LastSuccessAt<15)Bag.Add(TEXT("A6"));}
    while(Bag.Num()){const int32 Pick=FMath::RandHelper(Bag.Num());const FName Id=Bag[Pick];Bag.RemoveAt(Pick);if(StartLively(Id)){LivelyDefer=-1;break;}}
}
void ACourtyardController::CourtyardReviewLively(int32 Index,bool Enabled)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;InitializeLively();
    if(!LivelyRules.IsValidIndex(Index-1))return;auto& R=LivelyRules[Index-1];R.Enabled=Enabled;
    if(!Enabled && LivelyFaceId==R.Id)CancelFace();
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY toggle index=%d id=%s enabled=%d"),Index,*R.Id.ToString(),Enabled);
}
void ACourtyardController::CourtyardReviewLivelyStart(int32 Index)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;InitializeLively();if(!LivelyRules.IsValidIndex(Index-1))return;
    const FName Id=LivelyRules[Index-1].Id;const bool Ok=StartLively(Id,true);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY review index=%d id=%s accepted=%d"),Index,*Id.ToString(),Ok);
}
