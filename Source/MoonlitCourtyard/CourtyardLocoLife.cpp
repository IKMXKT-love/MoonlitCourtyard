#include "CourtyardController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

// Locomotion rework, phases B-D (Claude, 2026-10-06; plan Builds/Review/Locomotion_Plan.html, user decisions: base speed
// A with mood changes, approach C (occasional first), follow C (by mood), house ground floor allowed).
// She now owns her own days on foot: the blueprint's hub routes and the old segment trips are off while this runs.
// - where: 19 places (stations keep their exact spots and behaviours), never the last few again, mood-weighted
// - leaving: a look and smile at the player in front of her, a look toward where she is going, then the first step
// - on the way: glances at the player or the scenery, sometimes a short pause at a pretty spot, stops for a player in the way
// - arriving: the place's own behaviour (chime, reading, ancient sakura); otherwise the existing idle life runs there
// - the player: now and then she comes over to a player who stands still; she may follow a player who walks off (mood)

namespace {
void LifeFlag(AActor* A,FName N,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,V);}
void LifeTime(AActor* A,FName N,double V){if(A)if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N))if(P->IsFloatingPoint())P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),V);}
float Rand(float A,float B){return FMath::FRandRange(A,B);}
}

void ACourtyardController::InitLocoLife()
{
    if(LocoLifeReady)return;LocoLifeReady=true;
    // Name, spot, facing, indoor, station (-1 none, 1 chime, 5 reading, 0 ancient sakura), what she looks at there.
    const FVector Fountain(0,-1410,130),Ancient(-800,-2530,650),Garden(0,-900,120);
    auto Add=[&](const TCHAR* N,float X,float Y,float Yaw,bool In,int32 St,FVector F){LocoPois.Add({N,FVector(X,Y,82.4),Yaw,In,St,F});};
    Add(TEXT("home"),-195,0,105,false,-1,FVector(-210,300,120));
    Add(TEXT("chime"),46.4f,132,45,false,1,FVector(59,170,165));
    Add(TEXT("fountain_n"),0,-1000,-90,false,-1,Fountain);
    Add(TEXT("fountain_s"),0,-1820,90,false,-1,Fountain);
    Add(TEXT("fountain_e"),420,-1400,180,false,-1,Fountain);
    Add(TEXT("fountain_w"),-420,-1400,0,false,-1,Fountain);
    Add(TEXT("fountain_sw"),-330,-1720,43,false,-1,Fountain);
    Add(TEXT("bench_south"),0,-2750,-90,false,-1,FVector(0,-3000,90));   // she walks up to the south bench
    Add(TEXT("ancient"),-300,-2440,-170,false,0,Ancient);
    Add(TEXT("lawn_e"),900,-700,180,false,-1,Garden);
    Add(TEXT("south_e"),700,-2400,125,false,-1,Fountain);
    Add(TEXT("lawn_w"),-1100,-600,0,false,-1,Garden);
    Add(TEXT("west_path"),-1000,-1400,0,false,-1,Fountain);
    Add(TEXT("terrace"),0,260,-90,false,-1,Garden);
    Add(TEXT("path_mid"),0,-550,-90,false,-1,Garden);
    Add(TEXT("entrance_in"),0,620,-90,true,-1,FVector(0,0,140));
    Add(TEXT("hall_n"),0,1150,90,true,-1,FVector(0,1700,250));
    Add(TEXT("hall_w"),-450,650,180,true,-1,FVector(-900,700,140));
    Add(TEXT("reading"),-799.5f,870.7f,105,true,5,FVector(-807,913,110));
    LocoAt=0;LocoNextDecision=GetWorld()->GetTimeSeconds()+Rand(25.f,45.f);LocoNextPace=GetWorld()->GetTimeSeconds()+Rand(12.f,20.f);   // after the welcome, first walk
    if(auto* A=Emma()){LifeFlag(A,TEXT("ActivityEnabled"),false);LifeTime(A,TEXT("DueTime"),LocoNextDecision);}
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE ready places=%d first_walk_in=%.0f"),LocoPois.Num(),LocoNextDecision-GetWorld()->GetTimeSeconds());
}

