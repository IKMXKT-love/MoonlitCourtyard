#include "CourtyardController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace {
FLivelyFacePhase P2Face(float A,float B,std::initializer_list<TPair<int32,float>> V,float Fade=.3f){FLivelyFacePhase P;P.Start=A;P.End=B;P.Fade=Fade;for(const auto& I:V)P.Values.Add(I);return P;}
void P2Bool(UObject* A,FName Name,bool Value){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),Name))P->SetPropertyValue_InContainer(A,Value);}
}

bool ACourtyardController::LivelySkyOpen(float Up) const
{
    FVector E,F;if(!FaceFrame(E,F))return false;
    const float R=FMath::DegreesToRadians(Up);
    return VisibleTarget(E+(F*FMath::Cos(R)+FVector(0,0,FMath::Sin(R)))*5000) && VisibleTarget(E+FVector(0,0,650));
}
bool ACourtyardController::P2PlayerContext(float MaxDistance,float& Yaw) const
{
    if(Page!=ECourtyardPage::Game || !PlayerCharacter || PoseTransition || IsSeatBusy() || PlayerCharacter->GetVelocity().Size()>12)return false;
    FVector E,F,V;FRotator R;if(!FaceFrame(E,F))return false;GetPlayerViewPoint(V,R);
    const FVector D=V-E;const float Dist=D.Size();
    if(Dist<55 || Dist>MaxDistance || FVector::DotProduct(R.Vector(),-D.GetSafeNormal())<.96f)return false;
    Yaw=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F,D.GetSafeNormal2D()).Z,FVector::DotProduct(F,D.GetSafeNormal2D())));
    const float Pitch=FMath::RadiansToDegrees(FMath::Atan2(D.Z,D.Size2D()));
    return FMath::Abs(Yaw)<29 && Pitch>-30 && Pitch<15 && VisibleTarget(V,PlayerCharacter);
}
bool ACourtyardController::LivelyAuroraVisible() const
{
    if(LivelyTuneEnabled){
        if(!ContextSky)return false;FVector E,F;if(!FaceFrame(E,F))return false;
        const float T=GetWorld()->GetTimeSeconds()+ContextSky->K2_GetScalarParameterValue(TEXT("ReviewTimeOffset"));
        const float Center=FMath::Atan2(F.Y,F.X);
        for(float Offset:{-40.f,0.f,40.f}){const float Az=Center+FMath::DegreesToRadians(Offset);
            const float Flow=Az*4+.18f*FMath::Sin(Az*9+T*.07f)+T*.025f;
            const float El=.43f+.12f*FMath::Sin(Az*2+.7f)+.045f*FMath::Sin(Flow)+.014f*FMath::Sin(Az*7+T*.009f);
            const FVector D(FMath::Cos(El)*FMath::Cos(Az),FMath::Cos(El)*FMath::Sin(Az),FMath::Sin(El));
            if(VisibleTarget(E+D*5000))return true;
        }return false;
    }
    if(!ContextSky || !LivelySkyOpen())return false;
    // Same epoch, hash, drift and curtain centre as the installed QuietCirrus shader.
    auto Hash=[](uint32 X,uint32 Y,uint32 Z){uint32 N=X*1597334677u ^ Y*3812015801u ^ Z*2798796415u;N^=N>>16;N*=2246822519u;N^=N>>13;return float(N&16777215u)/16777216.f;};
    const float T=GetWorld()->GetTimeSeconds()+ContextSky->K2_GetScalarParameterValue(TEXT("ReviewTimeOffset"));
    const int32 Epoch=FMath::FloorToInt(T/120);const float Phase=T-Epoch*120-(25+Hash(Epoch,173,91)*65);
    const float Az=-PI+2*PI*Hash(Epoch,25,89);
    const float Flow=Az*4+.18f*FMath::Sin(Az*9+T*.07f)+T*.025f;
    const float El=.43f+.12f*FMath::Sin(Az*2+.7f)+.045f*FMath::Sin(Flow)+.014f*FMath::Sin(Az*7+T*.009f);
    if(Hash(Epoch,71,211)<.74f || Phase<8 || Phase>20 || El>.54f)return false;
    FVector E,F;if(!FaceFrame(E,F))return false;
    const FVector Dir(FMath::Cos(El)*FMath::Cos(Az),FMath::Cos(El)*FMath::Sin(Az),FMath::Sin(El));
    return FVector::DotProduct(F,Dir.GetSafeNormal2D())>.951f && VisibleTarget(E+Dir*5000);
}
bool ACourtyardController::AtAncientArrival() const
{
    auto* A=Cast<ACharacter>(Emma());
    if(A && LocoOn && LocoState==ELocoState::Idle && LocoPois.IsValidIndex(LocoAt) && LocoPois[LocoAt].Station==0 && LivingBehavior<0 && A->GetVelocity().Size()<2
        && AncientTree.IsValid() && FVector::Dist2D(A->GetActorLocation(),AncientTree->GetActorLocation())<700)return true;
    return A && LivingTrip && LivingTripPurpose==0 && !ReadingReturning && !ReadingPaused && !ReadingCurrent
        && ReadingSegment>=ReadingChain.Len() && LivingBehavior<0 && !BPBool(A,TEXT("TravelActive")) && !BPBool(A,TEXT("Interrupted"))
        && A->GetVelocity().Size()<2 && AncientAnchor.IsValid() && FVector::Dist2D(A->GetActorLocation(),AncientAnchor->GetActorLocation())<140;
}
bool ACourtyardController::AncientCanopyVisible() const
{
    FVector E,F;if(!AncientTree.IsValid() || !FaceFrame(E,F))return false;
    FVector Target=AncientTree->GetActorLocation();const FVector Delta=Target-E;
    if(Delta.Size2D()>1100 || FVector::DotProduct(F,Delta.GetSafeNormal2D())<.94f)return false;
    Target.Z=E.Z+Delta.Size2D()*FMath::Tan(FMath::DegreesToRadians(20.f));
    // The canopy is the intended target. Other scene geometry still blocks sight.
    return VisibleTarget(Target,AncientTree.Get());
}
bool ACourtyardController::PlayLivelyHead(FName Id,FName Name,bool AtStation)
{
    auto* Motion=LivelyMotion(Name,nullptr);if(!Motion)return false;
    if(!AtStation){if(!PlayCompanion(Motion,220,TEXT("lively_head")))return false;}
    else {
        auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
        if(!AtAncientArrival() || !I || ActiveAmbient || I->IsAnyMontagePlaying() || PendingLivelyPat || Page!=ECourtyardPage::Game || PoseTransition || IsSeatBusy() || !BPBool(A,TEXT("AllowInteraction")) || !QueryBool(A,TEXT("CanHeadPat")))return false;
        if(I->Montage_Play(Motion,1,EMontagePlayReturnType::MontageLength,0,false)<=0)return false;
        P2Bool(A,TEXT("AllowInteraction"),false);ActiveAmbient=Motion;AmbientActor=A;AmbientBehavior=221;
    }
    LivelyStarted(Id,AtStation?TEXT("ancient_arrival"):TEXT("head_motion"),true);return true;
}

