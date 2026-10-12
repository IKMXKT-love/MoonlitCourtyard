#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationBounds.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AlphaBlend.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// Locomotion rework (Claude, 2026-10-06; plan Builds/Review/Locomotion_Plan.html, approved).
// Runtime navigation: the map is not edited. The walkable area is registered from code and RecastNavMesh
// (RuntimeGeneration=Dynamic, agent 24 x 160 cm, Config/DefaultEngine.ini) builds its tiles after load. Ground floor only:
// the walk has no foot placement for stairs, so the box stops below the upper floor.
// Continuous walking: the accepted root-motion walk (AM_Emma_LocoStride) starts her, its cruise is looped one 1.6 s cycle at a
// time (identical poses at 2.0 and 3.6 s) for any distance, and its own stop part ends the walk. The in-place loop
// (AM_Emma_LocoLoop) is kept only as a fallback asset.

namespace {
// UNavigationSystemV1 keeps its bounds API protected (normally a placed NavMeshBoundsVolume calls it).
struct FCourtyardNavAccess : public UNavigationSystemV1 {
    static void AddBounds(UNavigationSystemV1* Nav,const FBox& Box,ULevel* Level){
        FNavigationBoundsUpdateRequest R;R.NavBounds.UniqueID=0x4C4F434F;R.NavBounds.AreaBox=Box;R.NavBounds.Level=Level;
        R.UpdateRequest=FNavigationBoundsUpdateRequest::Added;
        TArray<FNavigationBoundsUpdateRequest> Requests;Requests.Add(R);
        (Nav->*(&FCourtyardNavAccess::PerformNavigationBoundsUpdate))(Requests);
        (Nav->*(&FCourtyardNavAccess::SpawnMissingNavigationData))();
    }
};
const FBox LocoArea(FVector(-1450,-3200,-120),FVector(1450,1450,250));
// Flowerbeds, shrubs, foliage, the fountain and decor without collision, from the real ground footprints (Tools/make_loco_nowalk.py).
const float NoWalk[][4]={
#include "LocoNoWalkZones.inl"
};
// Forward root travel of AM_Emma_LocoStride every 0.1 s (Audit/Locomotion20261006/turn_probe.json). The in-place loop is this
// clip's cruise: stride 2.0 s == loop 1.0 s with 0.0 cm leg-pose error (foot_probe.json); period 1.6 s, 47.8 cm/s.
const float StrideRoot[71]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.1f,0.8f,2.4f,5.0f,8.5f,12.6f,17.2f,22.0f,26.8f,31.6f,36.3f,41.1f,45.9f,50.7f,55.5f,60.3f,65.1f,69.8f,74.6f,79.4f,84.2f,89.0f,93.8f,98.6f,103.3f,108.1f,112.9f,117.7f,122.5f,127.3f,132.1f,136.9f,141.6f,146.4f,151.2f,156.0f,160.8f,165.6f,170.4f,175.1f,179.9f,184.6f,188.5f,191.3f,192.9f,193.6f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f,193.7f};
constexpr float LoopLen=1.6f,StartAt=1.f,CruiseFrom=2.f,LastEntry=5.f,StrideStill=5.75f,Cruise=47.8f;
float StrideAt(float T){T=FMath::Clamp(T,0.f,6.99f);const int32 I=FMath::FloorToInt(T*10);return FMath::Lerp(StrideRoot[I],StrideRoot[I+1],T*10-I);}
float StrideLeft(float T){return StrideRoot[70]-StrideAt(T);}
float StrideFor(float Phase,int32 K){return CruiseFrom+FMath::Fmod(Phase-1.f+LoopLen*8,LoopLen)+LoopLen*K;}
void SetFlag(AActor* A,FName N,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,V);}
float YawTo(const FVector& From,const FVector& To){return FMath::RadiansToDegrees(FMath::Atan2(To.Y-From.Y,To.X-From.X));}
}

