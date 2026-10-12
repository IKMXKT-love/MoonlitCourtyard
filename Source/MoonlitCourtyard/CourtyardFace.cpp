#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Animation/AnimMontage.h"

namespace {
float FaceEase(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*U*(U*(U*6-15)+10);}
float FaceEnvelope(float T,float A,float B,float Fade){return FaceEase((T-A)/Fade)*FaceEase((B-T)/Fade);}
bool WholeFace(int32 N){return N==51 || N==53 || N==55 || N==56 || N==57 || (N>=59 && N<=62) || N==64 || N==66 || N==68;}
bool WideEye(int32 N){return N==30 || N==31 || N==41 || N==42 || N==56 || N==57;}
bool ClosedEye(int32 N){return (N>=24 && N<=29) || N==35 || N==63 || N==64 || N==65 || N==66;}   // #64 Ex_笑顔 closes the eyes (^^)
}

void ACourtyardController::ClearLivelyFace()
{
    if(auto* A=Cast<ACharacter>(Emma()))if(auto* M=A->GetMesh())
        for(const auto& P:FaceOwnedValues)M->SetMorphTarget(P.Key,0.f);
    FaceOwnedValues.Empty();
}
void ACourtyardController::CourtyardReviewFace(int32 Preset)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    ClearLivelyFace();FaceManualPaused=false;FaceProbe=FMath::Clamp(Preset,0,4);FaceProbeStarted=GetWorld()->GetTimeSeconds();FaceProbeReport=0;
    if(Preset>=10 && Preset<=13){FaceProbe=0;CourtyardReviewMood(Preset-10);}
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_FACE probe=%d time=%.3f"),FaceProbe,FaceProbeStarted);
}
void ACourtyardController::TickLivelyFace(float Dt)
{
    if(!FaceProbe){TickFaceLayers(Dt);return;}
    auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;auto* S=M?M->GetSkeletalMeshAsset():nullptr;
    if(!S)return;
    const double Now=GetWorld()->GetTimeSeconds(),T=Now-FaceProbeStarted;
    TMap<FName,float> Target;
    auto Add=[&](int32 Number,float Weight){if(S->GetMorphTargets().IsValidIndex(Number-1))Target.Add(S->GetMorphTargets()[Number-1]->GetFName(),Weight);};
    if(FaceProbe==1){Add(24,.8f);Add(18,.4f);}
    if(FaceProbe==2)Add(35,.65f);
    if(FaceProbe==3){float U=FMath::Clamp(float(T<1.5?T/1.5:(3-T)/1.5),0.f,1.f);U=U*U*U*(U*(U*6-15)+10);Add(24,U);Add(18,.4f*U);}
    if(FaceProbe==4)Add(73,1.f);
    for(const auto& P:Target){const float* Old=FaceOwnedValues.Find(P.Key);if(!Old || !FMath::IsNearlyEqual(*Old,P.Value,.0005f))M->SetMorphTarget(P.Key,P.Value);}
    FaceOwnedValues=Target;
    if(T>=FaceProbeReport){
        auto* I=M->GetAnimInstance();
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_FACE probe=%d elapsed=%.3f blink=%.3f component_smile=%.3f anim_smile=%.3f component_hau=%.3f anim_hau=%.3f"),FaceProbe,T,I?I->GetCurveValue(TEXT("まばたき")):0.f,M->GetMorphTarget(TEXT("笑い")),I?I->GetCurveValue(TEXT("笑い")):0.f,M->GetMorphTarget(TEXT("はぅ")),I?I->GetCurveValue(TEXT("はぅ")):0.f);
        FaceProbeReport+=.1;
    }
}

