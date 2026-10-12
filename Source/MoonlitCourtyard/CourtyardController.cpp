#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "HAL/IConsoleManager.h"
#include "CourtyardHUD.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Guid.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    float HalfHeight(ECourtyardPose P) { return P==ECourtyardPose::Standing ? 80.f : P==ECourtyardPose::Crouched ? 54.f : 24.f; }
    bool Low(ECourtyardPose P) { return P==ECourtyardPose::Prone || P==ECourtyardPose::Supine; }
    void WriteBool(UObject* O,FName N,bool V) { if(O) if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N)) P->SetPropertyValue_InContainer(O,V); }
    double ReadNumber(UObject* O,FName N,double Fallback=0) {if(O) if(auto* P=FindFProperty<FNumericProperty>(O->GetClass(),N)) if(P->IsFloatingPoint())return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(O));return Fallback;}
}

ACourtyardController::ACourtyardController()
{
    PrimaryActorTick.bCanEverTick=true;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Response(TEXT("/Game/Emma/Animations/A_Emma_HeadPatAttentive_Review"));
    AttentiveResponse=Response.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Bright(TEXT("/Game/Emma/Animations/A_Emma_HeadPatBright_Review"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Tender(TEXT("/Game/Emma/Animations/A_Emma_HeadPatTender_Review"));
    BrightResponse=Bright.Object; TenderResponse=Tender.Object;
    static ConstructorHelpers::FObjectFinder<UAnimMontage> Sky(TEXT("/Game/Emma/Animations/AM_Emma_SkyWatch_Review"));
    static ConstructorHelpers::FObjectFinder<UAnimMontage> Garden(TEXT("/Game/Emma/Animations/AM_Emma_GardenLook_Review"));
    static ConstructorHelpers::FObjectFinder<UAnimMontage> Enjoy(TEXT("/Game/Emma/Animations/AM_Emma_QuietEnjoy_Review"));
    SkyWatch=Sky.Object; GardenLook=Garden.Object; QuietEnjoy=Enjoy.Object;
    const TCHAR* Names[]={TEXT("MeteorNotice"),TEXT("FlowerCenter"),TEXT("QuietEnjoy"),TEXT("FlowerLeft"),TEXT("FlowerRight"),TEXT("CompanionLeft"),TEXT("CompanionRight"),TEXT("ThankfulGreeting"),TEXT("CompanionCenter")};
    for(const TCHAR* N:Names) {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/AM_Emma_"))+N+TEXT("_Review"))); ContextMotions.Add(Asset.Object); }
    const TCHAR* BodyNames[]={TEXT("Wave"),TEXT("Stretch"),TEXT("Yawn"),TEXT("Read"),TEXT("Tea"),TEXT("Flower"),TEXT("FoldArms"),TEXT("Hair"),TEXT("Clap"),TEXT("Bow")};
    for(const TCHAR* N:BodyNames){const FString Name(N);if(Name==TEXT("Bow")){BodyMotions.Add(nullptr);continue;}const bool Deferred=Name==TEXT("Read") || Name==TEXT("Tea") || Name==TEXT("Flower");const bool Repaired=Name==TEXT("Yawn") || Name==TEXT("Hair") || Name==TEXT("Clap");FString Path=Name==TEXT("Stretch")?TEXT("/Game/Emma/Animations/GestureExpressive20261001/AM_Emma_BodyStretch_Intent1"):Name==TEXT("FoldArms")?TEXT("/Game/Emma/Animations/GestureExpressive20261001/AM_Emma_BodyFoldArms_Intent1"):Name==TEXT("Hair")?TEXT("/Game/Emma/Animations/GestureFeedback20261001/AM_Emma_BodyHair_Clearance1"):Repaired?(FString(TEXT("/Game/Emma/Animations/GestureRepair20261001/AM_Emma_Body"))+N+TEXT("_Intent2")):Deferred?(FString(TEXT("/Game/Emma/Animations/AM_Emma_Body"))+N+TEXT("_Review")):(FString(TEXT("/Game/Emma/Animations/Natural20260927/AM_Emma_Body"))+N+(Name==TEXT("Clap")?TEXT("_ClapPalmFinal"):(Name==TEXT("FoldArms"))?TEXT("_ContactFinal"):((Name==TEXT("Wave") || Name==TEXT("Hair"))?TEXT("_ApproachFinal"):TEXT("_Natural2"))));
        // The front wave lowers in one coordinated sweep (WelcomeRestore 2026-10-04), like the accepted turning wave v3.
        if(Name==TEXT("Wave"))Path=TEXT("/Game/Emma/Animations/WelcomeRestore20261004/AM_Emma_BodyWave_FrontSweep");
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*Path);BodyMotions.Add(Asset.Object);}
    for(const TCHAR* N:{TEXT("CatchPetal"),TEXT("Chime"),TEXT("TraceStars"),TEXT("HighFive"),TEXT("BlowFeather"),TEXT("BookmarkRead"),TEXT("HighFiveReply")}) {
        const FString Suffix=(FString(N)==TEXT("CatchPetal") || FString(N)==TEXT("BlowFeather"))?TEXT("_Contact2"):(FString(N)==TEXT("BookmarkRead") || FString(N)==TEXT("Chime") || FString(N)==TEXT("TraceStars"))?TEXT("_Station2"):TEXT("_Candidate1");
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/LivingSix20261001/AM_Emma_"))+N+Suffix));LivingMotions.Add(Asset.Object);
    }
    for(const TCHAR* N:{TEXT("Petal"),TEXT("Feather"),TEXT("Book"),TEXT("Bookmark")}){ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(*(FString(TEXT("/Game/Environment/LivingSix20261001/SM_Living"))+N));LivingMeshes.Add(Asset.Object);}
    for(int32 N=0;N<3;++N){ConstructorHelpers::FObjectFinder<USoundBase> Asset(*FString::Printf(TEXT("/Game/Environment/LivingSix20261001/S_Chime%d"),N));ChimeSounds.Add(Asset.Object);}
    {ConstructorHelpers::FObjectFinder<USoundAttenuation> Asset(TEXT("/Game/Environment/LivingSix20261001/SA_Chime"));ChimeAttenuation=Asset.Object;}
    for(const TCHAR* N:{TEXT("AM_Emma_WalkIdleSlotReview"),TEXT("AM_Emma_WalkArc60_Review"),TEXT("AM_Emma_CompactTurn_Review")}){ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/"))+N));ReadingWalkMotions.Add(Asset.Object);}
    // Locomotion rework (2026-10-06): continuous walking from the accepted gait (Locomotion20261006).
    {ConstructorHelpers::FObjectFinder<UAnimMontage> L(TEXT("/Game/Emma/Animations/Locomotion20261006/AM_Emma_LocoLoop"));LocoLoop=L.Object;}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> S(TEXT("/Game/Emma/Animations/Locomotion20261006/AM_Emma_LocoStride"));LocoStride=S.Object;}
    for(const TCHAR* N:{TEXT("Stand"),TEXT("Book"),TEXT("Cup"),TEXT("Flower")}){ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(*(FString(TEXT("/Game/Environment/BodyProps/SM_Life"))+N));BodyPropMeshes.Add(Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Welcome20261003/AM_Emma_WelcomeTurnWave3"));WelcomeMotion=Asset.Object;}
    for(const TCHAR* N:{TEXT("Face"),TEXT("Face_NoneEdge"),TEXT("Face_Eyelid"),TEXT("Face_Tere")}){ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(*(FString(TEXT("/Game/Environment/Welcome20261003/FaceShading/M_WelcomeFace_"))+N));FaceShadingMaterials.Add(Asset.Object);}
    for(const TCHAR* N:{TEXT("WelcomeLonely"),TEXT("WelcomeShy"),TEXT("Reunion"),TEXT("WaveHappy"),TEXT("WaveLonely"),TEXT("WaveShy"),TEXT("ClapJump"),TEXT("StretchMood"),TEXT("YawnMood"),TEXT("FlowerCenterMood"),TEXT("FlowerLeftMood"),TEXT("FlowerRightMood"),TEXT("PetalSuccess"),TEXT("ChimeStartled")}){
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_"))+N));LivelyMotions.Add(N,Asset.Object);
    }
    for(const TCHAR* N:{TEXT("Happy"),TEXT("Shy"),TEXT("Lonely"),TEXT("Protest")}){
        ConstructorHelpers::FObjectFinder<UAnimSequence> Asset(*(FString(TEXT("/Game/Emma/Animations/Lively20261003/A_Emma_Lively_Response"))+N));LivelyResponses.Add(N,Asset.Object);
    }
    for(const TCHAR* N:{TEXT("DaydreamHead"),TEXT("HumHead"),TEXT("ShyAway"),TEXT("ShyAwayOther"),TEXT("PoutHead"),TEXT("PoutHeadOther"),TEXT("StartleBody"),TEXT("CheckPlayerSky"),TEXT("SkyGaze"),TEXT("AncientGaze"),TEXT("CountStars"),TEXT("CloudWatch"),TEXT("MusicNod"),TEXT("SmallStartle"),TEXT("Doze"),TEXT("LonelyWave"),TEXT("ByeWave"),TEXT("GoodnightWave"),TEXT("FoundLeft"),TEXT("FoundCenter"),TEXT("FoundRight"),TEXT("FoundProudLeft"),TEXT("FoundProudCenter"),TEXT("FoundProudRight"),TEXT("HighFiveInvite"),TEXT("HighFiveResponse")}){
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_"))+N));LivelyMotions.Add(N,Asset.Object);
    }
    {ConstructorHelpers::FObjectFinder<UAnimSequence> Asset(TEXT("/Game/Emma/Animations/Lively20261003/A_Emma_Lively_PoutResponse"));LivelyResponses.Add(TEXT("Pout"),Asset.Object);}

    for(const TCHAR* N:{TEXT("P3Prayer"),TEXT("P3Firefly"),TEXT("P3PointMeteor"),TEXT("P3PinchMoon"),TEXT("P3SmellFlower"),TEXT("P3Sneeze"),TEXT("P3HatShake"),TEXT("P3MothWatch"),TEXT("P3WindFace"),TEXT("P3ListenChime"),TEXT("P3WindHair"),TEXT("P3PetalMiss")}){ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_"))+N));LivelyMotions.Add(N,Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(TEXT("/Engine/BasicShapes/Sphere"));P3Sphere=Asset.Object;}
    {ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(TEXT("/Game/Environment/Lively20261003/M_Firefly"));P3GlowMaterial=Asset.Object;}
    {ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(TEXT("/Game/Environment/Lively20261003/M_Moth"));P3MothMaterial=Asset.Object;}
    {ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(TEXT("/Game/Environment/Lively20261003/M_GustPetal"));P3PetalMaterial=Asset.Object;}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_P3Prayer2"));LivelyMotions.Add(TEXT("P3Prayer"),Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_P3PointMeteor3"));LivelyMotions.Add(TEXT("P3PointMeteor"),Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_P3PinchMoon3"));LivelyMotions.Add(TEXT("P3PinchMoon"),Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_P3Firefly2"));LivelyMotions.Add(TEXT("P3Firefly"),Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_DozeQuiet"));LivelyMotions.Add(TEXT("Doze"),Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/Lively20261003/AM_Emma_Lively_SkyGazeHappy"));LivelyMotions.Add(TEXT("SkyGazeHappy"),Asset.Object);}
    {ConstructorHelpers::FObjectFinder<UAnimSequence> Asset(TEXT("/Game/Emma/Animations/Lively20261003/A_Emma_Lively_ResponseHat"));LivelyResponses.Add(TEXT("Hat"),Asset.Object);}
    for(const TCHAR* N:{TEXT("WaveLonely"),TEXT("WelcomeLonely"),TEXT("WaveShy"),TEXT("WelcomeShy"),TEXT("DaydreamHead"),TEXT("HumHead"),TEXT("ShyAway"),TEXT("ShyAwayOther"),TEXT("PoutHead"),TEXT("PoutHeadOther"),TEXT("StartleBody"),TEXT("SmallStartle"),TEXT("CheckPlayerSky"),TEXT("SkyGaze"),TEXT("SkyGazeHappy"),TEXT("AncientGaze"),TEXT("CountStars"),TEXT("CloudWatch"),TEXT("MusicNod"),TEXT("Doze"),TEXT("FoundLeft"),TEXT("FoundProudLeft"),TEXT("FoundCenter"),TEXT("FoundProudCenter"),TEXT("FoundRight"),TEXT("FoundProudRight"),TEXT("Chuckle"),TEXT("Mumble"),TEXT("SuppressYawn"),TEXT("Proud"),TEXT("DepartLeft"),TEXT("DepartRight"),TEXT("P3Sneeze"),TEXT("P3MothWatch"),TEXT("P3WindFace"),TEXT("LonelyWave"),TEXT("ByeWave"),TEXT("GoodnightWave"),TEXT("HighFiveInvite"),TEXT("HighFiveResponse")}){
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/Lively20261004/AM_Emma_Lively_"))+N+TEXT("_v2")));TunedMotions.Add(N,Asset.Object);
    }
    for(const TCHAR* N:{TEXT("Lonely"),TEXT("Shy"),TEXT("Protest")}){ConstructorHelpers::FObjectFinder<UAnimSequence> Asset(*(FString(TEXT("/Game/Emma/Animations/Lively20261004/A_Emma_Lively_Response"))+N+TEXT("_v2")));TunedResponses.Add(N,Asset.Object);}
    LivelyMotions.Add(TEXT("OriginalPetal"),LivingMotions[0]);LivelyMotions.Add(TEXT("OriginalChime"),LivingMotions[1]);
    for(const TCHAR* N:{TEXT("ByeWave"),TEXT("GoodnightWave"),TEXT("HighFiveInvite"),TEXT("HighFiveResponse")}){
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/Presence20261004/AM_Emma_Lively_"))+N+TEXT("_v4")));PresenceMotions.Add(N,Asset.Object);
    }
    for(const TCHAR* N:{TEXT("LonelyWave"),TEXT("WaveLonely"),TEXT("WaveShy"),TEXT("WelcomeLonely"),TEXT("WelcomeShy"),TEXT("DaydreamHead"),TEXT("HumHead"),TEXT("MusicNod"),TEXT("Chuckle"),TEXT("Mumble"),TEXT("Proud")}){
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/Presence20261004/AM_Emma_Lively_"))+N+TEXT("_v3")));PresenceMotions.Add(N,Asset.Object);
    }
    // WelcomeRestore 2026-10-04: the same one-sweep lowering for the happy wave and the S4 lonely/shy waves.
    {ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(TEXT("/Game/Emma/Animations/WelcomeRestore20261004/AM_Emma_Lively_WaveHappy_Sweep"));LivelyMotions.Add(TEXT("WaveHappy"),Asset.Object);}
    for(const TCHAR* N:{TEXT("WelcomeLonely"),TEXT("WaveLonely"),TEXT("WelcomeShy"),TEXT("WaveShy")}){
        ConstructorHelpers::FObjectFinder<UAnimMontage> Asset(*(FString(TEXT("/Game/Emma/Animations/WelcomeRestore20261004/AM_Emma_Lively_"))+N+TEXT("_v5")));PresenceMotions.Add(N,Asset.Object);
    }
    // Expression remap batch 1 (2026-10-05): copies of every action sequence whose face curves changed (generated list).
    {static const TCHAR* const Rows[][4]={
#include "ExpressionRemapList.inl"
    };
    for(const auto& P:Rows){ConstructorHelpers::FObjectFinder<UAnimSequence> Old(P[0]);if(!Old.Object)continue;
        UAnimSequence* B[3]={nullptr,nullptr,nullptr};
        for(int32 K=0;K<3;++K)if(*P[K+1]){ConstructorHelpers::FObjectFinder<UAnimSequence> New(P[K+1]);B[K]=New.Object;}
        RemapOld.Add(Old.Object);RemapNew.Add(B[0]);RemapNew2.Add(B[1]);RemapNew3.Add(B[2]);}}
    // Body de-duplication (2026-10-06): {original, body copy} (generated list).
    {static const TCHAR* const Rows[][2]={
#include "BodyRemapList.inl"
    };
    for(const auto& P:Rows){ConstructorHelpers::FObjectFinder<UAnimSequence> Old(P[0]);ConstructorHelpers::FObjectFinder<UAnimSequence> New(P[1]);
        if(Old.Object&&New.Object){BodyOld.Add(Old.Object);BodyNew.Add(New.Object);}}}
    GazeClass=FSoftClassPath(TEXT("/Game/Emma/Animations/LivelyGaze20261004/ABP_Emma_Gaze.ABP_Emma_Gaze_C"));
}
void ACourtyardController::BeginPlay()
{
    Super::BeginPlay();
    InitializeLively();InitializeMood();
    BodyRemapOn=!FParse::Param(FCommandLine::Get(),TEXT("NoBodyRemap"));
    LocoOn=!FParse::Param(FCommandLine::Get(),TEXT("NoLocomotion"));if(LocoOn)InitLocomotionNav();
    ApplyExpressionRemap(FParse::Param(FCommandLine::Get(),TEXT("NoExpressionRemap"))?0:3);
    BindResponse(0);
    NextAmbient=GetWorld()->GetTimeSeconds()+FMath::FRandRange(16.f,26.f);
    NextCompanion=GetWorld()->GetTimeSeconds()+FMath::FRandRange(18.f,30.f);
    AmbientStillSince=GetWorld()->GetTimeSeconds();
    // Keep engine diagnostics in logs/editor, not over the player's garden.
    if(!GetWorld()->IsPlayInEditor()) UKismetSystemLibrary::ExecuteConsoleCommand(this,TEXT("DisableAllScreenMessages"));
    // A stable random sky schedule for this visit; no per-frame decisions.
    float SkySeed=float(FGuid::NewGuid().A & 65535u);
    if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))
        FParse::Value(FCommandLine::Get(),TEXT("SkyReviewSeed="),SkySeed);
    for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
        if(It->ActorHasTag(TEXT("CourtyardSky")))
            if(auto* MID=It->GetStaticMeshComponent()->CreateDynamicMaterialInstance(0))
                { MID->SetScalarParameterValue(TEXT("SessionSeed"),SkySeed); ContextSky=MID; }
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_SKY session_seed=%.0f"),SkySeed);
    for(TActorIterator<AActor> It(GetWorld());It;++It) {
        TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
        for(auto* C:Parts) if(C->GetStaticMesh() && !C->bHiddenInGame) {
            FString N=C->GetStaticMesh()->GetName().ToLower();
            if((N.Contains(TEXT("flower")) && !N.Contains(TEXT("tree"))) || N.Contains(TEXT("lavender")))FlowerComponents.Add(C);
        }
    }
    bool Found=false; Applied=UCourtyardPreferencesSave::Load(Found); Draft=Applied;
    // Confirmed custom settings override both stale UE ini files and trial modes interrupted by exit/crash.
    Applied.ApplyEngine(!GetWorld()->IsPlayInEditor());
    InitializePlayer();
    InitializeBodyProps();
    OpenPage(ECourtyardPage::Game);
}