void ACourtyardController::InitLocomotionNav()
{
    if(LocoNavRequested)return;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav){UE_LOG(LogTemp,Warning,TEXT("COURTYARD_LOCO nav_missing"));LocoNavRequested=true;return;}
    // Candidate levels are now saved under Content28/Content29, not ContentFix.
    // Read the placed coordinator even if its BeginPlay has not assigned ContentLayer yet.
    // A renamed level must retain the same floor coverage; legacy maps stay ground-only.
    bool ContentFloors=false;
    for(TActorIterator<ACourtyardContentLife> It(GetWorld());It;++It){
        if(It->Enabled && It->FixEnabled){ContentFloors=true;break;}
    }
    const FBox Area=ContentFloors?FBox(FVector(-1450,-3200,-120),FVector(1450,2160,680)):LocoArea;
    FCourtyardNavAccess::AddBounds(Nav,Area,GetWorld()->PersistentLevel);
    LocoAddNoWalkZones();
    LocoNavRequested=true;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO nav_bounds_added box=%s navdata=%s t=%.2f"),*Area.ToString(),*GetNameSafe(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)),GetWorld()->GetTimeSeconds());
}

// One actor holds query-only boxes that ignore every channel (nothing else in the game is affected and they are not
// navigation geometry) plus a NavModifier that turns each box into a Null area.
void ACourtyardController::LocoAddNoWalkZones()
{
    if(LocoNoWalk)return;
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Z=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);if(!Z)return;
    auto* Root=NewObject<USceneComponent>(Z,TEXT("Root"));Z->SetRootComponent(Root);Root->RegisterComponent();
    int32 N=0;
    for(const auto& B:NoWalk){
        auto* Box=NewObject<UBoxComponent>(Z);Box->SetupAttachment(Root);Box->SetBoxExtent(FVector(B[2],B[3],80));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetGenerateOverlapEvents(false);
        Box->SetHiddenInGame(true);Box->SetRelativeLocation(FVector(B[0],B[1],40));Box->RegisterComponent();++N;
    }
    auto* Mod=NewObject<UNavModifierComponent>(Z,TEXT("NoWalk"));Mod->SetAreaClass(UNavArea_Null::StaticClass());Mod->RegisterComponent();Mod->RefreshNavigationModifiers();
    LocoNoWalk=Z;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO nowalk_boxes=%d"),N);
}

bool ACourtyardController::LocoFindPath(const FVector& From,const FVector& To,TArray<FVector>& Out) const
{
    Out.Reset();
    UNavigationPath* P=UNavigationSystemV1::FindPathToLocationSynchronously(const_cast<ACourtyardController*>(this),From,To,Emma());
    if(!P || !P->IsValid() || P->IsPartial())return false;
    Out=P->PathPoints;
    if(ContentLayer.IsValid() && ContentLayer->FixEnabled){
        // The retained flat walk must never flatten a route over the stairs.
        // Only the measured stair coordinator can change floors.
        if(FMath::Abs(From.Z-To.Z)>35)return false;
        for(int32 K=0;K<Out.Num();++K){
            FVector& V=Out[K];if(FMath::Abs(V.Z-(From.Z-80))<=35)continue;
            // Recast's first/last vertex straddles the final stair raster cell.
            // Only the measured upper landing may use its real support plane;
            // interior vertices and all other floor changes remain forbidden.
            const FVector End=K==0?From:To;
            if((K!=0 && K!=Out.Num()-1) || End.Z<520 || End.Z>535 ||
                End.X<-135 || End.X>-65 || End.Y<1500 || End.Y>1590 || FVector::Dist2D(V,End)>8)return false;
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(ContentLandingSupport),false,Emma());
            if(!GetWorld()->LineTraceSingleByChannel(H,End-FVector(0,0,65),End-FVector(0,0,98),ECC_Pawn,Q) ||
                FMath::Abs(H.ImpactPoint.Z-(From.Z-80))>5 || H.ImpactNormal.Z<.95f)return false;
            V.Z=H.ImpactPoint.Z;
        }
    }
    return Out.Num()>=2;
}