// Edges are sampled even while she is busy. Expired surprises are discarded, not replayed later.
void ACourtyardController::SampleLivelyEvents(double Now)
{
    if(LivelyStage<2 || !PlayerCharacter || !ViewCamera)return;
    auto* A=Emma();FVector Eyes,Front;if(!A || !FaceFrame(Eyes,Front))return;
    const FVector Delta=ViewCamera->GetComponentLocation()-Eyes;
    const float Distance=FVector::Dist2D(PlayerCharacter->GetActorLocation(),A->GetActorLocation());
    const float Facing=FVector::DotProduct(Front,Delta.GetSafeNormal2D());
    const bool Visible=VisibleTarget(ViewCamera->GetComponentLocation(),PlayerCharacter);
    if(!P2SampleReady){P2SampleReady=true;P2WasVisible=Visible;P2PreviousDistance=Distance;P2PreviousFacing=Facing;P2HiddenSince=Visible?-1:Now;P2PoutUntil=-1;}
    else {
        if(!Visible && P2WasVisible)P2HiddenSince=Now;
        if(Visible && !P2WasVisible){
            const double Hidden=P2HiddenSince>=0?Now-P2HiddenSince:0;
            if(Hidden>=2 && Hidden<=10 && Distance<=400 && PlayerWatching && Facing>.85f)P2FoundUntil=Now+2;
            if(Hidden>=2 && Distance<=120)P2StartleUntil=Now+1;
            P2HiddenSince=-1;
        }
        if(P2PreviousDistance>150 && Distance<=150 && PlayerCharacter->GetVelocity().Size2D()>200 && P2PreviousFacing<.5f && Visible)P2StartleUntil=Now+1;
    }
    if(Distance>330){if(P2FarSince<0)P2FarSince=Now;if(Now-P2FarSince>=4)P2ReturnArmed=true;}
    else {P2FarSince=-1;if(Distance<=240 && P2ReturnArmed){P2PoutUntil=Mood==TEXT("lonely")?Now+25:-1;P2ReturnArmed=false;}}
    if(Distance<=300){if(P2NearSince<0)P2NearSince=Now;if(Now-P2NearSince>=60)P2NearQualified=true;}
    else {P2NearSince=-1;if(Distance>400 && P2NearQualified){P2ByeUntil=Now+10;P2NearQualified=false;}}
    float Y=0;const bool Gaze=Distance<=180 && P2PlayerContext(190,Y);
    if(Gaze){if(P2GazeSince<0)P2GazeSince=Now;}
    else if(LivelyBodyId!=TEXT("B2") && LivelyFaceId!=TEXT("B2"))P2GazeSince=-1;
    float StrictYaw=0;const bool StrictGaze=Distance<=180 && CompanionContext(StrictYaw);
    if(StrictGaze){if(P2StrictGazeSince<0)P2StrictGazeSince=Now;}
    else if(LivelyBodyId!=TEXT("B2") && LivelyFaceId!=TEXT("B2"))P2StrictGazeSince=-1;
    if(!PlayerWatching || Distance>240)P2GazeConsumed=false;
    if(P2ContestUntil>0){
        if(LivelyFaceId!=TEXT("B2"))P2ContestUntil=-1;
        else if(!PlayerWatching || Now>=P2ContestUntil){
            const bool Won=!PlayerWatching;FacePhases={Won?P2Face(0,1.3,{{17,.6f},{26,.9f}}):P2Face(0,1.7,{{11,.6f},{24,.8f}})};
            FaceStarted=Now;FaceEnd=Now+1.8;FaceLips.Empty();P2ContestUntil=-1;
            UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY phase id=B2 result=%s time=%.3f"),Won?TEXT("player_looked_away"):TEXT("emma_laughed"),Now);
        }
    }
    if(!P2WakePending && LivelyBodyId==TEXT("A3") && PlayerWatching && WatchedSince>P2HeadStarted && ActiveAmbient){
        if(auto* C=Cast<ACharacter>(A))if(auto* I=C->GetMesh()->GetAnimInstance()){I->Montage_Stop(.3f,ActiveAmbient);P2WakePending=true;}
    }
    bool MusicNow=false;if(MusicManager){TInlineComponentArray<UAudioComponent*> Audio;MusicManager->GetComponents(Audio);for(auto* C:Audio)MusicNow|=C->IsPlaying();}
    if(MusicNow && !P2MusicWasPlaying){P2MusicDueUntil=Now+25;P2SongConsumed=false;}
    if(!MusicNow && P2MusicWasPlaying){P2MusicDueUntil=-1;P2MusicStoppedUntil=Now+4;
        if(LivelyBodyId==TEXT("C16") && ActiveAmbient)if(auto* C=Cast<ACharacter>(A))if(auto* I=C->GetMesh()->GetAnimInstance())I->Montage_Stop(.4f,ActiveAmbient);
    }
    P2MusicWasPlaying=MusicNow;P2WasVisible=Visible;P2PreviousDistance=Distance;P2PreviousFacing=Facing;
}