bool ACourtyardController::BindResponse(int32 Index)
{
    UAnimSequence* Sequence=Index==1?BrightResponse.Get():Index==2?TenderResponse.Get():AttentiveResponse.Get();
    auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Emma/Animations/AM_Emma_HeadPatSmile.AM_Emma_HeadPatSmile"));
    if(!Sequence || !Montage || Montage->SlotAnimTracks.Num()!=1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()!=1
       || Montage->GetSkeleton()!=Sequence->GetSkeleton() || !FMath::IsNearlyEqual(Montage->GetPlayLength(),Sequence->GetPlayLength(),.01f)) return false;
    Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].SetAnimReference(Sequence,false);
    LastResponse=Index;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_RESPONSE variant=%d asset=%s"),Index,*Sequence->GetName());
    return true;
}
void ACourtyardController::SelectResponse()
{
    if(SelectLivelyResponse())return;
    // Called only after CanRespond; never change an in-flight montage.
    if(ResponseBag.IsEmpty()) ResponseBag={0,1,2};
    TArray<int32> Choices;
    for(int32 I=0;I<ResponseBag.Num();++I) if(ResponseBag[I]!=LastResponse) Choices.Add(I);
    if(Choices.IsEmpty()) Choices.Add(0);
    const int32 Pick=Choices[FMath::RandHelper(Choices.Num())];
    const int32 Variant=ResponseBag[Pick];
    if(BindResponse(Variant)) ResponseBag.RemoveAt(Pick);
}