// Review: 0 = status, 1 = dump the generated walkable surface + path tests to Audit/Locomotion20261006/navmesh.json.
void ACourtyardController::CourtyardReviewNav(int32 Mode)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    InitLocomotionNav();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    auto* Recast=Nav?Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)):nullptr;
    const int32 Tiles=Recast?Recast->GetNavMeshTilesCount():0;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO nav_status navdata=%s tiles=%d building=%d t=%.2f"),*GetNameSafe(Recast),Tiles,Nav?Nav->IsNavigationBuildInProgress():-1,GetWorld()->GetTimeSeconds());
    if(Mode!=1 || !Recast)return;
    TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Tris;
    for(int32 T=0;T<Tiles;++T){
        FRecastDebugGeometry G;if(!Recast->GetDebugGeometryForTile(G,T))continue;
        for(int32 Area=0;Area<RECAST_MAX_AREAS;++Area)for(int32 I=0;I+2<G.AreaIndices[Area].Num();I+=3){
            TArray<TSharedPtr<FJsonValue>> Tri;
            for(int32 K=0;K<3;++K){const FVector& V=G.MeshVerts[G.AreaIndices[Area][I+K]];Tri.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(V.X)));Tri.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(V.Y)));Tri.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(V.Z)));}
            Tri.Add(MakeShared<FJsonValueNumber>(Area));Tris.Add(MakeShared<FJsonValueArray>(Tri));
        }
    }
    Root->SetArrayField(TEXT("tris"),Tris);
    // Path tests between today's stops (home, chime, ancient sakura, reading corner) and a few open places.
    const FVector Home(-195,0,82.4);
    TArray<TPair<FString,FVector>> Goals={{TEXT("chime"),FVector(46.4,132,82.4)},{TEXT("ancient"),FVector(-401.8,-2193,82.4)},{TEXT("reading"),FVector(-799.5,870.7,82.4)},
        {TEXT("fountain_north"),FVector(0,-1000,82.4)},{TEXT("bench_south"),FVector(0,-2700,82.4)},{TEXT("living_room"),FVector(250,800,82.4)}};
    TArray<TSharedPtr<FJsonValue>> Paths;
    for(const auto& G:Goals){
        TArray<FVector> Pts;const bool Ok=LocoFindPath(Home,G.Value,Pts);float Len=0;for(int32 I=1;I<Pts.Num();++I)Len+=FVector::Dist(Pts[I-1],Pts[I]);
        TSharedRef<FJsonObject> O=MakeShared<FJsonObject>();O->SetStringField(TEXT("goal"),G.Key);O->SetBoolField(TEXT("ok"),Ok);O->SetNumberField(TEXT("length"),Len);
        TArray<TSharedPtr<FJsonValue>> P;for(const FVector& V:Pts){P.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(V.X)));P.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(V.Y)));P.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(V.Z)));}
        O->SetArrayField(TEXT("points"),P);Paths.Add(MakeShared<FJsonValueObject>(O));
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO path_test goal=%s ok=%d points=%d length=%.0f"),*G.Key,Ok,Pts.Num(),Len);
    }
    Root->SetArrayField(TEXT("paths"),Paths);
    FString Text;auto W=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(Root,W);
    FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Audit/Locomotion20261006/navmesh.json")));
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO nav_dump tris=%d"),Tris.Num());
}

void ACourtyardController::CourtyardReviewWalkTo(float X,float Y,float Yaw)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    auto* A=Emma();if(!A)return;
    const bool Ok=LocoStartWalk(FVector(X,Y,A->GetActorLocation().Z),Yaw,Yaw>-900,TEXT("review"),12);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO review_walk x=%.0f y=%.0f yaw=%.0f ok=%d"),X,Y,Yaw,Ok);
}