bool ACourtyardController::LocoIndoors()
{
    const double Now=GetWorld()->GetTimeSeconds();
    if(Now-LocoIndoorAt<1.)return LocoIndoorCached;
    LocoIndoorAt=Now;auto* A=Cast<ACharacter>(Emma());if(!A)return false;
    const FVector Head=A->GetMesh()->GetSocketLocation(TEXT("頭"));FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(LocoRoof),false,A);
    LocoIndoorCached=GetWorld()->LineTraceSingleByChannel(H,Head,Head+FVector(0,0,1200),ECC_Visibility,Q);
    return LocoIndoorCached;
}

// Mood-weighted choice of the next place: not where she is, not the last three; sleepy prefers rest places and indoors,
// happy prefers the fountain/lawns, moderate distances are favoured. Stations obey their own cooldowns.
int32 ACourtyardController::LocoChoose()
{
    auto* A=Emma();if(!A)return -1;
    const FVector Pos=A->GetActorLocation();const double Now=GetWorld()->GetTimeSeconds();
    TArray<float> W;W.SetNum(LocoPois.Num());float Sum=0;
    for(int32 I=0;I<LocoPois.Num();++I){
        const auto& P=LocoPois[I];float Wt=1;
        if(I==LocoAt || LocoRecent.Contains(I)){W[I]=0;continue;}
        const float D=FVector::Dist2D(Pos,P.Pos);
        Wt*=D<250?.2f:D<1600?1.f:D<2600?.6f:.3f;
        if(Mood==TEXT("sleepy"))Wt*=D<1000?1.f:D<1600?.45f:.15f;   // sleepy: drifts to somewhere close
        if(P.Indoor)Wt*=Mood==TEXT("sleepy")?1.3f:.55f;
        if(P.Station==1 && Now<LivingCooldown[1])Wt*=.15f;
        if(P.Station==5 && Now<LivingCooldown[5])Wt*=.15f;
        if(P.Station==0 && Now<LivingCooldown[0])Wt*=.3f;
        if(Mood==TEXT("happy") && P.Name.ToString().StartsWith(TEXT("fountain")))Wt*=1.4f;
        if(Mood==TEXT("sleepy") && (P.Name==TEXT("home") || P.Name==TEXT("bench_south") || P.Name==TEXT("reading")))Wt*=1.6f;
        if(Mood==TEXT("lonely") && PlayerCharacter){const float DP=FVector::Dist2D(PlayerCharacter->GetActorLocation(),P.Pos);Wt*=DP<600?1.8f:1.f;}
        if(P.Name==TEXT("path_mid"))Wt*=.4f;
        W[I]=Wt;Sum+=Wt;
    }
    if(Sum<=0)return -1;
    float R=FMath::FRand()*Sum;for(int32 I=0;I<W.Num();++I){R-=W[I];if(R<=0 && W[I]>0)return I;}
    return -1;
}

bool ACourtyardController::LocoDepartTo(int32 Poi)
{
    if(!LocoPois.IsValidIndex(Poi))return false;
    const auto& P=LocoPois[Poi];
    // stations need their exact spot and facing; other places allow a natural stop near the spot
    const bool Exact=P.Station==1 || P.Station==5 || P.Name==TEXT("home");
    // only the stations need an exact facing (straight run-in); elsewhere she arrives the way she walks and looks round
    if(!LocoStartWalk(P.Pos,P.Yaw,Exact,P.Name,Exact?12.f:6.f))return false;
    LocoHeading=Poi;LocoNextGlance=GetWorld()->GetTimeSeconds()+Rand(2.5f,4.f);return true;
}