bool ACourtyardController::FaceFrame(FVector& Eyes,FVector& Front) const
{
    auto* A=Cast<ACharacter>(Emma());if(!A || !A->GetMesh())return false;
    auto* M=A->GetMesh();Eyes=(M->GetSocketLocation(TEXT("目_L"))+M->GetSocketLocation(TEXT("目_R")))*.5;
    Front=(Eyes-M->GetSocketLocation(TEXT("頭"))).GetSafeNormal2D();return !Front.IsNearlyZero();
}
bool ACourtyardController::VisibleTarget(const FVector& Target,const AActor* TargetActor) const
{
    FVector E,F;if(!FaceFrame(E,F))return false;
    FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardContext),false,Emma());
    return !GetWorld()->LineTraceSingleByChannel(H,E,Target,ECC_Visibility,Q) || (TargetActor && H.GetActor()==TargetActor);
}
bool ACourtyardController::CompanionContext(float& Yaw) const
{
    if(Page!=ECourtyardPage::Game || !PlayerCharacter || PoseTransition || IsSeatBusy() || PlayerCharacter->GetVelocity().Size()>12.f)return false;
    FVector E,F,V;FRotator R;if(!FaceFrame(E,F))return false;GetPlayerViewPoint(V,R);
    const FVector D=V-E;const float Distance=D.Size();
    if(Distance<55.f || Distance>240.f || FVector::DotProduct(R.Vector(),-D.GetSafeNormal())<.96f)return false;
    Yaw=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F,D.GetSafeNormal2D()).Z,FVector::DotProduct(F,D.GetSafeNormal2D())));
    const float Pitch=FMath::RadiansToDegrees(FMath::Atan2(D.Z,D.Size2D()));
    return FMath::Abs(Yaw)<29.f && Pitch>-21.f && Pitch<5.f && VisibleTarget(V,PlayerCharacter);
}
int32 ACourtyardController::FlowerContext()
{
    FVector E,F;if(!FaceFrame(E,F))return INDEX_NONE;
    float Best=10000.f;int32 Choice=INDEX_NONE;
    for(auto Weak:FlowerComponents) if(auto* C=Weak.Get()) {
        if(!C->GetStaticMesh() || !C->IsVisible())continue;
        auto* Instances=Cast<UInstancedStaticMeshComponent>(C);
        const int32 Count=Instances?Instances->GetInstanceCount():1;
        for(int32 I=0;I<Count;++I) {
            FTransform T=C->GetComponentTransform();if(Instances && !Instances->GetInstanceTransform(I,T,true))continue;
            const auto B=C->GetStaticMesh()->GetBounds();
            const FVector Target=T.TransformPosition(B.Origin+FVector(0,0,B.BoxExtent.Z*.55));
            const FVector D=Target-E;const float Dist=D.Size2D();if(Dist<130.f || Dist>620.f)continue;
            const float Y=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F,D.GetSafeNormal2D()).Z,FVector::DotProduct(F,D.GetSafeNormal2D())));
            const float Down=-FMath::RadiansToDegrees(FMath::Atan2(D.Z,Dist));
            if(FMath::Abs(Y)>29.f || Down<7.f || Down>25.f || !VisibleTarget(Target,C->GetOwner()))continue;
            const float Score=FMath::Abs(Y)*3.f+Dist*.08f+FMath::Abs(Down-14.f);
            if(Score<Best){Best=Score;AmbientTarget=Target;Choice=Y>8.f?3:Y< -8.f?4:1;}
        }
    }
    return Choice;
}
bool ACourtyardController::MeteorContext()
{
    if(!ContextSky)return false;
    // Match the existing QuietCirrus shader's integer hash and event geography exactly.
    auto Hash=[](uint32 X,uint32 Y,uint32 Z){uint32 N=X*1597334677u ^ Y*3812015801u ^ Z*2798796415u;N^=N>>16;N*=2246822519u;N^=N>>13;return float(N&16777215u)/16777216.f;};
    const float T=GetWorld()->GetTimeSeconds()+ContextSky->K2_GetScalarParameterValue(TEXT("ReviewTimeOffset"));
    const int32 Epoch=FMath::FloorToInt(T/68.f);if(Epoch==LastMeteorEpoch)return false;
    const uint32 Seed=uint32(ContextSky->K2_GetScalarParameterValue(TEXT("SessionSeed")));
    const float S=Hash(Epoch,249,Seed),Phase=T-Epoch*68-(12+S*30);
    const int32 Kind=FMath::FloorToInt(Hash(Epoch,8,Seed+21)*4);const float Duration=Kind==0?2.8f:Kind==1?4.7f:Kind==2?3.8f:6.f;
    if(Phase<.15f || Phase>FMath::Min(1.1f,Duration-1.6f))return false;
    const float Dir=Hash(Epoch,49,Seed+37);
    const float Az=(Dir<.76f?FMath::Lerp(-2.7f,-.3f,Dir/.76f):FMath::Lerp(.15f,2.95f,(Dir-.76f)/.24f))+(.12f+S*.045f)*(Phase+.65f);
    const float El=.43f+Hash(Epoch,88,Seed+3)*.40f+(-.055f-S*.018f)*(Phase+.65f);
    FVector E,F;if(!FaceFrame(E,F))return false;
    const FVector D(FMath::Cos(El)*FMath::Cos(Az),FMath::Cos(El)*FMath::Sin(Az),FMath::Sin(El));
    const float Y=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F,D.GetSafeNormal2D()).Z,FVector::DotProduct(F,D.GetSafeNormal2D())));
    if(FMath::Abs(Y)>16.f || El<.28f || El>.67f)return false;
    AmbientTarget=E+D*5000.f;if(!VisibleTarget(AmbientTarget))return false;
    LastMeteorEpoch=Epoch;LastMeteorKind=Kind;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTEXT meteor_visible epoch=%d kind=%d phase=%.2f yaw=%.2f elevation=%.2f"),Epoch,Kind,Phase,Y,FMath::RadiansToDegrees(El));
    return true;
}
bool ACourtyardController::TryAmbientBehavior(int32 Variant)
{
    if(ContentLayer.IsValid() && ContentLayer->BlocksOrdinaryBehavior())return false;
    ACharacter* A=Cast<ACharacter>(Emma());
    if(!A || LivingTrip || ActiveAmbient || !ContextMotions.IsValidIndex(Variant) || !A->GetMesh())return false;
    UAnimInstance* Inst=A->GetMesh()->GetAnimInstance();const double Now=GetWorld()->GetTimeSeconds();
    if(!Inst || Inst->IsAnyMontagePlaying() || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted"))
       || !BPBool(A,TEXT("AllowInteraction")) || !QueryBool(A,TEXT("CanHeadPat")) || A->GetVelocity().Size()>2.f
       || Now-AmbientStillSince<5. || ReadNumber(A,TEXT("DueTime"),Now)-Now<9.
       || Now-ReadNumber(A,TEXT("LastGlanceTime"),-100.)<10. || Page!=ECourtyardPage::Game)return false;
    if(Variant==0 && !MeteorContext())return false;
    if(Variant==0 && LivelyStage>=3 && LastMeteorKind==3 && StartLively(TEXT("C2")))return true;
    if((Variant==1 || Variant==3 || Variant==4) && FlowerContext()!=Variant)return false;
    if(Variant==2 && (!RestPending || Now-ArrivalTime>40.))return false;
    if(Variant==5 || Variant==6 || Variant==8) {
        float Y=0;if(Now<NextCompanion || AttentionSince<0 || Now-AttentionSince<2.2 || !CompanionContext(Y))return false;
        if(Variant!=(Y>8.f?5:Y< -8.f?6:8))return false;
    }
    if(Variant==7) {
        float Y=0;
        if(!GreetingArmed || GreetingArrival<0 || Now-GreetingArrival<2. || Now-GreetingArrival>25. || Now<NextGreeting || !CompanionContext(Y) || FMath::Abs(Y)>12.f)return false;
    }
    if(GazeEnabled && GazeLayerEnabled && (Variant==5 || Variant==6 || Variant==8)){
        FLivelyFacePhase NoticePhase;NoticePhase.Start=0;NoticePhase.End=.6f;NoticePhase.Fade=.18f;NoticePhase.Values={{41,.35f}};  // P6 only (expression remap batch 3)
        if(!StartFace(TEXT("GazeNotice"),{NoticePhase},.8f,TEXT(""),1))return false;
        GazeNoticeAt=Now;MoodEvent(TEXT("eye"));LastAmbient=Variant;AttentionSince=-1;
        NextCompanion=Now+(Pose==ECourtyardPose::Seated?FMath::FRandRange(35.f,55.f):FMath::FRandRange(65.f,105.f));
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_GAZE notice variant=%d montage=0 time=%.3f"),Variant,Now);return true;
    }
    UAnimMontage* Motion=ContextMotions[Variant];if(!Motion)return false;
    if(LivelyReady(TEXT("B12")) && Pose==ECourtyardPose::Crouched && (Variant==1 || Variant==3 || Variant==4))Motion=LivelyMotion(Variant==1?TEXT("FlowerCenterMood"):Variant==3?TEXT("FlowerLeftMood"):TEXT("FlowerRightMood"),Motion);
    if(Variant==7)Motion=LivelyBodyVariant(0,Motion);
    WriteBool(A,TEXT("AllowInteraction"),false);
    if(Inst->Montage_Play(Motion,1.f,EMontagePlayReturnType::MontageLength,0.f,false)<=0.f)
    {WriteBool(A,TEXT("AllowInteraction"),true);return false;}
    ActiveAmbient=Motion;AmbientActor=A;AmbientBehavior=Variant;LastAmbient=Variant;
    if(Variant==7)LivelyBodyBegan(0);
    if(Variant==5 || Variant==6 || Variant==8)MoodEvent(TEXT("eye"));
    if(Variant==0){MoodEvent(TEXT("sky"));P3WishPending=LivelyStage>=3 && LivelyReady(TEXT("C1")) && (ReviewWishOnce || FMath::FRand()<.2f);ReviewWishOnce=false;}
    if(Motion==LivelyMotion(TEXT("FlowerCenterMood"),nullptr) || Motion==LivelyMotion(TEXT("FlowerLeftMood"),nullptr) || Motion==LivelyMotion(TEXT("FlowerRightMood"),nullptr)){LivelyStarted(TEXT("B12"),TEXT("shared_flowers"),true);MoodEvent(TEXT("shared"));}
    if(Variant==2)AmbientTarget=FVector::ZeroVector;
    if(Variant==5 || Variant==6 || Variant==7 || Variant==8){FRotator R;GetPlayerViewPoint(AmbientTarget,R);}
    if(Variant==2)RestPending=false;
    if(Variant==7){GreetingArmed=false;GreetingArrival=-1;NextGreeting=Now+FMath::FRandRange(130.f,200.f);NextCompanion=Now+FMath::FRandRange(45.f,75.f);}
    // Sitting together on the bench, she turns to the player more often.
    if(Variant==5 || Variant==6 || Variant==8){NextCompanion=Now+(Pose==ECourtyardPose::Seated?FMath::FRandRange(35.f,55.f):FMath::FRandRange(65.f,105.f));AttentionSince=-1;}
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_AMBIENT start variant=%d time=%.2f seen=%d pos=%s target=%s"),Variant,Now,PlayerWatching,*A->GetActorLocation().ToString(),*AmbientTarget.ToString());
    return true;
}
void ACourtyardController::TickAmbientBehavior()
{
    const double Now=GetWorld()->GetTimeSeconds();
    if(ActiveAmbient) {
        ACharacter* A=AmbientActor.Get();auto* Inst=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
        // If the player leaves, calmly blend out rather than maintain eye contact through a wall.
        if(Inst && (AmbientBehavior==5 || AmbientBehavior==6 || AmbientBehavior==8)) {
            FVector E,F,V;FRotator R;GetPlayerViewPoint(V,R);
            if(FaceFrame(E,F) && ((V-E).Size()>270.f || !VisibleTarget(V,PlayerCharacter)))Inst->Montage_Stop(.65f,ActiveAmbient);
        }
        if(Inst && (Inst->Montage_IsActive(ActiveAmbient) || Inst->IsAnyMontagePlaying()))return;
        if(A && !BPBool(A,TEXT("TravelActive")) && (!Inst || !Inst->IsAnyMontagePlaying()))WriteBool(A,TEXT("AllowInteraction"),true);
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_AMBIENT end variant=%d time=%.2f"),AmbientBehavior,Now);
        if(AmbientBehavior==0 && P3WishPending){P3WishPending=false;P3WishUntil=Now+12;}
        LivelyBodyEnded();ActiveAmbient=nullptr;AmbientActor.Reset();AmbientBehavior=INDEX_NONE;
        AmbientStillSince=Now;NextAmbient=Now+FMath::FRandRange(22.f,44.f);return;
    }
    if(Now<NextAmbientCheck)return;NextAmbientCheck=Now+.2;
    AActor* A=Emma();if(!A)return;
    if(PlayerCharacter) {
        const float Distance=FVector::Dist(PlayerCharacter->GetActorLocation(),A->GetActorLocation());
        if(Distance>330.f){if(PlayerFarSince<0)PlayerFarSince=Now;if(Now-PlayerFarSince>4.)GreetingArmed=true;GreetingArrival=-1;}
        else {PlayerFarSince=-1;if(Distance<240.f && GreetingArmed && GreetingArrival<0)GreetingArrival=Now;}
        if(GreetingArrival>=0 && Now-GreetingArrival>25.){GreetingArrival=-1;GreetingArmed=false;}
    }
    const bool Travelling=BPBool(A,TEXT("TravelActive")) || A->GetVelocity().Size()>2.f;
    if(Travelling){WasTravelling=true;RestPending=false;}
    else if(WasTravelling && !BPBool(A,TEXT("Interrupted")) && BPBool(A,TEXT("AllowInteraction")) && ReadNumber(A,TEXT("DueTime"),Now)-Now>9.) {
        WasTravelling=false;ArrivalTime=Now;RestPending=FMath::FRand()<.72f;
        NextAmbient=Now+FMath::FRandRange(6.f,11.f);
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTEXT arrived time=%.2f rest=%d"),Now,RestPending);
    }
    if(Travelling || !BPBool(A,TEXT("AllowInteraction")) || BPBool(A,TEXT("Interrupted")))
    {AmbientStillSince=Now;AttentionSince=-1;return;}
    float Y=0;const bool Attention=CompanionContext(Y);
    if(Attention){if(AttentionSince<0)AttentionSince=Now;}else AttentionSince=-1;
    if(Now-AmbientStillSince<5. || Page!=ECourtyardPage::Game)return;
    auto* C=Cast<ACharacter>(A);auto* Inst=C&&C->GetMesh()?C->GetMesh()->GetAnimInstance():nullptr;
    if(!Inst || Inst->IsAnyMontagePlaying() || ReadNumber(A,TEXT("DueTime"),Now)-Now<9.)return;
    if(ReviewBodyHold || (ContentLayer.IsValid() && ContentLayer->BlocksOrdinaryBehavior()))return;
    if(TryAmbientBehavior(0))return;
    if(TryAmbientBehavior(7))return;
    if(Attention && TryAmbientBehavior(Y>8.f?5:Y< -8.f?6:8))return;
    if(TryCompanionReaction())return;
    if(Now<NextAmbient)return;
    if(!SeenOrWaited(AmbientDefer)){NextAmbient=Now+3;return;}
    NextAmbient=Now+FMath::FRandRange(22.f,44.f);
    if(FMath::FRand()<.22f)return;
    if(RestPending && Now-ArrivalTime<40. && TryAmbientBehavior(2)){AmbientDefer=-1;return;}
    int32 Flower=FlowerContext();if(Flower!=INDEX_NONE && Flower!=LastAmbient && TryAmbientBehavior(Flower))AmbientDefer=-1;
}