FVector ACourtyardController::LocoPointAt(float Dist) const
{
    if(LocoPath.Num()==0)return LocoGoal;
    if(Dist<=0)return LocoPath[0];
    for(int32 I=1;I<LocoPath.Num();++I)if(LocoPathDist[I]>=Dist){
        const float Seg=LocoPathDist[I]-LocoPathDist[I-1];return FMath::Lerp(LocoPath[I-1],LocoPath[I],Seg>1e-3f?(Dist-LocoPathDist[I-1])/Seg:1.f);
    }
    return LocoPath.Last();
}
float ACourtyardController::LocoProject(const FVector& P) const
{
    float Best=LocoProgress,BestD=1e18f;
    for(int32 I=1;I<LocoPath.Num();++I){
        if(LocoPathDist[I]<LocoProgress-30.f || LocoPathDist[I-1]>LocoProgress+250.f)continue;   // local window: never jump across the path
        const FVector A=LocoPath[I-1],B=LocoPath[I];const FVector AB=(B-A)*FVector(1,1,0);const float L2=AB.SizeSquared();
        const float U=L2>1e-3f?FMath::Clamp(FVector::DotProduct((P-A)*FVector(1,1,0),AB)/L2,0.f,1.f):0.f;
        const float D=FVector::DistSquared2D(A+(B-A)*U,P);
        if(D<BestD){BestD=D;Best=LocoPathDist[I-1]+(LocoPathDist[I]-LocoPathDist[I-1])*U;}
    }
    return FMath::Max(Best,LocoProgress);
}