bool ACourtyardController::StartFace(FName Id,TArray<FLivelyFacePhase> Phases,float Length,const FString& Word,int32 Priority)
{
    if(!LivelyEnabled || FaceManualPaused || FaceProbe || (!LivelyFaceId.IsNone() && Priority<=FacePriority))return false;
    if(!LivelyFaceId.IsNone())LivelyFinished(LivelyFaceId,TEXT("replaced"));
    FacePhases=MoveTemp(Phases);FaceLips.Empty();FaceStarted=GetWorld()->GetTimeSeconds();FaceEnd=FaceStarted+Length;FaceCancel=-1;FacePriority=Priority;
    LivelyFaceId=Id;QueueLips(Word,1.f,Word==TEXT("mumble") || Word==TEXT("wish"));
    LivelyStarted(Id,TEXT("face"),false);return true;
}
void ACourtyardController::CancelFace(){if(!LivelyFaceId.IsNone() && FaceCancel<0)FaceCancel=GetWorld()->GetTimeSeconds();}
void ACourtyardController::QueueLips(const FString& Word,float Start,bool Quiet)
{
    if(Word.IsEmpty())return;
    TArray<int32> Tokens;
    if(Word==TEXT("okaeri"))Tokens={10,1,8,3};
    else if(Word==TEXT("nee"))Tokens={9,9};
    else if(Word==TEXT("kirei"))Tokens={3,8,3};
    else if(Word==TEXT("mite"))Tokens={3,8};
    else if(Word==TEXT("waa"))Tokens={5,2};
    else if(Word==TEXT("sugoi"))Tokens={5,10,3};
    else if(Word==TEXT("hai"))Tokens={1,3};
    else if(Word==TEXT("ehehe"))Tokens={8,8,8};
    else if(Word==TEXT("fufu"))Tokens={6,6};
    else if(Word==TEXT("mou"))Tokens={10,5};
    else if(Word==TEXT("yoshi"))Tokens={10,4};
    else if(Word==TEXT("mitsuketa"))Tokens={3,5,8,1};
    else if(Word==TEXT("count"))Tokens={3,3,0,3,0,1,7};
    else if(Word==TEXT("sigh"))Tokens={2};
    else if(Word==TEXT("oyasumi"))Tokens={10,1,5,3};
    else if(Word==TEXT("matane"))Tokens={1,1,9};
    else if(Word==TEXT("wish")){for(int32 N=0;N<5;++N)Tokens.Add(TArray<int32>{5,3,1}[FMath::RandHelper(3)]);}
    else if(Word==TEXT("mumble")){const int32 Count=FMath::RandRange(4,8);for(int32 N=0;N<Count;++N)Tokens.Add(TArray<int32>{1,3,8,9,10}[FMath::RandHelper(5)]);}
    else if(Word==TEXT("hum")){for(int32 N=0;N<18;++N)Tokens.Add(TArray<int32>{5,6,10}[FMath::RandHelper(3)]);}
    float T=Start;
    for(int32 N=0;N<Tokens.Num();++N){
        int32 S=Tokens[N];float Len=FMath::FRandRange(.13f,.18f),Weight=FMath::FRandRange(.4f,.7f);
        if(!S){T+=.25f;continue;}
        if(Word==TEXT("hum")){Len=FMath::FRandRange(.25f,.4f);Weight=FMath::FRandRange(.3f,.45f);}
        if(Word==TEXT("sigh"))Len=.75f;
        if(Word==TEXT("nee") && N==1)Len=.4f;
        if(Word==TEXT("waa") && N==1){Len=.6f;Weight=.35f;}
        if(Quiet){if(S==1)S=2;else if(S==3)S=4;else if(S==5)S=6;else if(S==8)S=9;Weight*=.6f;}
        FaceLips.Add({S,T,Len+.05f,Weight});T+=Len;
    }
}
// Ending pool (expression remap G2). The generic closing smile was removed from the clips; instead each listed action
// draws one short closing expression in its last 2 s, only once the clip's own face has faded. Endings are not tied to
// the action's category; E0 is "just relax". Mood shifts the odds and the same action never repeats its last ending.
namespace {
const TMap<int32,float>& EndingFace(int32 E){
    // Strong enough to read at normal distance (first A/B: 0.2-0.35 looked like no ending at all).
    static const TMap<int32,float> Ends[8]={{},{{45,.55f},{18,.35f}},{{17,.55f}},{{43,.45f},{16,.35f}},{{54,.6f}},{{6,.55f}},{{25,.5f},{2,.25f}},{{69,.45f}}};
    return Ends[FMath::Clamp(E,0,7)];
}
TArray<int32> EndingPoolFor(const FString& N,int32 Response){
    if(N.Contains(TEXT("HeadPatSmile")))return Response==11?TArray<int32>{1,4,0}:Response==12?TArray<int32>{7,1,0}:Response==13?TArray<int32>{5,0}:TArray<int32>{};
    if(N.Contains(TEXT("Chuckle")))return {2,1,0};
    if(N.Contains(TEXT("ShyAway")))return {4,1,0};
    if(N.Contains(TEXT("WelcomeLonely")))return {7,1,0};
    if(N.Contains(TEXT("WaveLonely")))return {7,5,0};
    if(N.Contains(TEXT("WelcomeShy")))return {4,1,0};
    if(N.Contains(TEXT("WaveShy")))return {4,0};
    if(N.Contains(TEXT("HighFiveResponse")))return {2,1,7};
    if(N.Contains(TEXT("CountStars")))return {3,2,0};
    if(N.Contains(TEXT("PetalMiss")))return {5,6,0};
    if(N.Contains(TEXT("MothWatch")))return {3,1,0};
    if(N.Contains(TEXT("Found")))return {2,1,0};
    return {};
}
}
void ACourtyardController::TickEndingPool(double Now,UAnimInstance* I,TMap<int32,float>& Desired,TFunctionRef<float(int32)> Anim)
{
    if(!ExpressionRemapOn)return;
    if(UAnimMontage* M=I->GetCurrentActiveMontage()){
        const float Pos=I->Montage_GetPosition(M),Len=M->GetPlayLength();const double Instance=Now-Pos;
        const bool Same=EndingMontage.Get()==M && FMath::Abs(Instance-EndingInstance)<.25;
        if(!Same && EndingStart>-50 && EndingCancel<0)EndingCancel=Now;     // a new action takes the face over
        if(!Same && Pos>=Len-2.f){
            const FString Name=M->GetName();const TArray<int32> Pool=EndingPoolFor(Name,LastResponse);
            bool Quiet=true;for(int32 N=1;N<=72 && Quiet;++N)if(N!=25 && Anim(N)>.08f)Quiet=false;
            if(Pool.Num() && Quiet){
                const FString Key=Name+FString::FromInt(Name.Contains(TEXT("HeadPatSmile"))?LastResponse:0);const int32* Last=EndingLast.Find(Key);
                TArray<int32> Options;TArray<float> Weights;float Sum=0;
                for(int32 E:Pool){if(Pool.Num()>1 && Last && *Last==E)continue;
                    float W=E==0?.5f:1.f;if(Mood==TEXT("happy")&&(E==1||E==2))W=2;if(Mood==TEXT("lonely")&&E==7)W=2;if(Mood==TEXT("lonely")&&E==5)W=1.5f;if(Mood==TEXT("sleepy")&&(E==0||E==6))W*=2;
                    Options.Add(E);Weights.Add(W);Sum+=W;}
                float Roll=FMath::FRand()*Sum;int32 Pick=Options.Last();for(int32 K=0;K<Options.Num();++K){Roll-=Weights[K];if(Roll<=0){Pick=Options[K];break;}}
                EndingLast.Add(Key,Pick);EndingMontage=M;EndingInstance=Instance;EndingStart=Now;EndingCancel=-1;EndingValues=EndingFace(Pick);
                UE_LOG(LogTemp,Display,TEXT("COURTYARD_ENDING montage=%s response=%d ending=E%d mood=%s t=%.2f"),*Name,LastResponse,Pick,*Mood,Now);
            }
        }
    }
    if(EndingStart>-50){
        const float T=Now-EndingStart;float W=FaceEnvelope(T,0,2.4f,.4f);if(EndingCancel>=0)W*=1-FaceEase(float((Now-EndingCancel)/.3));
        if(T>2.4f || (EndingCancel>=0 && Now-EndingCancel>.3)){EndingStart=-100;EndingValues.Empty();EndingCancel=-1;}
        else for(const auto& P:EndingValues)Desired.FindOrAdd(P.Key)=FMath::Max(Desired.FindRef(P.Key),P.Value*W);
    }
}
void ACourtyardController::TickFaceLayers(float Dt)
{
    if(!LivelyEnabled || FaceManualPaused)return;
    auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;auto* I=M?M->GetAnimInstance():nullptr;auto* S=M?M->GetSkeletalMeshAsset():nullptr;
    if(!I || !S)return;
    const double Now=GetWorld()->GetTimeSeconds();const auto& Morphs=S->GetMorphTargets();
    auto Anim=[&](int32 N){return Morphs.IsValidIndex(N-1)?I->GetCurveValue(Morphs[N-1]->GetFName()):0.f;};
    const bool Montage=I->IsAnyMontagePlaying();
    if(Montage || LivingTrip || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted")))CancelFace();
    if(!LivelyFaceId.IsNone() && (Now>=FaceEnd || (FaceCancel>=0 && Now-FaceCancel>=.3))){
        const FName Done=LivelyFaceId;LivelyFaceId=NAME_None;FacePhases.Empty();FaceLips.Empty();FacePriority=0;LivelyFinished(Done,FaceCancel>=0?TEXT("yield"):TEXT("finished"));FaceCancel=-1;
        if(Done==TEXT("A7"))MoodEvent(TEXT("rest"));
    }
    if(FaceBaselineMood!=Mood || FaceBaselineLogged!=LivelyOn(TEXT("A1"))){
        if(FaceBaselineLogged)LivelyFinished(TEXT("A1"),TEXT("baseline_change"));
        FaceBaselineFrom=FaceBaseline;FaceBaselineMood=Mood;FaceBaselineChanged=Now;FaceBaselineLogged=LivelyOn(TEXT("A1"));
        if(FaceBaselineLogged)UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY start id=A1 mood=%s seen=%d reason=baseline slot=baseline time=%.3f"),*Mood,PlayerWatching,Now);
    }
    TMap<int32,float> BaseTarget;
    if(LivelyOn(TEXT("A1"))){
        if(Mood==TEXT("happy"))BaseTarget=LivelyTuneEnabled?TMap<int32,float>{{18,.45f},{45,.5f},{40,.5f}}:TMap<int32,float>{{18,.3f},{45,.35f},{40,.35f}};
        else if(Mood==TEXT("sleepy"))BaseTarget=LivelyTuneEnabled?TMap<int32,float>{{25,.45f},{39,.3f},{38,.5f}}:TMap<int32,float>{{25,.3f},{38,.35f},{48,.3f}};
        else if(Mood==TEXT("lonely"))BaseTarget=LivelyTuneEnabled?TMap<int32,float>{{52,.5f},{38,.5f},{22,.35f}}:TMap<int32,float>{{52,.3f},{38,.35f},{22,.25f}};
    }
    TSet<int32> BaseKeys;for(const auto& P:BaseTarget)BaseKeys.Add(P.Key);for(const auto& P:FaceBaselineFrom)BaseKeys.Add(P.Key);
    FaceBaseline.Empty();const float Blend=FaceEase(float((Now-FaceBaselineChanged)/3.));
    for(int32 N:BaseKeys)FaceBaseline.Add(N,FMath::Lerp(FaceBaselineFrom.FindRef(N),BaseTarget.FindRef(N),Blend));
    FaceBaselineGate=FMath::FInterpConstantTo(FaceBaselineGate,Montage?0.f:1.f,Dt,Montage?1.f/.3f:1.f/3.f);
    TMap<int32,float> Desired;
    for(const auto& P:FaceBaseline)Desired.Add(P.Key,P.Value*FaceBaselineGate);
    const float T=Now-FaceStarted,Cancel=FaceCancel>=0?1-FaceEase(float((Now-FaceCancel)/.3)):1;
    TMap<int32,float> Clip;
    if(!LivelyFaceId.IsNone())for(const auto& Phase:FacePhases){
        const float W=FaceEnvelope(T,Phase.Start,Phase.End,Phase.Fade)*Cancel;
        for(const auto& P:Phase.Values)Clip.FindOrAdd(P.Key)=FMath::Max(Clip.FindRef(P.Key),P.Value*W);
    }
    // Whole-face ownership includes eyes, eyebrows and mouth; fade out baseline before it enters.
    int32 Whole=0;float WholeWeight=0;
    for(const auto& P:Clip)if(WholeFace(P.Key) && P.Value>WholeWeight){Whole=P.Key;WholeWeight=P.Value;}
    if(WholeWeight>.001f){
        for(auto& P:Desired)P.Value*=1-FaceEase(WholeWeight/.15f);
        for(auto& P:Clip)if(P.Key!=Whole && P.Key<69)P.Value*=1-FaceEase(WholeWeight/.15f);
    }
    for(const auto& P:Clip)Desired.FindOrAdd(P.Key)=FMath::Max(Desired.FindRef(P.Key),P.Value);
    // Mouth source precedence: complete preset, then one cross-faded phoneme stream, then baseline.
    if(WholeWeight<.05f && !Montage){
        TMap<int32,float> Lips;float Sum=0;
        for(const auto& Lip:FaceLips){float W=FaceEnvelope(T,Lip.Start,Lip.Start+Lip.Length,.05f)*Lip.Weight*Cancel;Lips.FindOrAdd(Lip.Shape)+=W;Sum+=W;}
        if(Sum>.001f){for(int32 N=1;N<=(LivelyTuneEnabled?15:23);++N)Desired.Remove(N);for(const auto& P:Lips)Desired.Add(P.Key,P.Value/FMath::Max(1.f,Sum/.7f));}
    }else if(WholeWeight>=.05f){for(int32 N=1;N<=(LivelyTuneEnabled?15:23);++N)Desired.Remove(N);}
    // Blinks during actions (expression remap G1): the fixed blinks baked into the clips were removed, so the random blink
    // layer also runs while an action plays, but only while the clip's own eyes are plainly open.
    // Uses the shared eye lists: a hand-copied list here once missed #64 (laughing ^^ eyes in the opening wave), so a random
    // blink was stacked on already-closed eyes.
    bool EyesFree=Anim(25)<.05f;float AnimClosed=0;
    for(int32 N=24;N<=68;++N)if(N!=25){if((ClosedEye(N) || WideEye(N)) && Anim(N)>.15f)EyesFree=false;if(ClosedEye(N))AnimClosed=FMath::Max(AnimClosed,Anim(N));}
    const bool BlinkHere=!Montage || (ExpressionRemapOn && EyesFree);
    if(Montage && ExpressionRemapOn && !BlinkWasMontage)NextExtraBlink=FMath::Min(NextExtraBlink,Now+FMath::FRandRange(.8f,3.f));
    BlinkWasMontage=Montage;
    if(ExtraBlinkLogged && (!BlinkHere || !LivelyOn(TEXT("A2")) || Now-ExtraBlinkStart>=ExtraBlinkLength)){
        LivelyFinished(TEXT("A2"),TEXT("blink_end"));ExtraBlinkLogged=false;
    }
    if(BlinkHere && WholeWeight<.001f && LivelyOn(TEXT("A2")) && Now>=NextExtraBlink && Anim(25)<.02f){
        ExtraBlinkStart=Now;ExtraBlinkLength=Mood==TEXT("sleepy")?(Montage?.6f:FMath::FRandRange(1.f,1.5f)):.5f;ExtraBlinkDouble=Mood!=TEXT("sleepy") && FMath::FRand()<.4f;
        ExtraBlinkLogged=true;
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY start id=A2 mood=%s seen=%d reason=%s slot=blink time=%.3f"),*Mood,PlayerWatching,ExtraBlinkDouble?TEXT("double"):TEXT("slow"),Now);
        // During an action the idle animation's own blinks are absent, so the layer blinks more often (about twice per 8 s action).
        NextExtraBlink=Now+(Montage&&ExpressionRemapOn?FMath::FRandRange(2.5f,5.5f)*(Mood==TEXT("sleepy")?1.3f:1.f):FMath::FRandRange(6.f,14.f));
    }
    float Blink=Anim(25),RandomBlink=0;
    if(BlinkHere && LivelyOn(TEXT("A2"))){const float BT=Now-ExtraBlinkStart;const float B=ExtraBlinkDouble?FMath::Max(FaceEnvelope(BT,0,.14f,.065f),FaceEnvelope(BT,.23f,.37f,.065f)):FaceEnvelope(BT,0,ExtraBlinkLength,ExtraBlinkLength*.45f);Blink=FMath::Max(Blink,B);RandomBlink=B;}
    float Closed=0,Wide=0;
    for(const auto& P:Desired){if(ClosedEye(P.Key))Closed=FMath::Max(Closed,P.Value);if(WideEye(P.Key))Wide=FMath::Max(Wide,P.Value);}
    // Incompatible eyes never accumulate. The authored montage owns its own eye composition.
    if(!Montage && WholeWeight<.05f){
        if(Wide>Closed){for(auto& P:Desired)if(ClosedEye(P.Key))P.Value*=1-FaceEase(Wide/.2f);}
        else{for(auto& P:Desired)if(WideEye(P.Key))P.Value*=1-FaceEase(Closed/.2f);}
        if(Blink>.02f && Wide<.1f){for(auto& P:Desired)if(ClosedEye(P.Key) && P.Key!=25)P.Value*=1-Blink;Desired.FindOrAdd(25)=FMath::Max(Desired.FindRef(25),Blink);}
    }
    // Authored montage faces win as soon as they carry facial expression, including during E entry.
    // The component fade normally finishes before the authored .4s face envelope starts.
    if(Montage){bool AuthoredFace=false;for(int32 N=1;N<=72;++N)if(N!=25 && Anim(N)>.02f){AuthoredFace=true;break;}if(AuthoredFace)Desired.Empty();}
    // A random blink already under way fades out if the clip's own closed eyes start rising (never stacks on them).
    if(Montage && BlinkHere && Blink>.02f)Desired.FindOrAdd(25)=FMath::Max(Desired.FindRef(25),FMath::Max(Anim(25),RandomBlink*(1-FaceEase(AnimClosed/.15f))));
    TickEndingPool(Now,I,Desired,[&](int32 N){return Anim(N);});
    TMap<FName,float> Target;
    for(const auto& P:Desired)if(P.Key>=1 && P.Key<=72 && Morphs.IsValidIndex(P.Key-1) && P.Value>.001f){
        const FName Name=Morphs[P.Key-1]->GetFName();Target.Add(Name,FMath::Clamp(FMath::Max(P.Value,Anim(P.Key)),0.f,1.f));
    }
    // Component values override animation values in this UE version. Temporarily attenuate only
    // conflicting idle channels, then remove our overrides so the original blink resumes.
    if(!Montage){
        const float Own=FaceEase(WholeWeight/.15f),WideGate=FaceEase(Wide/.2f);
        for(int32 N=1;N<=68;++N)if(Morphs.IsValidIndex(N-1)){
            float Gate=1.f;
            if(Own>0 && N!=Whole)Gate*=1-Own;
            if(Wide>Closed && ClosedEye(N))Gate*=1-WideGate;
            if(Gate<1){const FName Name=Morphs[N-1]->GetFName();const float Value=FMath::Max(Desired.FindRef(N),Anim(N))*Gate;
                if(Value>.001f || Anim(N)>.001f || FaceOwnedValues.Contains(Name))Target.Add(Name,Value);
            }
        }
    }
    TArray<FName> Positive;for(const auto& P:Target)if(P.Value>.001f)Positive.Add(P.Key);
    Positive.Sort([&](FName AName,FName BName){return Target[AName]>Target[BName];});
    for(int32 N=12;N<Positive.Num();++N)Target[Positive[N]]=0.f;
    FacePeakChannels=FMath::Max(FacePeakChannels,FMath::Min(12,Positive.Num()));
    if(ContentLayer.IsValid() && ContentLayer->FixEnabled)ContentLayer->ComposeFixedFace(Target);
    // Remove only overrides that this layer created. Animation curves remain untouched.
    for(const auto& P:FaceOwnedValues)if(!Target.Contains(P.Key))M->SetMorphTarget(P.Key,0.f,true);
    for(const auto& P:Target){const float* Old=FaceOwnedValues.Find(P.Key);if(!Old || !FMath::IsNearlyEqual(*Old,P.Value,.0005f))M->SetMorphTarget(P.Key,P.Value,false);}
    FaceOwnedValues=MoveTemp(Target);
}