bool ACourtyardController::TryP2Reaction()
{
    if(LivelyStage<2 || !LivelySafe(false))return false;
    const double Now=GetWorld()->GetTimeSeconds();auto* A=Emma();if(!A || !PlayerCharacter)return false;
    const float Distance=FVector::Dist2D(PlayerCharacter->GetActorLocation(),A->GetActorLocation());
    if(Now<P2StartleUntil && StartLively(TEXT("B4"))){P2StartleUntil=-1;return true;}
    if(Now<P2FoundUntil && Mood==TEXT("happy") && StartLively(TEXT("B9"))){P2FoundUntil=-1;return true;}
    if(Now<P2PoutUntil && Mood==TEXT("lonely") && Distance<=240 && P2WasVisible && WelcomeDone && Now-GreetingEndedAt>4 && StartLively(TEXT("B8"))){P2PoutUntil=-1;return true;}
    float GazeYaw=0;
    if(!P2GazeConsumed && P2StrictGazeSince>=0 && Now-P2StrictGazeSince>=6 && (Mood==TEXT("calm") || Mood==TEXT("happy")) && Distance<=180 && CompanionContext(GazeYaw) && StartLively(TEXT("B2"))){P2GazeConsumed=true;return true;}
    if(Now<P2ByeUntil && PlayerWatching && P2PreviousFacing>.85f && StartLively(TEXT("B13"))){P2ByeUntil=-1;return true;}
    if(!P2SongConsumed && Now<P2MusicDueUntil && (Mood==TEXT("happy") || Mood==TEXT("calm")) && StartLively(TEXT("C16"))){P2SongConsumed=true;return true;}
    if(LivelyAuroraVisible()){
        if(StartLively(TEXT("C3")))return true;
        if(LivelyOn(TEXT("C3")) && !LivelyReady(TEXT("C3"),false) && Now-LastC3CooldownLog>60){LastC3CooldownLog=Now;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVELY cooldown_held id=C3 time=%.2f"),Now);}
    }
    if(Pose==ECourtyardPose::Supine && Distance<=600 && P2WasVisible && Now-PoseSince>=2.5 && Now>=NextSkyShare && P2PreviousFacing>.85f && LivelySkyOpen() && StartLively(TEXT("B6"))){NextSkyShare=Now+FMath::FRandRange(100.f,160.f);return true;}
    if(Mood==TEXT("happy") && Distance<=150 && P2GazeSince>=0 && Now-P2GazeSince>=2 && StartLively(TEXT("B10")))return true;
    if(Now<P2AutonomousDue || !SeenOrWaited(P2Defer))return false;
    P2AutonomousDue=Now+FMath::FRandRange(35.f,65.f);
    if(Mood==TEXT("sleepy") && Now-AmbientStillSince>=30 && Now-LastBodyPat>=30 && StartLively(TEXT("D1")))return true;
    if(Distance<=900 && Mood==TEXT("calm") && FVector::Dist2D(A->GetActorLocation(),FVector(-113.75,-182.31,82.4))<45 && LivelySkyOpen() && StartLively(TEXT("C4")))return true;
    if((Mood==TEXT("calm") || Mood==TEXT("sleepy")) && LivelySkyOpen(LivelyTuneEnabled?25:12) && StartLively(TEXT("C6")))return true;
    return false;
}

bool ACourtyardController::StartP2(FName Id,bool Forced)
{
    if(LivelyStage<2)return false;
    FName Name;
    if(Id==TEXT("A3"))Name=TEXT("DaydreamHead");
    if(Id==TEXT("A5"))Name=TEXT("HumHead");
    if(Id==TEXT("B2")){
        // Calm: she shyly looks away. Happy: half the time a playful "who laughs first" contest instead.
        if(Mood!=TEXT("happy") || FMath::FRand()<.5f){float Y=0;P2PlayerContext(250,Y);Name=Y>=0?TEXT("ShyAwayOther"):TEXT("ShyAway");}
        else{const float Hold=FMath::FRandRange(3.f,5.f);if(StartFace(Id,{P2Face(0,Hold,{{34,.4f},{16,.6f},{23,.3f}})},Hold+2,TEXT(""),2)){P2ContestUntil=GetWorld()->GetTimeSeconds()+Hold;return true;}return false;}
    }
    if(Id==TEXT("B4"))Name=TEXT("StartleBody");
    if(Id==TEXT("B6"))Name=TEXT("CheckPlayerSky");
    if(Id==TEXT("B8")){float Y=0;if(!P2PlayerContext(250,Y) && !Forced)return false;Name=Y>=0?TEXT("PoutHeadOther"):TEXT("PoutHead");}
    if(Id==TEXT("B9")){float Y=0;if(!P2PlayerContext(410,Y))return false;Name=FName(*(FString(P2FoundCount>=2?TEXT("FoundProud"):TEXT("Found"))+(Y>8?TEXT("Left"):Y<-8?TEXT("Right"):TEXT("Center"))));}
    if(Id==TEXT("B10"))return StartLiving(3,Forced);
    if(Id==TEXT("B11"))Name=TEXT("LonelyWave");
    if(Id==TEXT("B13"))Name=GetWorld()->GetTimeSeconds()>1800?TEXT("GoodnightWave"):TEXT("ByeWave");
    if(Id==TEXT("C3"))Name=Mood==TEXT("happy") && FMath::FRand()<.5f?TEXT("SkyGazeHappy"):TEXT("SkyGaze");
    if(Id==TEXT("C4"))Name=TEXT("CountStars");
    if(Id==TEXT("C6"))Name=TEXT("CloudWatch");
    if(Id==TEXT("C16"))Name=TEXT("MusicNod");
    if(Id==TEXT("C17"))Name=TEXT("SmallStartle");
    if(Id==TEXT("D1"))Name=TEXT("Doze");
    if(Name.IsNone())return false;
    if(!Forced && Id==TEXT("C3") && LivelyTuneEnabled && !LivelyAuroraVisible())return false;
    if(!Forced && (Id==TEXT("B6") || (Id==TEXT("C3") && !LivelyTuneEnabled) || Id==TEXT("C4") || Id==TEXT("C6")) && !LivelySkyOpen(Id==TEXT("C6")?(LivelyTuneEnabled?25:12):20))return false;
    const bool OK=PlayLivelyHead(Id,Name);if(OK){P2HeadStarted=GetWorld()->GetTimeSeconds();if(Id==TEXT("B9"))++P2FoundCount;}
    return OK;
}