// Walk to Goal (optionally arriving facing Yaw, placed within Tolerance cm). Corners of the navigation path become arcs.
bool ACourtyardController::LocoStartWalk(const FVector& Goal,float Yaw,bool HasYaw,FName Purpose,float Tolerance)
{
    auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
    if(!A || !I || !LocoLoop || !LocoStride || LocoState!=ELocoState::Idle || I->IsAnyMontagePlaying())return false;
    const FVector From=A->GetActorLocation();
    // Arrive along the goal's facing: aim the path at a point behind the goal, then finish straight. The stop part needs
    // ~85 cm of straight path, so the run-in is 170 cm where the ground allows (else 120 / 70 / none).
    TArray<FVector> Raw;bool Found=false;
    for(const float Run:{170.f,120.f,70.f,0.f}){
        if(!HasYaw && Run>0)continue;
        const FVector Pre=Goal-FRotator(0,Yaw,0).Vector()*Run;
        if(Run>0){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(LocoRunIn),false,A);
            if(GetWorld()->SweepSingleByChannel(H,Pre,Goal,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(22.f),Q))continue;}
        if(LocoFindPath(From,Pre,Raw)){Found=true;if(Run>0)Raw.Add(Goal);break;}
    }
    if(!Found){UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO no_path purpose=%s goal=%s"),*Purpose.ToString(),*Goal.ToString());return false;}
    for(auto& P:Raw)P.Z=From.Z;
    LocoPath.Reset();LocoPath.Add(From);
    for(int32 K=1;K<Raw.Num()-1;++K){
        const FVector P0=LocoPath.Last(),C=Raw[K],P1=Raw[K+1];
        const float R=FMath::Min3(70.f,float(FVector::Dist2D(P0,C))*.45f,float(FVector::Dist2D(C,P1))*.45f);
        const FVector In=C+(P0-C).GetSafeNormal2D()*R,Out=C+(P1-C).GetSafeNormal2D()*R;
        for(int32 S=0;S<=6;++S){const float U=S/6.f;LocoPath.Add(FMath::Lerp(FMath::Lerp(In,C,U),FMath::Lerp(C,Out,U),U));}
    }
    LocoPath.Add(Raw.Last());
    LocoPathDist.Reset();LocoPathDist.Add(0);
    for(int32 K=1;K<LocoPath.Num();++K)LocoPathDist.Add(LocoPathDist.Last()+FVector::Dist2D(LocoPath[K-1],LocoPath[K]));
    const float Total=LocoPathDist.Last(),Whole=StrideLeft(StartAt);
    // New opt-in flower dock reuses the existing stride-length planner. No old
    // route/clip changes and no stationary final alignment (tolerance is0).
    ContentPreciseWalk=ContentLayer.IsValid() && ContentLayer->FixEnabled && (Purpose.ToString().StartsWith(TEXT("Floor")) || (ContentLayer->FlowerScentEnabled && Purpose==TEXT("FlowerScentVisit")));
    if(ContentPreciseWalk){
        float Best=1e10f;ContentWalkCycles=-1;
        for(int32 Cycles=0;Cycles<80;++Cycles){
            const float Pre=StrideAt(3.6f)-StrideAt(StartAt)+Cycles*Cruise*LoopLen;
            for(int32 K=0;K<=60;++K){
                const float Stop=.85f+.005f*K,Scale=(Total-Stop*StrideLeft(3.6f))/Pre;
                if(Scale<.92f || Scale>1.08f)continue;
                const float Cost=FMath::Abs(Scale-1)+.3f*FMath::Abs(Stop-1);
                if(Cost<Best){Best=Cost;ContentWalkCycles=Cycles;ContentWalkScale=Scale;}
            }
        }
        if(ContentWalkCycles<0){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_stride_unreachable length=%.2f"),Total);return false;}
        UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_stride_plan cycles=%d cruise_scale=%.4f total=%.2f"),ContentWalkCycles,ContentWalkScale,Total);
    }
    if(Total<Whole*.55f){UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO too_short length=%.0f"),Total);return false;}
    LocoGoal=Goal;LocoGoalYaw=Yaw;LocoGoalHasYaw=HasYaw;LocoGoalTolerance=Tolerance;LocoPurpose=Purpose;LocoProgress=0;LocoTrim=0;LocoRate=1;++LocoTrip;LocoStopRequested=false;LocoLastArrived=false;
    LocoSavedControllerYaw=A->bUseControllerRotationYaw;A->bUseControllerRotationYaw=false;
    auto* Move=A->GetCharacterMovement();LocoSavedSpeed=Move->MaxWalkSpeed;LocoSavedAccel=Move->MaxAcceleration;LocoSavedBraking=Move->BrakingDecelerationWalking;
    Move->MaxWalkSpeed=150;Move->MaxAcceleration=2000;Move->BrakingDecelerationWalking=2000;
    SetFlag(A,TEXT("TravelActive"),true);SetFlag(A,TEXT("AllowInteraction"),false);SetFlag(A,TEXT("ActivityEnabled"),false);
    // a path shorter than one full stride still uses the stride, its root motion scaled to the length
    A->SetAnimRootMotionTranslationScale(ContentPreciseWalk?ContentWalkScale:Total<Whole?FMath::Clamp(Total/Whole,.55f,1.f):1.f);
    I->Montage_PlayWithBlendIn(LocoStride,FAlphaBlendArgs(.25f),1.f,EMontagePlayReturnType::MontageLength,StartAt,false);
    LocoState=ELocoState::Start;LocoStateSince=GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO start trip=%d purpose=%s length=%.0f points=%d goal=%s yaw=%.0f has_yaw=%d mood_rate=%.2f t=%.2f"),LocoTrip,*Purpose.ToString(),Total,LocoPath.Num(),*Goal.ToString(),Yaw,HasYaw,LocoMoodRate,LocoStateSince);
    return true;
}

void ACourtyardController::LocoEnd(bool Arrived,const TCHAR* Why)
{
    auto* A=Cast<ACharacter>(Emma());if(!A){LocoState=ELocoState::Idle;return;}
    auto* I=A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
    if(I){if(I->Montage_IsPlaying(LocoLoop))I->Montage_StopWithBlendOut(FAlphaBlendArgs(.35f),LocoLoop);if(I->Montage_IsPlaying(LocoStride))I->Montage_StopWithBlendOut(FAlphaBlendArgs(.35f),LocoStride);}
    auto* Move=A->GetCharacterMovement();Move->StopMovementImmediately();Move->MaxWalkSpeed=LocoSavedSpeed;Move->MaxAcceleration=LocoSavedAccel;Move->BrakingDecelerationWalking=LocoSavedBraking;
    A->SetAnimRootMotionTranslationScale(1);A->bUseControllerRotationYaw=LocoSavedControllerYaw;
    if(auto* C=A->GetController())C->SetControlRotation(A->GetActorRotation());
    SetFlag(A,TEXT("TravelActive"),false);SetFlag(A,TEXT("AllowInteraction"),true);
    const float Err=FVector::Dist2D(A->GetActorLocation(),LocoGoal),YawErr=LocoGoalHasYaw?FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,LocoGoalYaw)):0;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO end trip=%d arrived=%d why=%s err_cm=%.1f yaw_err=%.1f pos=%s yaw=%.1f t=%.2f"),LocoTrip,Arrived,Why,Err,YawErr,*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw,GetWorld()->GetTimeSeconds());
    LocoLastEndAt=GetWorld()->GetTimeSeconds();LocoEndArrived=Arrived && !LocoStopRequested && (!ContentPreciseWalk || (Err<=6 && YawErr<=5));LocoStopRequested=false;
    LocoState=ELocoState::Idle;LocoPath.Reset();
}