void ACourtyardController::CourtyardReviewBehavior(int32 Variant)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    const bool Started=TryAmbientBehavior(Variant);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_AMBIENT review variant=%d accepted=%d"),Variant,Started);
}

void ACourtyardController::InitializePlayer()
{
    PlayerCharacter=Cast<ACharacter>(GetPawn());
    if(!PlayerCharacter) return;
    ViewCamera=PlayerCharacter->FindComponentByClass<UCameraComponent>();
    if(!ViewCamera) return;
    OriginalCamera=ViewCamera->GetRelativeLocation();
    BodyYaw=GetControlRotation().Yaw;
    Initialized=true;
    // Disable the old music key listener without changing its Tick-based random scheduler.
    for(TActorIterator<AActor> It(GetWorld());It;++It)
        if(It->GetClass()->GetName().Contains(TEXT("BP_QuietMusic"))) { MusicManager=*It; It->DisableInput(this); }
    ApplyRuntimePreferences();
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_SYSTEMS initialized camera=%s input=%s"),*OriginalCamera.ToString(),*GetNameSafe(PlayerCharacter->InputComponent));
    if(PlayerCharacter->InputComponent) for(auto& K:PlayerCharacter->InputComponent->KeyBindings)
        UE_LOG(LogTemp,Display,TEXT("COURTYARD_LEGACY key=%s event=%d bound=%d"),*K.Chord.Key.ToString(),K.KeyEvent,K.KeyDelegate.IsBound());
}