// Where to stand for the player: in front of them (toward her), about 1.55 m away, facing them; or, following, behind
// and to one side of them. Snapped onto the walkable area.
bool ACourtyardController::LocoSocialGoal(bool Follow,FVector& Goal,float& Yaw)
{
    auto* A=Emma();if(!A || !PlayerCharacter)return false;
    const FVector Me=A->GetActorLocation(),P=PlayerCharacter->GetActorLocation();
    FVector Want;
    if(!Follow){const FVector Dir=(Me-P).GetSafeNormal2D();Want=P+Dir*155.f;}
    else{
        const FVector F=PlayerCharacter->GetVelocity().Size2D()>20?PlayerCharacter->GetVelocity().GetSafeNormal2D():PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
        const FVector Rt(-F.Y,F.X,0);const float Side=FVector::DotProduct(Me-P,Rt)>=0?1.f:-1.f;
        Want=P-F*170.f+Rt*Side*70.f;
    }
    Want.Z=Me.Z;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Snap;
    if(!Nav || !Nav->ProjectPointToNavigation(Want,Snap,FVector(120,120,200)))return false;
    Goal=FVector(Snap.Location.X,Snap.Location.Y,Me.Z);
    Yaw=FMath::RadiansToDegrees(FMath::Atan2(P.Y-Goal.Y,P.X-Goal.X));
    return FVector::Dist2D(Goal,P)>110.f;
}

void ACourtyardController::LocoRetarget(const FVector& Goal,float Yaw,bool HasYaw)
{
    if(LocoState!=ELocoState::Cruise)return;
    auto* A=Emma();if(!A)return;
    TArray<FVector> Raw;const FVector From=A->GetActorLocation();
    if(!LocoFindPath(From,Goal,Raw))return;
    for(auto& P:Raw)P.Z=From.Z;
    LocoPath=Raw;LocoPath[0]=From;LocoPathDist.Reset();LocoPathDist.Add(0);
    for(int32 K=1;K<LocoPath.Num();++K)LocoPathDist.Add(LocoPathDist.Last()+FVector::Dist2D(LocoPath[K-1],LocoPath[K]));
    LocoProgress=0;LocoGoal=Goal;LocoGoalYaw=Yaw;LocoGoalHasYaw=HasYaw;LocoGoalTolerance=6;
}