void ACourtyardController::TickLocomotion(float Dt)
{
    TickLocoLife(Dt);
    if(LocoState==ELocoState::Idle || Dt<=0)return;
    auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
    if(!A || !I){LocoState=ELocoState::Idle;return;}
    const double Now=GetWorld()->GetTimeSeconds();
    const FVector Pos=A->GetActorLocation();
    if(ContentPreciseWalk && LocoPath.Num() && FMath::Abs(Pos.Z-LocoPath[0].Z)>10.f){LocoEnd(false,TEXT("floor_height_guard"));return;}
    LocoProgress=LocoProject(Pos);
    const float Total=LocoPathDist.Num()?LocoPathDist.Last():0,Remaining=FMath::Max(0.f,Total-LocoProgress);
    // shorter look-ahead in tight curves so she follows the arc instead of cutting the corner
    const FVector T0=(LocoPointAt(LocoProgress+10.f)-LocoPointAt(LocoProgress)).GetSafeNormal2D(),T1=(LocoPointAt(LocoProgress+60.f)-LocoPointAt(LocoProgress+50.f)).GetSafeNormal2D();
    const float Look=FVector::DotProduct(T0,T1)<.7f?28.f:55.f;
    FVector Ahead=LocoPointAt(LocoProgress+FMath::Clamp(Remaining,15.f,Look));
    // local avoidance: a player standing near the path ahead is passed with a sideways margin (navigation ignores pawns)
    if(PlayerCharacter && Remaining>60){
        const FVector P=PlayerCharacter->GetActorLocation();
        for(float S=0;S<=160.f;S+=20.f){
            const FVector C=LocoPointAt(LocoProgress+S),N=(LocoPointAt(LocoProgress+S+10.f)-C).GetSafeNormal2D();
            const FVector Side(-N.Y,N.X,0);const float Lat=FVector::DotProduct(P-C,Side),Along=FVector::DotProduct(P-C,N);
            if(FMath::Abs(Along)<35.f && FMath::Abs(Lat)<75.f){Ahead+=Side*(Lat>=0?-1.f:1.f)*(75.f-FMath::Abs(Lat));break;}
        }
    }
    float Want=Remaining>8?YawTo(Pos,Ahead):A->GetActorRotation().Yaw;
    // the end of the walk: aim at the spot itself, and only in the last steps turn to the place's facing
    if(Remaining<120 && Remaining>=25)Want=YawTo(Pos,LocoGoal);
    if(LocoGoalHasYaw && Remaining<25)Want=LocoGoalYaw;
    const float Yaw=A->GetActorRotation().Yaw,Err=FMath::FindDeltaAngleDegrees(Yaw,Want);
    auto Steer=[&](float MaxDegPerSec){const float Step=FMath::Clamp(Err,-MaxDegPerSec*Dt,MaxDegPerSec*Dt);A->SetActorRotation(FRotator(0,Yaw+Step,0));};
    float Phase=0;
    switch(LocoState){
    case ELocoState::Start:
    case ELocoState::Cruise:{
        // Everything is the stride clip's own root motion: the start part (1.0-2.0 s), then its cruise looped by jumping the
        // playhead back one cycle (3.6 -> 2.0 s, identical poses), then its stop part. Speed = play rate (cadence and travel
        // together, no slide); steering = facing; the stride-length correction = root-motion scale.
        if(!I->Montage_IsPlaying(LocoStride)){LocoEnd(false,TEXT("walk_interrupted"));return;}
        float T=I->Montage_GetPosition(LocoStride);
        const float Speed=A->GetVelocity().Size2D();
        // turning: quick while still slow (stepping round), gentle at walking speed
        Steer(FMath::Lerp(130.f,62.f,FMath::Clamp(Speed/Cruise,0.f,1.f))*FMath::Max(LocoRate,.8f));
        if(LocoState==ELocoState::Start){
            if(!ContentPreciseWalk && Total<StrideLeft(StartAt)){if(T>=StrideStill){LocoState=ELocoState::Settle;LocoStateSince=Now;LocoSettleFrom=Pos;LocoSettleYaw0=A->GetActorRotation().Yaw;}break;}
            if(T<CruiseFrom)break;
            LocoState=ELocoState::Cruise;LocoStateSince=Now;
            UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO cruise trip=%d remaining=%.0f t=%.2f"),LocoTrip,Remaining,Now);
        }
        Phase=1.f+(T-CruiseFrom);
        const float Turn=FMath::Clamp(1.f-FMath::Abs(Err)/150.f,.72f,1.f);
        LocoRate=FMath::FInterpTo(LocoRate,LocoMoodRate*Turn,Dt,1.6f);I->Montage_SetPlayRate(LocoStride,LocoRate);
        // Lattice: at constant stride a walk can only end 85.6 cm (the stop part from 3.6 s) after a cycle boundary, i.e. on
        // a 76.5 cm lattice. From ~4.5 m out the stride is lengthened/shortened (<= 8%) so the lattice lands on the goal.
        const float ToEdge=FMath::Max(0.f,3.6f-T)*Cruise*(1.f+LocoTrim);
        const float Lattice0=ToEdge+StrideLeft(3.6f);
        const float Cycles=FMath::Max(0.f,FMath::RoundToFloat((Remaining-Lattice0)/(Cruise*LoopLen)));
        const float Eps=Remaining-(Lattice0+Cruise*LoopLen*Cycles);
        LocoTrim=Remaining<450.f?FMath::Clamp(Eps/FMath::Max(Remaining-StrideLeft(3.6f),60.f),-.08f,.08f):0.f;
        if(ContentPreciseWalk && !LocoStopRequested){
            const float CruiseLeft=FMath::Max(0.f,StrideAt(3.6f)-StrideAt(T))+ContentWalkCycles*Cruise*LoopLen;
            if(CruiseLeft>8){
                // Curved travel loses path progress compared with straight root distance.
                // Correct during walking within the existing 8% range, before stopping.
                const float Desired=FMath::Clamp((Remaining-StrideLeft(3.6f))/CruiseLeft,.92f,1.08f);
                ContentWalkScale=FMath::FInterpTo(ContentWalkScale,Desired,Dt,2.f);
            }
        }
        A->SetAnimRootMotionTranslationScale(ContentPreciseWalk?ContentWalkScale:1.f+LocoTrim);
        if(T>=3.6f){
            float S=Remaining/StrideLeft(ContentPreciseWalk?T:3.6f);
            if(ContentPreciseWalk && (LocoPurpose==TEXT("FloorReadingApproach") || (ContentLayer.IsValid() && ContentLayer->FlowerScentEnabled && LocoPurpose==TEXT("FlowerScentVisit"))) && Remaining<120.f){
                // The chair's curved approach can project ahead of the real position.
                // At the final straight stop, use the measured distance to the chair
                // entry, not progress around a nearby curve. No endpoint relocation.
                const float Direct=FVector::Dist2D(Pos,LocoGoal);
                const FVector Direction=(LocoGoal-Pos).GetSafeNormal2D();
                if(FVector::DotProduct(A->GetActorForwardVector(),Direction)>.96f){
                    UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX reading_stop_distance path=%.3f direct=%.3f"),Remaining,Direct);
                    S=Direct/StrideLeft(T);
                }
            }
            if(LocoStopRequested && S>1.15f){   // asked to stop early (a player in the way, a follow that ends): stop here, short of them
                const float Room=PlayerCharacter?float(FVector::Dist2D(PlayerCharacter->GetActorLocation(),Pos))-80.f:StrideLeft(3.6f);
                S=FMath::Clamp(Room/StrideLeft(3.6f),.55f,1.f);
            }
            if(ContentPreciseWalk?(ContentWalkCycles<=0 || LocoStopRequested):S<=1.15f){   // this boundary is the stop: let the clip run on into its own stop part
                LocoStopScale=FMath::Clamp(S,.85f,1.15f);A->SetAnimRootMotionTranslationScale(LocoStopScale);
                LocoStopEntry=T;LocoState=ELocoState::Stop;LocoStateSince=Now;LocoStopYaw0=Yaw;LocoStopYaw1=LocoGoalHasYaw?LocoGoalYaw:Want;
                UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO stop trip=%d t=%.3f stop_dist=%.1f remaining=%.1f scale=%.3f rate=%.2f t=%.2f"),LocoTrip,T,StrideLeft(3.6f),Remaining,S,LocoRate,Now);
                break;
            }
            if(ContentPreciseWalk)--ContentWalkCycles;
            I->Montage_SetPosition(LocoStride,T-LoopLen);T-=LoopLen;
        }
        break;}
    case ELocoState::Stop:{
        const float T=I->Montage_IsPlaying(LocoStride)?I->Montage_GetPosition(LocoStride):StrideStill;
        // keep following the path while slowing down (root motion follows the facing); the arrival facing comes in at the end
        const float U=FMath::Clamp((T-LocoStopEntry)/FMath::Max(StrideStill-LocoStopEntry,.3f),0.f,1.f);
        Steer(62.f);
        if(T>=StrideStill){LocoState=ELocoState::Settle;LocoStateSince=Now;LocoSettleFrom=Pos;LocoSettleYaw0=A->GetActorRotation().Yaw;}
        break;}
    case ELocoState::Settle:{
        // tiny final correction for stations that need the exact spot/facing (only within tolerance; never a visible slide)
        const float U=FMath::Clamp(float((Now-LocoStateSince)/.4),0.f,1.f),S=FMath::SmoothStep(0.f,1.f,U);
        if(LocoGoalHasYaw && FVector::Dist2D(LocoSettleFrom,LocoGoal)<=LocoGoalTolerance){
            A->SetActorLocation(FMath::Lerp(LocoSettleFrom,FVector(LocoGoal.X,LocoGoal.Y,LocoSettleFrom.Z),S));
            const float DY=FMath::FindDeltaAngleDegrees(LocoSettleYaw0,LocoGoalYaw);if(FMath::Abs(DY)<12.f)A->SetActorRotation(FRotator(0,LocoSettleYaw0+DY*S,0));
        }
        if(U>=1.f){LocoEnd(true,TEXT("arrived"));return;}
        break;}
    default:break;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) && Now>=LocoSampleAt){
        LocoSampleAt=Now+.1;auto* M=A->GetMesh();const FVector L=M->GetSocketLocation(TEXT("足首_L")),R=M->GetSocketLocation(TEXT("足首_R"));
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LOCO_SAMPLE %.3f,%d,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f"),Now,(int32)LocoState,Pos.X,Pos.Y,A->GetActorRotation().Yaw,A->GetVelocity().Size2D(),LocoRate,Phase,Remaining,L.X,L.Y,L.Z,R.X,R.Y,R.Z,LocoTrim);
    }
}