bool ACourtyardController::BPBool(const UObject* O,FName N) const
{
    if(O) if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N)) return P->GetPropertyValue_InContainer(O);
    return false;
}
bool ACourtyardController::QueryBool(UObject* O,FName Name)
{
    if(!O) return false;
    UFunction* F=O->FindFunction(Name); if(!F) return false;
    FStructOnScope Args(F); O->ProcessEvent(F,Args.GetStructMemory());
    for(TFieldIterator<FBoolProperty> It(F);It;++It)
        if(It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)) return It->GetPropertyValue_InContainer(Args.GetStructMemory());
    return false;
}
AActor* ACourtyardController::Emma() const
{
    for(TActorIterator<ACharacter> It(GetWorld());It;++It) if(*It!=PlayerCharacter && It->FindFunction(TEXT("CanHeadPat"))) return *It;
    return nullptr;
}
bool ACourtyardController::IsSeatBusy() const { return BPBool(PlayerCharacter,TEXT("SeatTransition")); }
bool ACourtyardController::CanRespond() const
{
    if(IsApproachPrototypeWorld())return false;
    if(Page!=ECourtyardPage::Game || PoseTransition || IsSeatBusy())return false;
    if(ContentLayer.IsValid() && ContentLayer->BlocksPatResponse())return false;
    FVector Eye;FRotator R;GetPlayerViewPoint(Eye,R);FHitResult H;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardRespond),false,PlayerCharacter);
    // Exactly the existing input/HUD range and channel, now shared with every new posture.
    return GetWorld()->LineTraceSingleByChannel(H,Eye,Eye+R.Vector()*120.f,ECC_Visibility,Q) && (QueryBool(H.GetActor(),TEXT("CanHeadPat")) || (H.GetActor()==Emma() && LivelyCanYieldToPat()));
}
bool ACourtyardController::CanSeat() const { return Page==ECourtyardPage::Game && !PoseTransition && !IsSeatBusy() && (Pose==ECourtyardPose::Standing || Pose==ECourtyardPose::Seated) && QueryBool(PlayerCharacter,TEXT("CanUseSeat")); }
FString ACourtyardController::KeyLabel(ECourtyardAction A) const { return Applied.Keys.IsValidIndex((int32)A) ? Applied.Keys[(int32)A].GetDisplayName().ToString() : TEXT("?"); }
void ACourtyardController::SetNotice(const FString& Text) { Notice=Text; NoticeUntil=FPlatformTime::Seconds()+4; }