void ACourtyardController::TickLocoLife(float Dt)
{
    if(!LocoOn || Page!=ECourtyardPage::Game)return;
    InitLocoLife();
    if(LocoLifePaused)return;
    auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;if(!A || !I)return;
    const double Now=GetWorld()->GetTimeSeconds();
    const FVector Me=A->GetActorLocation();
    LocoMoodRate=Mood==TEXT("happy")?1.12f:Mood==TEXT("sleepy")?.86f:Mood==TEXT("lonely")?.92f:1.f;
    // the player's movement
    FVector PP=PlayerCharacter?PlayerCharacter->GetActorLocation():FVector::ZeroVector;
    const float PSpeed=PlayerCharacter?PlayerCharacter->GetVelocity().Size2D():0.f,PDist=PlayerCharacter?FVector::Dist2D(PP,Me):1e9f;
    if(PSpeed<15.f){if(LocoPlayerStillSince<0)LocoPlayerStillSince=Now;LocoPlayerMovingSince=-1;}else{LocoPlayerStillSince=-1;if(LocoPlayerMovingSince<0)LocoPlayerMovingSince=Now;}
    float PYaw=0,PDistB=0;const bool Bearing=PlayerBearing(PYaw,PDistB);
    const bool PlayerUp=Pose==ECourtyardPose::Standing && !PoseTransition;

    // ---------------------------------------------------------------- walking
    if(LocoState!=ELocoState::Idle){
        LocoLastArrived=false;
        if(LocoSocial==1 && Now>=LocoRetargetAt){   // coming to the player: keep aiming at where they stand now
            LocoRetargetAt=Now+1.;FVector G;float Y;
            if(LocoSocialGoal(false,G,Y) && FVector::Dist2D(G,LocoGoal)>60.f)LocoRetarget(G,Y,true);
        }
        if(LocoSocial==2){   // following
            LocoMoodRate=PDist>450.f?1.22f:LocoMoodRate;
            const bool Stopped=LocoPlayerStillSince>=0 && Now-LocoPlayerStillSince>1.2;
            if(PDist>1500.f){if(LocoFarSince<0)LocoFarSince=Now;}else LocoFarSince=-1;
            if(Now>=LocoRetargetAt){
                LocoRetargetAt=Now+1.;FVector G;float Y;
                if(LocoSocialGoal(!Stopped,G,Y) && FVector::Dist2D(G,LocoGoal)>50.f)LocoRetarget(G,Y,Stopped);
            }
            if(Now>LocoFollowUntil || (LocoFarSince>=0 && Now-LocoFarSince>6.)){LocoSocial=0;LocoStopRequested=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE follow_end reason=%s t=%.2f"),Now>LocoFollowUntil?TEXT("time"):TEXT("too_far"),Now);}
        }
        // a player standing in her way: she slows, stops in front of them and looks at them
        if(PlayerCharacter && LocoSocial==0){
            // only a player squarely on the path within about a metre ahead blocks her (otherwise she steps round them)
            bool Block=false;
            for(float S=20.f;S<=110.f && !Block && PSpeed<60.f;S+=15.f){
                const FVector C=LocoPointAt(LocoProgress+S);if(FVector::Dist2D(C,PP)<38.f)Block=true;
            }
            const FVector To=PP-Me;
            if(Block){if(LocoBlockedSince<0)LocoBlockedSince=Now;if(!LocoStopRequested){LocoStopRequested=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE blocked_by_player distance=%.0f t=%.2f"),To.Size2D(),Now);}}
            else LocoBlockedSince=-1;
        }
        // glances on the way: the player when they are in view, otherwise the scenery; the destination near the end
        if(Now>=LocoNextGlance && LocoState==ELocoState::Cruise){
            const float Remaining=LocoPathDist.Num()?LocoPathDist.Last()-LocoProgress:0;
            if(LocoPois.IsValidIndex(LocoHeading) && Remaining<220.f){GazePoi=LocoPois[LocoHeading].Focus;GazePoiUntil=Now+1.6;GazePoiHead=.45f;LocoNextGlance=Now+30;}
            else if(Bearing && PDistB<900.f && FMath::Abs(PYaw)<80.f && FMath::FRand()<(Mood==TEXT("lonely")?.8f:.55f)){
                FVector V;FRotator R;GetPlayerViewPoint(V,R);GazePoi=V;GazePoiUntil=Now+Rand(1.f,1.6f);GazePoiHead=.5f;
            }else{
                // scenery: the nearest interesting thing ahead of her (fountain, trees, the house), or up at the sky outside
                const FVector F=A->GetActorForwardVector();FVector Best=Me+F*400+FVector(0,0,FMath::FRand()<.4f && !LocoIndoors()?500:60);float BestScore=-1;
                for(const auto& P:LocoPois){const FVector D=P.Focus-Me;const float Dist=D.Size2D();if(Dist<200 || Dist>1500)continue;
                    const float Dot=FVector::DotProduct(D.GetSafeNormal2D(),F);if(Dot<.35f)continue;const float S=Dot/Dist;if(S>BestScore){BestScore=S;Best=P.Focus;}}
                GazePoi=Best;GazePoiUntil=Now+Rand(.9f,1.4f);GazePoiHead=.42f;
            }
            LocoNextGlance=Now+(Mood==TEXT("happy")?Rand(2.8f,5.f):Mood==TEXT("sleepy")?Rand(5.f,8.f):Rand(3.5f,6.5f));
        }
        return;
    }

    // ---------------------------------------------------------------- just arrived
    if(LocoLastEndAt>=0 && !LocoLastArrived){
        LocoLastArrived=true;
        if(!LocoEndArrived && LocoSocial==0 && LocoPois.IsValidIndex(LocoHeading)){   // stopped short (player in the way): look, then go on
            LocoNextDecision=Now+(Now-LocoPatWantedAt<8.?15.f:Rand(3.f,6.f));LocoDepartStage=1;LocoDepartAt=LocoNextDecision;LifeTime(A,TEXT("DueTime"),LocoNextDecision+2);
            UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE stopped_short heading=%s resume_in=%.1f t=%.2f"),*LocoPois[LocoHeading].Name.ToString(),LocoNextDecision-Now,Now);
            return;
        }
        if(LocoSocial==2 && Now<LocoFollowUntil && PDist>320.f && PlayerCharacter){   // still following: the player moved on, so does she
            FVector G;float Y;const bool Moving=LocoPlayerStillSince<0;
            if(LocoSocialGoal(Moving,G,Y)){if(Moving && FVector::Dist2D(G,Me)<260.f)G=PP;if(LocoStartWalk(G,Y,!Moving,TEXT("follow"),6.f)){LocoRetargetAt=Now+1.;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE follow_continue distance=%.0f t=%.2f"),PDist,Now);return;}}
        }
        if(LocoSocial==1 || LocoSocial==2){
            UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE with_player social=%d distance=%.0f t=%.2f"),LocoSocial,PDist,Now);
            LocoSocial=0;LocoAt=-1;LocoNextDecision=Now+Rand(45.f,90.f);LocoNextPace=Now+Rand(30.f,50.f);LocoForceSocial=0;   // she stays with you a while
            FLivelyFacePhase Smile;Smile.Start=.1f;Smile.End=1.6f;Smile.Fade=.3f;Smile.Values={{45,.35f},{18,.3f}};StartFace(TEXT("GazeArrival"),{Smile},1.8f,TEXT(""),1);
        }else if(LocoPois.IsValidIndex(LocoHeading)){
            const auto& P=LocoPois[LocoHeading];
            if(LocoPurpose==TEXT("pace")){GazePoi=LocoPaceLook;GazePoiUntil=Now+Rand(2.5f,4.f);GazePoiHead=.7f;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE paced at=%s t=%.2f"),*P.Name.ToString(),Now);LifeTime(A,TEXT("DueTime"),LocoNextDecision);return;}
            LocoAt=LocoHeading;LocoRecent.Add(LocoHeading);if(LocoRecent.Num()>3)LocoRecent.RemoveAt(0);LocoNextPace=Now+Rand(25.f,45.f);
            if(LocoFinal>=0 && LocoFinal!=LocoHeading){   // a pause on the way: look at the pretty thing, then go on
                GazePoi=P.Focus;GazePoiUntil=Now+Rand(2.5f,4.f);GazePoiHead=.7f;LocoNextDecision=Now+Rand(4.f,7.f);LocoDepartStage=1;LocoDepartAt=LocoNextDecision;LocoHeading=LocoFinal;
                UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE pause place=%s next=%s t=%.2f"),*P.Name.ToString(),*LocoPois[LocoFinal].Name.ToString(),Now);
            }else{
                LocoFinal=-1;
                const float Base=Mood==TEXT("sleepy")?Rand(60.f,110.f):Mood==TEXT("happy")?Rand(20.f,45.f):Mood==TEXT("lonely")?Rand(30.f,60.f):Rand(30.f,70.f);
                LocoNextDecision=Now+Base+(P.Station>=0?15.f:0.f);
                LocoArrivalAction=P.Station;LocoActionAt=Now+.8;
                if(P.Station<0 && P.Name!=TEXT("home")){GazePoi=P.Focus;GazePoiUntil=Now+Rand(3.f,5.f);GazePoiHead=.7f;}   // arrived: she looks at what is there
                UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE arrived place=%s station=%d stay=%.0f indoor=%d t=%.2f"),*P.Name.ToString(),P.Station,LocoNextDecision-Now,LocoIndoors(),Now);
            }
        }
        LifeTime(A,TEXT("DueTime"),LocoNextDecision);
    }
    // the place's own behaviour, once she has settled
    if(LocoArrivalAction>=0 && Now>=LocoActionAt && LocoState==ELocoState::Idle && !I->IsAnyMontagePlaying() && LivingBehavior<0 && !ActiveAmbient){
        const int32 V=LocoArrivalAction;bool Done=false;LocoArrivalAction=-1;
        if(V==0 && LivelyOn(TEXT("C10")) && AncientCanopyVisible() && FMath::FRand()<.7f){
            Done=PlayLivelyHead(TEXT("C10"),TEXT("AncientGaze"),true);
            if(Done){LocoArrivalAction=10;LocoActionAt=Now+10;}   // after the sakura look-up, maybe the petals
        }
        if(!Done && V==10 && FMath::FRand()<.6f)Done=StartLiving(0,true);
        else if(!Done && V!=10)Done=StartLiving(V,true);
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE station_action id=%d started=%d t=%.2f"),V,Done,Now);
    }

    // ---------------------------------------------------------------- dwelling
    // the player walking off from close by is remembered for a few seconds (she may be finishing a gesture)
    if(PlayerCharacter && PlayerUp && PDist<520.f && LocoPlayerMovingSince>=0 && Now-LocoPlayerMovingSince>1. && PSpeed>60.f
        && FVector::DotProduct(PlayerCharacter->GetVelocity().GetSafeNormal2D(),(PP-Me).GetSafeNormal2D())>.2f && Now-LocoPlayerLeftAt>12.)LocoPlayerLeftAt=Now;
    const bool Busy=I->IsAnyMontagePlaying() || ActiveAmbient || LivingBehavior>=0 || PoseTransition || IsSeatBusy() || PendingLivelyPat || LivelyResponseActive
        || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction"));
    if(Busy){LocoNextDecision=FMath::Max(LocoNextDecision,Now+3.);LifeTime(A,TEXT("DueTime"),LocoNextDecision);LocoDepartStage=0;return;}
    // coming over to a player who stands still nearby (occasional; mood sets how often)
    if(PlayerCharacter && PlayerUp && Now>=LocoNextApproach && LocoDepartStage==0 && !KeepingCompany()){
        const float Wait=Mood==TEXT("lonely")?14.f:Mood==TEXT("happy")?24.f:Mood==TEXT("sleepy")?90.f:40.f;
        if((LocoForceSocial==1 || (LocoPlayerStillSince>=0 && Now-LocoPlayerStillSince>Wait)) && PDist>320.f && PDist<1400.f && (Bearing || LocoForceSocial==1)){LocoForceSocial=0;
            const float Chance=LocoForceSocial==1?1.f:Mood==TEXT("lonely")?.8f:Mood==TEXT("happy")?.55f:Mood==TEXT("sleepy")?.2f:.4f;
            LocoNextApproach=Now+(Mood==TEXT("lonely")?Rand(60.f,100.f):Mood==TEXT("happy")?Rand(110.f,180.f):Mood==TEXT("sleepy")?Rand(400.f,600.f):Rand(170.f,280.f));
            FVector G;float Y;
            if(FMath::FRand()<Chance && LocoSocialGoal(false,G,Y)){
                GazeDepartLookUntil=Now+1.2;   // she looks at the player first
                if(LocoStartWalk(G,Y,true,TEXT("to_player"),6.f)){LocoSocial=1;LocoRetargetAt=Now+1.;LocoHeading=-1;LocoNextGlance=Now+2.;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE approach distance=%.0f mood=%s t=%.2f"),PDist,*Mood,Now);return;}
            }
        }
    }
    // following a player who walks off from close by (mood)
    if(PlayerCharacter && PlayerUp && Now>=LocoNextFollow && LocoDepartStage==0 && Now-LocoPlayerLeftAt<10. && PDist<1500.f){
        LocoNextFollow=Now+Rand(45.f,80.f);
        const float Chance=LocoForceSocial==2?1.f:Mood==TEXT("lonely")?.85f:Mood==TEXT("happy")?.45f:Mood==TEXT("sleepy")?.1f:.3f;
        FVector G;float Y;const bool Roll=FMath::FRand()<Chance,HaveGoal=Roll && LocoSocialGoal(true,G,Y);
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE follow_check mood=%s roll=%d goal=%d distance=%.0f t=%.2f"),*Mood,Roll,HaveGoal,PDist,Now);
        // set off toward where the player is heading (their trailing spot is still right beside her); updates trail them after
        if(HaveGoal && FVector::Dist2D(G,Me)<260.f){const FVector Ahead=PP+PlayerCharacter->GetVelocity().GetSafeNormal2D()*FMath::Max(150.f,280.f-PDist);FNavLocation Snap;auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(Nav && Nav->ProjectPointToNavigation(Ahead,Snap,FVector(150,150,200)))G=FVector(Snap.Location.X,Snap.Location.Y,Me.Z);}
        if(HaveGoal && LocoStartWalk(G,Y,false,TEXT("follow"),6.f)){
            LocoSocial=2;LocoForceSocial=0;LocoRetargetAt=Now+1.;LocoHeading=-1;LocoNextGlance=Now+2.;
            LocoFollowUntil=Now+(Mood==TEXT("lonely")?Rand(120.f,240.f):Mood==TEXT("happy")?Rand(45.f,80.f):Mood==TEXT("sleepy")?Rand(15.f,25.f):Rand(25.f,50.f));
            UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE follow mood=%s for=%.0f t=%.2f"),*Mood,LocoFollowUntil-Now,Now);return;
        }
    }
    // a few steps during a long stay: she strolls to a spot a metre or two away and faces something new (not at stations)
    const bool Company=KeepingCompany();if(!Company && LocoState==ELocoState::Idle && Now-LocoLastEndAt>20.)LocoCompanyStrolls=0;
    if(LocoDepartStage==0 && Now>=LocoNextPace && (LocoNextDecision-Now>14. || (Company && LocoCompanyStrolls<2)) && (!LocoPois.IsValidIndex(LocoAt) || LocoPois[LocoAt].Station<0)){
        LocoNextPace=Now+(Mood==TEXT("happy")?Rand(15.f,30.f):Mood==TEXT("sleepy")?Rand(50.f,90.f):Mood==TEXT("lonely")?Rand(25.f,40.f):Rand(20.f,35.f));
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Spot;
        const FLocoPoi Here=LocoPois.IsValidIndex(LocoAt)?LocoPois[LocoAt]:FLocoPoi{TEXT("near_player"),Me,0,false,-1,PP+FVector(0,0,150)};
        TArray<FVector> Probe;float Len=1e9f;
        if(Nav && FMath::FRand()<.8f && Nav->GetRandomReachablePointInRadius(Me,240.f,Spot) && FVector::Dist2D(Spot.Location,Me)>120.f && FVector::Dist2D(Spot.Location,Here.Pos)<300.f
            && LocoFindPath(Me,Spot.Location,Probe)){Len=0;for(int32 K=1;K<Probe.Num();++K)Len+=FVector::Dist2D(Probe[K-1],Probe[K]);}
        if(Len<=300.f){   // a few steps only: no detours round flowerbeds
            // face the place's view, or the player when they are around
            FVector Look=Here.Focus;if(Bearing && PDistB<700.f && FMath::FRand()<(Company?.75f:.5f)){FVector V;FRotator R;GetPlayerViewPoint(V,R);Look=V;}
            const FVector G(Spot.Location.X,Spot.Location.Y,Me.Z);const float Y=FMath::RadiansToDegrees(FMath::Atan2(Look.Y-G.Y,Look.X-G.X));
            LocoPaceLook=Look;
            if(LocoStartWalk(G,Y,false,TEXT("pace"),6.f)){if(Company)++LocoCompanyStrolls;LocoHeading=LocoAt;LocoFinal=-1;LocoNextGlance=Now+1.5;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE pace at=%s t=%.2f"),*Here.Name.ToString(),Now);return;}
        }
    }
    // the next walk
    if(Now<LocoNextDecision)return;
    if(LocoDepartStage==0){
        if(KeepingCompany()){LocoNextDecision=Now+Rand(20.f,30.f);LifeTime(A,TEXT("DueTime"),LocoNextDecision);return;}
        const int32 Next=LocoChoose();if(Next<0){LocoNextDecision=Now+20;return;}
        LocoFinal=Next;LocoHeading=Next;
        // sometimes a short pause on the way at a pretty spot she passes anyway
        if(FMath::FRand()<.3f){
            const FVector From=Me,To=LocoPois[Next].Pos;
            for(int32 K=0;K<LocoPois.Num();++K){const auto& P=LocoPois[K];if(K==Next || K==LocoAt || P.Station>=0 || P.Indoor!=LocoPois[Next].Indoor)continue;
                const FVector AB=To-From;const float U=FVector::DotProduct(P.Pos-From,AB)/FMath::Max(1.,AB.SizeSquared());
                if(U<.25f || U>.75f)continue;if(FVector::Dist2D(From+AB*U,P.Pos)>250.f)continue;LocoHeading=K;break;}
        }
        if(Bearing && PDistB<600.f && FMath::Abs(PYaw)<100.f && GazeEnabled){
            GazeDepartLookUntil=Now+1.5;
            FLivelyFacePhase Smile;Smile.Start=.1f;Smile.End=1.3f;Smile.Fade=.25f;Smile.Values={{45,.3f},{18,.3f}};StartFace(TEXT("GazeDepart"),{Smile},1.5f,TEXT(""),1);
            LocoDepartAt=Now+1.5;
        }else LocoDepartAt=Now;
        LocoDepartStage=1;LifeTime(A,TEXT("DueTime"),Now+4);
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE decide next=%s via=%s mood=%s t=%.2f"),*LocoPois[LocoFinal].Name.ToString(),*LocoPois[LocoHeading].Name.ToString(),*Mood,Now);
        return;
    }
    if(LocoDepartStage==1 && Now>=LocoDepartAt){   // a look toward where she is going
        if(!LocoPois.IsValidIndex(LocoHeading)){LocoDepartStage=0;return;}
        GazePoi=LocoPois[LocoHeading].Pos+FVector(0,0,60);GazePoiUntil=Now+1.1;GazePoiHead=.75f;LocoDepartAt=Now+.9;LocoDepartStage=2;return;
    }
    if(LocoDepartStage==2 && Now>=LocoDepartAt){
        LocoDepartStage=0;
        if(!LocoDepartTo(LocoHeading)){LocoNextDecision=Now+Rand(10.f,20.f);LocoRecent.Add(LocoHeading);if(LocoRecent.Num()>3)LocoRecent.RemoveAt(0);LocoFinal=-1;}
    }
}

// Review: pause/resume her own scheduling; walk to a place by index (with its station behaviour on arrival when resumed).
void ACourtyardController::CourtyardReviewLife(int32 On)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    LocoLifePaused=On==0;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE review_paused=%d"),LocoLifePaused);
}
void ACourtyardController::CourtyardReviewGoPlace(int32 Index)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    InitLocoLife();const bool Ok=LocoPois.IsValidIndex(Index) && LocoDepartTo(Index);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE review_go index=%d place=%s ok=%d"),Index,LocoPois.IsValidIndex(Index)?*LocoPois[Index].Name.ToString():TEXT("?"),Ok);
}
void ACourtyardController::CourtyardReviewSocial(int32 Mode)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    LocoForceSocial=Mode;LocoNextApproach=0;LocoNextFollow=0;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE review_force_social=%d"),Mode);
}