void ACourtyardController::FlushPressedKeys() { Held.Empty(); if(auto* H=Cast<ACourtyardHUD>(GetHUD()))H->Release();Super::FlushPressedKeys(); }
void ACourtyardController::OpenPage(ECourtyardPage NewPage)
{
    FlushPressedKeys(); Page=NewPage; bShowMouseCursor=Page!=ECourtyardPage::Game;
    DropMouseUntil=FPlatformTime::Seconds()+.15;
    if(bShowMouseCursor) SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    else SetInputMode(FInputModeGameOnly());
    if(auto* V=GetWorld()->GetGameViewport())
    {
        V->SetMouseCaptureMode(bShowMouseCursor?EMouseCaptureMode::CaptureDuringMouseDown:EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
        V->SetMouseLockMode(bShowMouseCursor?EMouseLockMode::DoNotLock:EMouseLockMode::LockOnCapture);
    }
    // Do not disable movement or pause: gravity and existing seated transitions keep running.
    if(PlayerCharacter) { PlayerCharacter->ConsumeMovementInputVector(); if(PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) PlayerCharacter->GetCharacterMovement()->StopMovementImmediately(); }
    if(NewPage==ECourtyardPage::Settings) Draft=Applied;
}
void ACourtyardController::Escape()
{
    if(CaptureAction!=INDEX_NONE) { CaptureAction=INDEX_NONE;SetNotice(TEXT("已取消改键"));return; }
    switch(Page)
    {
        case ECourtyardPage::Game:OpenPage(ECourtyardPage::Home);break;
        case ECourtyardPage::Home:case ECourtyardPage::Map:OpenPage(ECourtyardPage::Game);break;
        case ECourtyardPage::DisplayConfirm:ConfirmDisplay(false);break;
        default:Draft=Applied;OpenPage(ECourtyardPage::Home);break;
    }
}
bool ACourtyardController::InputKey(const FInputKeyEventArgs& P)
{
    if(ContentLayer.IsValid() && ContentLayer->CaptureOnly)return true; // Background observation only; normal preview input is unchanged.
    if(P.Key==EKeys::MouseX || P.Key==EKeys::MouseY)
    {
        if(Page==ECourtyardPage::Game && !PoseTransition && !IsSeatBusy() && !IsLookInputIgnored() && FPlatformTime::Seconds()>DropMouseUntil)
        {
            FRotator R=GetControlRotation(); const float D=P.AmountDepressed*.07f*Applied.Sensitivity;
            if(P.Key==EKeys::MouseX) R.Yaw+=D; else R.Pitch+=D;
            R.Pitch=FMath::ClampAngle(R.Pitch,Pose==ECourtyardPose::Supine?35.f:-80.f,Pose==ECourtyardPose::Supine?85.f:85.f);
            if(Pose==ECourtyardPose::Supine) R.Yaw=BodyYaw+FMath::Clamp(FMath::FindDeltaAngleDegrees(BodyYaw,R.Yaw),-50.f,50.f);
            R.Roll=0; SetControlRotation(R);
        }
        return true;
    }
    if(P.Event==IE_Released) { Held.Remove(P.Key);if(P.Key==EKeys::LeftMouseButton)if(auto* H=Cast<ACourtyardHUD>(GetHUD())){float X,Y;if(GetMousePosition(X,Y))H->Drag(X);H->Release();} if(P.Key==Applied.Keys[(int32)ECourtyardAction::Jump] && PlayerCharacter) PlayerCharacter->StopJumping(); return true; }
    if(P.Event!=IE_Pressed) return true;
    if(P.Key==EKeys::Escape) { Escape();return true; }
    if(CaptureAction!=INDEX_NONE)
    {
        if(!FCourtyardPreferences::IsBindable(P.Key)) {SetNotice(TEXT("请选择普通键盘按键；Esc 保留用于返回"));return true;}
        const int32 Conflict=Draft.Keys.IndexOfByKey(P.Key);
        if(Conflict!=INDEX_NONE && Conflict!=CaptureAction) {SetNotice(TEXT("此键已用于：")+FCourtyardPreferences::ActionName(Conflict));return true;}
        Draft.Keys[CaptureAction]=P.Key;CaptureAction=INDEX_NONE;SetNotice(TEXT("按键已选择，应用后生效"));return true;
    }
    if(P.Key==EKeys::Enter && (IsInputKeyDown(EKeys::LeftAlt)||IsInputKeyDown(EKeys::RightAlt)||Held.Contains(EKeys::LeftAlt)||Held.Contains(EKeys::RightAlt)))
    {
        if(Page!=ECourtyardPage::DisplayConfirm) {Draft=Applied;Draft.WindowMode=Applied.WindowMode==2?1:2;ApplyDraft();} return true;
    }
    if(Page!=ECourtyardPage::Game)
    {
        if(Page==ECourtyardPage::Map && P.Key==Applied.Keys[(int32)ECourtyardAction::Map]) OpenPage(ECourtyardPage::Game);
        else if(P.Key==EKeys::LeftMouseButton) {Held.Add(P.Key);float X=0,Y=0;bool Valid=GetMousePosition(X,Y);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CLICK valid=%d pos=%.1f,%.1f page=%d"),Valid,X,Y,(int32)Page);if(Valid) ClickUI(X,Y);}
        return true;
    }
    Held.Add(P.Key);
    const int32 Action=Applied.Keys.IndexOfByKey(P.Key);
    if(Action==INDEX_NONE && ContentLayer.IsValid() && ContentLayer->HandleKey(P.Key))return true;
    if(Action!=INDEX_NONE) HandleAction(Action);
    return true; // Legacy bindings never also receive the physical key.
}
void ACourtyardController::ExecuteLegacy(FKey Key)
{
    if(!PlayerCharacter || !PlayerCharacter->InputComponent) return;
    for(auto& B:PlayerCharacter->InputComponent->KeyBindings)
        if(B.Chord.Key==Key && B.KeyEvent==IE_Pressed) {B.KeyDelegate.Execute(Key);return;}
    SetNotice(TEXT("原交互入口未连接，已拒绝执行"));
    UE_LOG(LogTemp,Error,TEXT("COURTYARD missing legacy %s"),*Key.ToString());
}
void ACourtyardController::HandleAction(int32 A)
{
    if(A==(int32)ECourtyardAction::Map) {OpenPage(ECourtyardPage::Map);return;}
    if(!PlayerCharacter || PoseTransition || IsSeatBusy()) return;
    if(A==(int32)ECourtyardAction::Respond) {if(IsApproachPrototypeWorld())return;if(ContentLayer.IsValid() && ContentLayer->BlocksPatResponse())return;if(ReplyHighFive())return;if(TryLivelyPat())return;
        // walking (locomotion rework): E near her asks her to stop; she stays for the pat
        if(LocoOn && LocoState!=ELocoState::Idle && PlayerCharacter && Emma() && FVector::Dist2D(PlayerCharacter->GetActorLocation(),Emma()->GetActorLocation())<320.f){LocoStopRequested=true;LocoPatWantedAt=GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIFE pat_while_walking stop_requested"));return;}
        if(CanRespond())ExecutePatResponse();return;}
    if(A==(int32)ECourtyardAction::Seat) {if(CanSeat())ExecuteLegacy(EKeys::F);return;}
    if(Pose==ECourtyardPose::Seated) return;
    if(A==(int32)ECourtyardAction::Supine) {if(Pose!=ECourtyardPose::Prone) TryPose(Pose==ECourtyardPose::Supine?ECourtyardPose::Standing:ECourtyardPose::Supine);return;}
    if(Pose==ECourtyardPose::Supine) return;
    if(A==(int32)ECourtyardAction::Crouch) TryPose(Pose==ECourtyardPose::Crouched?ECourtyardPose::Standing:ECourtyardPose::Crouched);
    if(A==(int32)ECourtyardAction::Prone) TryPose(Pose==ECourtyardPose::Prone?ECourtyardPose::Standing:ECourtyardPose::Prone);
    if(A==(int32)ECourtyardAction::Jump && Pose==ECourtyardPose::Standing) PlayerCharacter->Jump();
}

bool ACourtyardController::LowBodySupported(FVector Feet,float Yaw) const
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardSupport),false,PlayerCharacter);
    const FRotator R(0,Yaw,0);
    for(float X:{-65.f,0.f,65.f}) for(float Y:{-18.f,18.f})
    {
        FVector P=Feet+R.RotateVector(FVector(X,Y,0)); FHitResult H;
        if(!GetWorld()->LineTraceSingleByChannel(H,P+FVector(0,0,22),P-FVector(0,0,12),ECC_Visibility,Q) || H.ImpactNormal.Z<.92f || FMath::Abs(H.ImpactPoint.Z-Feet.Z)>9.f) return false;
    }
    return true;
}
bool ACourtyardController::HasPoseSpace(ECourtyardPose P,FVector Feet,float Yaw) const
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardPoseSpace),false,PlayerCharacter);
    const float H=HalfHeight(P);
    if(GetWorld()->OverlapBlockingTestByChannel(Feet+FVector(0,0,H+2),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(23.f,H-.5f),Q))return false;
    if(Low(P))
    {
        if(!LowBodySupported(Feet,Yaw))return false;
        if(GetWorld()->OverlapBlockingTestByChannel(Feet+FVector(0,0,25),FRotator(0,Yaw,0).Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(80,24,22)),Q))return false;
    }
    return true;
}
bool ACourtyardController::TryPose(ECourtyardPose P)
{
    if(!PlayerCharacter || !ViewCamera || !PlayerCharacter->GetCharacterMovement()->IsMovingOnGround())return false;
    auto* Cap=PlayerCharacter->GetCapsuleComponent(); const FVector Feet=PlayerCharacter->GetActorLocation()-FVector(0,0,Cap->GetScaledCapsuleHalfHeight());
    const float Yaw=Low(P)?GetControlRotation().Yaw:BodyYaw;
    if(!HasPoseSpace(P,Feet,Yaw)) {SetNotice(TEXT("这里的空间或地面支撑不足"));return false;}
    HeightStart=Cap->GetUnscaledCapsuleHalfHeight(); CameraStart=ViewCamera->GetRelativeLocation(); LookStart=GetControlRotation();
    if(P==ECourtyardPose::Supine) LookBeforeSupine=LookStart;
    TargetPose=P;BodyYaw=Yaw;PoseTime=0;PoseDuration=Low(P)||Low(Pose)?.65f:.4f;PoseTransition=true;
    PlayerCharacter->bUseControllerRotationYaw=!Low(P);
    PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();Held.Empty();return true;
}
void ACourtyardController::TickPose(float Dt)
{
    if(!PoseTransition)return;
    auto* Cap=PlayerCharacter->GetCapsuleComponent();auto* Move=PlayerCharacter->GetCharacterMovement();
    const FVector Feet=PlayerCharacter->GetActorLocation()-FVector(0,0,Cap->GetScaledCapsuleHalfHeight());
    if(!HasPoseSpace(TargetPose,Feet,BodyYaw)) {TargetPose=Pose;HeightStart=Cap->GetUnscaledCapsuleHalfHeight();CameraStart=ViewCamera->GetRelativeLocation();PoseTime=0;SetNotice(TEXT("姿态空间受到阻挡，正在恢复"));}
    PoseTime+=Dt;const float T=FMath::Clamp(PoseTime/PoseDuration,0.f,1.f);const float A=T*T*(3-2*T);
    const float H=FMath::Lerp(HeightStart,HalfHeight(TargetPose),A);const float OldH=Cap->GetUnscaledCapsuleHalfHeight();
    if(H<=OldH)Cap->SetCapsuleHalfHeight(H,true);
    FHitResult Hit;PlayerCharacter->SetActorLocation(PlayerCharacter->GetActorLocation()+FVector(0,0,H-OldH),true,&Hit);
    if(H>OldH)Cap->SetCapsuleHalfHeight(H,true);
    FVector End=OriginalCamera;
    if(TargetPose==ECourtyardPose::Crouched)End=FVector(0,0,41);
    if(TargetPose==ECourtyardPose::Prone)End=FRotator(0,BodyYaw-PlayerCharacter->GetActorRotation().Yaw,0).RotateVector(FVector(50,0,8));
    if(TargetPose==ECourtyardPose::Supine)End=FRotator(0,BodyYaw-PlayerCharacter->GetActorRotation().Yaw,0).RotateVector(FVector(-50,0,8));
    ViewCamera->SetRelativeLocation(FMath::Lerp(CameraStart,End,A));
    if(TargetPose==ECourtyardPose::Supine)SetControlRotation(FRotator(FMath::Lerp(LookStart.Pitch,68.f,A),LookStart.Yaw,0));
    else if(Pose==ECourtyardPose::Supine)SetControlRotation(FMath::Lerp(LookStart,LookBeforeSupine,A));
    Move->Velocity=FVector::ZeroVector;
    if(T>=1){Pose=TargetPose;PoseTransition=false;UE_LOG(LogTemp,Display,TEXT("COURTYARD_POSE %d height=%.1f"),(int32)Pose,H);}
}
bool ACourtyardController::CanCrawl(FVector Delta) const
{
    const FVector Feet=PlayerCharacter->GetActorLocation()-FVector(0,0,24);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtyardCrawl),false,PlayerCharacter);FHitResult H;
    if(GetWorld()->SweepSingleByChannel(H,Feet+FVector(0,0,25),Feet+FVector(0,0,25)+Delta,FRotator(0,BodyYaw,0).Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(80,24,22)),Q))return false;
    return LowBodySupported(Feet+Delta,BodyYaw);
}
void ACourtyardController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    if(!Initialized)InitializePlayer();if(!Initialized)return;
    TickMood(Dt);
    const bool ApproachOnly=IsApproachPrototypeWorld();
    if(!ApproachOnly){
    TickWelcome();
    }
    TickFaceShading();
    TickLivelyFace(Dt);
    if(!ApproachOnly){
    TickCompanion();
    TickLively(Dt);
    TickLiving(Dt);
    TickLivelyEnvironment(Dt);
    TickBodyBehavior();
    TickAmbientBehavior();
    }else TickApproachPrototype();
    if(LocoOn)TickLocomotion(Dt);
    TickGaze(Dt);
    if(Page==ECourtyardPage::DisplayConfirm && FPlatformTime::Seconds()>=ConfirmUntil)ConfirmDisplay(false);
    if(Page==ECourtyardPage::Settings && Held.Contains(EKeys::LeftMouseButton)){float X,Y;if(GetMousePosition(X,Y))if(auto* H=Cast<ACourtyardHUD>(GetHUD()))H->Drag(X);}
    if(IsSeatBusy())return;
    if(BPBool(PlayerCharacter,TEXT("Seated"))) Pose=ECourtyardPose::Seated;
    else if(Pose==ECourtyardPose::Seated)Pose=ECourtyardPose::Standing;
    TickPose(Dt);
    auto* Move=PlayerCharacter->GetCharacterMovement();
    Move->MaxWalkSpeed=Pose==ECourtyardPose::Prone?35.f:Pose==ECourtyardPose::Crouched?70.f:Held.Contains(Applied.Keys[(int32)ECourtyardAction::Sprint])?280.f:120.f;
    if(Page==ECourtyardPage::Game && !PoseTransition && Pose!=ECourtyardPose::Supine && Pose!=ECourtyardPose::Seated && !IsMoveInputIgnored())
    {
        const float Forward=(Held.Contains(Applied.Keys[0])?1.f:0.f)-(Held.Contains(Applied.Keys[2])?1.f:0.f);
        const float Right=(Held.Contains(Applied.Keys[3])?1.f:0.f)-(Held.Contains(Applied.Keys[1])?1.f:0.f);
        const FRotator R(0,GetControlRotation().Yaw,0);FVector Direction=R.RotateVector(FVector(Forward,Right,0)).GetClampedToMaxSize(1);
        if(Pose==ECourtyardPose::Prone && !CanCrawl(Direction*(Move->MaxWalkSpeed*FMath::Max(Dt,.016f)+4)+Move->Velocity*Dt)) {Direction=FVector::ZeroVector;Move->StopMovementImmediately();}
        PlayerCharacter->AddMovementInput(Direction,1.f);
    }
    if(FPlatformTime::Seconds()>NextAudioScan){NextAudioScan=FPlatformTime::Seconds()+1;UpdateAudio();}
}
void ACourtyardController::ApplyRuntimePreferences()
{
    if(ViewCamera)ViewCamera->SetFieldOfView(Applied.FOV);
    if(MusicManager && BPBool(MusicManager,TEXT("Enabled"))!=Applied.MusicEnabled)WriteBool(MusicManager,TEXT("Enabled"),Applied.MusicEnabled);
    UpdateAudio();
}
void ACourtyardController::UpdateAudio()
{
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TInlineComponentArray<UAudioComponent*> Sounds(*It);
        for(UAudioComponent* A:Sounds)
        {
            if(*It==MusicManager) {A->SetVolumeMultiplier(Applied.Master*Applied.Music);continue;}
            if(!EnvironmentBase.Contains(A))EnvironmentBase.Add(A,A->VolumeMultiplier);
            A->SetVolumeMultiplier(EnvironmentBase[A]*Applied.Master*Applied.Environment*(A==P3WindAudio.Get()?1+.4f*P3CurrentGust:1));
        }
    }
}
void ACourtyardController::ApplyDraft()
{
    Draft.Sanitize();BeforeTrial=Applied;
    if(Draft.DisplayDiffers(Applied))
    {
        Applied=Draft;Applied.ApplyEngine(true);ApplyRuntimePreferences();ConfirmUntil=FPlatformTime::Seconds()+15;
        OpenPage(ECourtyardPage::DisplayConfirm);
    }
    else {Applied=Draft;Applied.ApplyEngine(false);ApplyRuntimePreferences();if(!UCourtyardPreferencesSave::Store(Applied))SetNotice(TEXT("保存失败，请检查磁盘空间"));else SetNotice(TEXT("设置已应用并保存"));}
}
void ACourtyardController::ConfirmDisplay(bool Keep)
{
    if(Keep) {GEngine->GetGameUserSettings()->ConfirmVideoMode();if(!UCourtyardPreferencesSave::Store(Applied))SetNotice(TEXT("设置保存失败"));}
    else {Applied=BeforeTrial;Applied.ApplyEngine(true);ApplyRuntimePreferences();SetNotice(TEXT("已恢复上一次显示设置"));}
    OpenPage(ECourtyardPage::Settings);
}
void ACourtyardController::ClickUI(float X,float Y) {if(auto* H=Cast<ACourtyardHUD>(GetHUD()))H->Click(X,Y);}

void ACourtyardController::CourtyardReviewKey(FString Key,bool Down)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    InputKey(FInputKeyEventArgs::CreateSimulated(FKey(*Key),Down?IE_Pressed:IE_Released,Down?1.f:0.f));
}
void ACourtyardController::CourtyardReviewLook(float Yaw,float Pitch)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || Page!=ECourtyardPage::Game)return;
    SetControlRotation(FRotator(FMath::Clamp(Pitch,-80.f,85.f),Yaw,0));
}
void ACourtyardController::CourtyardReviewState(FString Tag)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || !Initialized)return;
    TSharedRef<FJsonObject> J=MakeShared<FJsonObject>();
    AddMoodReview(J);
    J->SetNumberField(TEXT("facePeakChannels"),FacePeakChannels);J->SetNumberField(TEXT("faceOverrideKeys"),FaceOwnedValues.Num());
    J->SetNumberField(TEXT("bodyOverlapEpisodes"),ReviewOverlapEpisodes);J->SetNumberField(TEXT("longestNearIdleSeconds"),ReviewMaxNearIdle);J->SetStringField(TEXT("activeMontages"),ReviewSlotNames);
    J->SetStringField(TEXT("livelyBody"),LivelyBodyId.ToString());J->SetStringField(TEXT("livelyFace"),LivelyFaceId.ToString());
    J->SetBoolField(TEXT("p2Visible"),ViewCamera && VisibleTarget(ViewCamera->GetComponentLocation(),PlayerCharacter));J->SetNumberField(TEXT("p2GazeSince"),P2GazeSince);J->SetBoolField(TEXT("p2SkyOpen"),LivelySkyOpen());
    J->SetNumberField(TEXT("emmaDistance"),Emma()?FVector::Dist(PlayerCharacter->GetActorLocation(),Emma()->GetActorLocation()):-1);
    J->SetStringField(TEXT("tag"),Tag);J->SetNumberField(TEXT("time"),GetWorld()->GetTimeSeconds());
    J->SetNumberField(TEXT("pose"),(int32)Pose);J->SetNumberField(TEXT("page"),(int32)Page);J->SetBoolField(TEXT("transition"),PoseTransition);
    J->SetBoolField(TEXT("seated"),BPBool(PlayerCharacter,TEXT("Seated")));J->SetBoolField(TEXT("seatTransition"),IsSeatBusy());
    J->SetNumberField(TEXT("livingBehavior"),LivingBehavior);J->SetBoolField(TEXT("readingTrip"),LivingTrip);J->SetNumberField(TEXT("readingSegment"),ReadingSegment);J->SetBoolField(TEXT("readingPaused"),ReadingPaused);J->SetBoolField(TEXT("highFiveReady"),CanHighFiveReply());
    J->SetBoolField(TEXT("canE"),CanRespond());J->SetBoolField(TEXT("canF"),CanSeat());J->SetBoolField(TEXT("music"),BPBool(MusicManager,TEXT("Enabled")));
    J->SetNumberField(TEXT("responseVariant"),LastResponse);J->SetBoolField(TEXT("emmaTravel"),BPBool(Emma(),TEXT("TravelActive")));J->SetBoolField(TEXT("emmaAllow"),BPBool(Emma(),TEXT("AllowInteraction")));
    J->SetNumberField(TEXT("ambientBehavior"),AmbientBehavior);
    J->SetNumberField(TEXT("bodyBehavior"),BodyBehavior);
    J->SetBoolField(TEXT("watching"),PlayerWatching);J->SetNumberField(TEXT("lookUpSince"),LookUpSince);
    // Read-only review evidence: these curves belong to the montage, not a second facial writer.
    if(auto* Subject=Cast<ACharacter>(Emma()))if(auto* Mesh=Subject->GetMesh())if(auto* Anim=Mesh->GetAnimInstance()){
        auto Face=MakeShared<FJsonObject>();
        for(const TCHAR* Name:{TEXT("まばたき"),TEXT("笑い"),TEXT("口角上げ"),TEXT("あ")})Face->SetNumberField(Name,Anim->GetCurveValue(FName(Name)));
        J->SetObjectField(TEXT("gestureFaceCurves"),Face);
    }
    float CY=0;J->SetBoolField(TEXT("companionContext"),CompanionContext(CY));J->SetNumberField(TEXT("companionYaw"),CY);
    J->SetNumberField(TEXT("flowerContext"),FlowerContext());J->SetNumberField(TEXT("flowerComponents"),FlowerComponents.Num());J->SetBoolField(TEXT("restPending"),RestPending);
    J->SetBoolField(TEXT("greetingArmed"),GreetingArmed);J->SetNumberField(TEXT("arrivalTime"),ArrivalTime);J->SetNumberField(TEXT("attentionSince"),AttentionSince);
    const FVector P=PlayerCharacter->GetActorLocation();J->SetNumberField(TEXT("x"),P.X);J->SetNumberField(TEXT("y"),P.Y);J->SetNumberField(TEXT("z"),P.Z);
    if(Tag.StartsWith(TEXT("Grand"))){
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(ManorPassageReview),false,PlayerCharacter);
        auto* Cap=PlayerCharacter->GetCapsuleComponent();FVector Start=P+FVector(0,0,1);
        const FVector End=Start+FRotator(0,GetControlRotation().Yaw,0).Vector()*80;
        if(GetWorld()->SweepSingleByProfile(Hit,Start,End,FQuat::Identity,TEXT("Pawn"),FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius()-.5f,Cap->GetScaledCapsuleHalfHeight()-1),Query)){
            J->SetStringField(TEXT("aheadActor"),GetNameSafe(Hit.GetActor()));J->SetStringField(TEXT("aheadComponent"),GetNameSafe(Hit.GetComponent()));
            J->SetStringField(TEXT("aheadPoint"),Hit.ImpactPoint.ToString());
        }
    }
    J->SetNumberField(TEXT("halfHeight"),PlayerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
    J->SetNumberField(TEXT("speed"),PlayerCharacter->GetVelocity().Size());J->SetNumberField(TEXT("fov"),ViewCamera->FieldOfView);
    J->SetNumberField(TEXT("yaw"),GetControlRotation().Yaw);J->SetNumberField(TEXT("pitch"),GetControlRotation().Pitch);
    J->SetStringField(TEXT("notice"),Notice);J->SetNumberField(TEXT("capture"),CaptureAction);
    J->SetStringField(TEXT("mapKey"),Applied.Keys[11].ToString());J->SetStringField(TEXT("draftMapKey"),Draft.Keys[11].ToString());
    J->SetNumberField(TEXT("sensitivity"),Applied.Sensitivity);J->SetNumberField(TEXT("gamma"),GEngine->DisplayGamma);
    J->SetNumberField(TEXT("master"),Applied.Master);J->SetNumberField(TEXT("environment"),Applied.Environment);J->SetNumberField(TEXT("musicGain"),Applied.Music);
    for(const TCHAR* Name:{TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("sg.ShadowQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage"),TEXT("r.Streaming.PoolSize")})
        if(auto* Var=IConsoleManager::Get().FindConsoleVariable(Name))J->SetNumberField(Name,Var->GetFloat());
    TArray<TSharedPtr<FJsonValue>> Audio;
    for(TActorIterator<AActor> It(GetWorld());It;++It){TInlineComponentArray<UAudioComponent*> Sounds(*It);for(UAudioComponent* A:Sounds){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("actor"),It->GetName());O->SetNumberField(TEXT("gain"),A->VolumeMultiplier);O->SetBoolField(TEXT("playing"),A->IsPlaying());Audio.Add(MakeShared<FJsonValueObject>(O));}}
    J->SetArrayField(TEXT("audio"),Audio);
    FVector Eye;FRotator Rot;GetPlayerViewPoint(Eye,Rot);J->SetNumberField(TEXT("eyeZ"),Eye.Z);
    if(auto* V=GetWorld()->GetGameViewport())if(V->Viewport){J->SetNumberField(TEXT("width"),V->Viewport->GetSizeXY().X);J->SetNumberField(TEXT("height"),V->Viewport->GetSizeXY().Y);J->SetNumberField(TEXT("mode"),(int32)V->Viewport->GetWindowMode());}
    FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(J,Writer);
    Text.ReplaceInline(TEXT("\n"),TEXT(""));Text.ReplaceInline(TEXT("\r"),TEXT(""));UE_LOG(LogTemp,Display,TEXT("COURTYARD_REVIEW %s"),*Text);
}

void ACourtyardController::CourtyardReviewClick(float X,float Y)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || Page==ECourtyardPage::Game)return;
    int32 W,H;GetViewportSize(W,H);const float S=FMath::Min(W/1280.f,H/800.f);
    ClickUI((W-1280*S)*.5f+X*S,(H-800*S)*.5f+Y*S);
}

void ACourtyardController::CourtyardReviewCeiling(bool Enabled)
{
    // One temporary, visible low-clearance fixture. Never saved or enabled in normal play.
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("CourtyardReviewCeiling")))It->Destroy();
    if(!Enabled)return;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(!Mesh)return;
    for(int32 I=0;I<3;++I)
    {
        auto* A=GetWorld()->SpawnActor<AStaticMeshActor>();A->Tags.Add(TEXT("CourtyardReviewCeiling"));
        auto* S=A->GetStaticMeshComponent();S->SetMobility(EComponentMobility::Movable);S->SetStaticMesh(Mesh);S->SetCollisionProfileName(TEXT("BlockAll"));
        A->SetActorLocation(I==0?FVector(-265,-480,81):FVector(I==1?-375:-155,-480,38));
        A->SetActorScale3D(I==0?FVector(2.4,1.4,.1):FVector(.1,1.4,.76));
    }
}

void ACourtyardController::ActivateSetting(int32 Row,float V,bool Absolute)
{
    const int32 Step=V<0?-1:1;
    switch(Row)
    {
        case 0:Draft.Master=V;break;
        case 1:Draft.Music=V;break;
        case 2:Draft.Environment=V;break;
        case 3:Draft.MusicEnabled=!Draft.MusicEnabled;break;
        case 4:Draft.Sensitivity=.2f+V*2.8f;break;
        case 5:Draft.FOV=60+V*40;break;
        case 6:Draft.WindowMode=(Draft.WindowMode+Step+3)%3;break;
        case 7:
        {
            if(Draft.WindowMode==1)break;
            TArray<FIntPoint> Resolutions;UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
            Resolutions.RemoveAll([](FIntPoint R){return R.X<800 || R.Y<600;});
            Resolutions.Sort([](const FIntPoint& A,const FIntPoint& B){return A.X==B.X?A.Y<B.Y:A.X<B.X;});
            if(Resolutions.Num())
            {
                const FIntPoint Current=Draft.Resolution==FIntPoint::ZeroValue?GEngine->GetGameUserSettings()->GetDesktopResolution():Draft.Resolution;
                const int32 At=Resolutions.IndexOfByKey(Current);
                Draft.Resolution=Resolutions[(FMath::Max(0,At)+Step+Resolutions.Num())%Resolutions.Num()];
            }
            else SetNotice(TEXT("当前显示设备未报告可用分辨率"));
            break;
        }
        case 8:Draft.VSync=!Draft.VSync;break;
        case 9:{const TArray<int32> Caps={30,60,120,0};Draft.FrameLimit=Caps[(Caps.IndexOfByKey(Draft.FrameLimit)+Step+4)%4];break;}
        case 10:Draft.SetPreset((FMath::Max(0,Draft.Preset)+Step+4)%4);break;
        case 11:Draft.Shadows=(Draft.Shadows+Step+4)%4;Draft.Preset=-1;break;
        case 12:Draft.AntiAliasing=(Draft.AntiAliasing+Step+4)%4;Draft.Preset=-1;break;
        case 13:Draft.Post=(Draft.Post+Step+4)%4;Draft.Preset=-1;break;
        case 14:Draft.Foliage=(Draft.Foliage+Step+4)%4;Draft.Preset=-1;break;
        case 15:Draft.Gamma=1.8f+V*.8f;break;
    }
}

