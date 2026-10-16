#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

void ACourtyardController::CourtyardReviewBodyView(int32 Angle){
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    if(Angle==0){SetViewTargetWithBlend(GetPawn(),0);return;}
    if(Angle>=25 && Angle<=30){
        const FVector P[]={FVector(0,2005,378),FVector(695,1505,155),FVector(-650,1550,155),FVector(-640,1745,601),FVector(660,1435,601),FVector(75,1460,601)};
        const FVector T[]={FVector(0,1810,330),FVector(890,1760,105),FVector(-880,1790,110),FVector(-925,1935,530),FVector(855,1640,540),FVector(-30,1850,385)};
        if(!BodyReviewCamera)BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
        BodyReviewCamera->SetActorLocationAndRotation(P[Angle-25],(T[Angle-25]-P[Angle-25]).Rotation());
        BodyReviewCamera->GetCameraComponent()->SetFieldOfView(78);SetViewTargetWithBlend(BodyReviewCamera,0);return;
    }
    if(Angle>=14 && Angle<=24){
        const FVector P[]={FVector(0,-1900,155),FVector(-1700,-300,155),FVector(1700,-300,155),FVector(-630,500,155),FVector(-3200,-1700,1080),FVector(3200,-1700,1080),FVector(0,-3800,1080),FVector(-650,520,601),FVector(640,520,601),FVector(0,1430,155),FVector(-350,1300,601)};
        const FVector T[]={FVector(0,400,980),FVector(0,850,780),FVector(0,850,780),FVector(-930,940,140),FVector(0,1240,1040),FVector(0,1240,1040),FVector(0,500,1040),FVector(-885,860,550),FVector(900,890,540),FVector(0,1890,330),FVector(300,900,570)};
        if(!BodyReviewCamera)BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
        BodyReviewCamera->SetActorLocationAndRotation(P[Angle-14],(T[Angle-14]-P[Angle-14]).Rotation());
        BodyReviewCamera->GetCameraComponent()->SetFieldOfView(Angle>=18&&Angle<=20?65:78);SetViewTargetWithBlend(BodyReviewCamera,0);return;
    }
      if(Angle>=10 && Angle<=13){
          const FVector Locations[]={FVector(0,-1330,154),FVector(530,-130,155),FVector(-75,640,148),FVector(-55,80,150)};
          const FVector Targets[]={FVector(0,340,550),FVector(0,320,420),FVector(-350,875,110),FVector(65,215,196)};
          if(!BodyReviewCamera)BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
          BodyReviewCamera->SetActorLocationAndRotation(Locations[Angle-10],(Targets[Angle-10]-Locations[Angle-10]).Rotation());
          BodyReviewCamera->GetCameraComponent()->SetFieldOfView(70);SetViewTargetWithBlend(BodyReviewCamera,0);return;
      }
    AActor* A=Emma();if(!A)return;
    auto* M=A->FindComponentByClass<USkeletalMeshComponent>();if(!M)return;
    FVector F=(M->GetSocketLocation(TEXT("目_L"))+M->GetSocketLocation(TEXT("目_R")))*.5-M->GetSocketLocation(TEXT("頭"));F.Z=0;F.Normalize();
    FVector R=M->GetSocketLocation(TEXT("腕_R"))-M->GetSocketLocation(TEXT("腕_L"));R.Z=0;R.Normalize();
    FVector Focus=M->GetSocketLocation(TEXT("上半身2"))+FVector(0,0,10);
    FVector Delta=F*195;
    if(Angle==2)Delta=F*35+R*200;
    if(Angle==3)Delta=F*150+R*145;
    if(Angle==4){Delta=F*170+R*230;Focus.Z-=37;}
    if(Angle==5){Focus=M->GetSocketLocation(TEXT("手首_R"));Delta=F*65+R*38;}
    if(Angle==6){Focus=M->GetComponentLocation()+FVector(0,0,76);Delta=F*265-R*130;}
    FVector Pos=Focus+Delta+FVector(0,0,7);
    if(!BodyReviewCamera)BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    if(!BodyReviewCamera)return;
    BodyReviewCamera->SetActorLocationAndRotation(Pos,(Focus-Pos).Rotation());
    BodyReviewCamera->GetCameraComponent()->SetFieldOfView(65);
    SetViewTargetWithBlend(BodyReviewCamera,0);
}
void ACourtyardController::CourtyardReviewBodyRate(float Rate){
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    if(AActor* A=Emma())if(auto* M=A->FindComponentByClass<USkeletalMeshComponent>())M->GlobalAnimRateScale=FMath::Clamp(Rate,.5f,1.f);
}

namespace {
double ReadBodyNumber(UObject* A,FName N,double Default=0){if(A)if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N))if(P->IsFloatingPoint())return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A));return Default;}
void BodyAllow(UObject* A,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("AllowInteraction")))P->SetPropertyValue_InContainer(A,V);}
float Ease(float A){A=FMath::Clamp(A,0.f,1.f);return A*A*A*(A*(A*6-15)+10);}
const TCHAR* BodyNames[]={TEXT("Wave"),TEXT("Stretch"),TEXT("Yawn"),TEXT("Read"),TEXT("Tea"),TEXT("Flower"),TEXT("FoldArms"),TEXT("Hair"),TEXT("Clap"),TEXT("Bow")};
}
void ACourtyardController::InitializeBodyProps(){
    // The close prop station conflicts with the proven seat/turn footprint.
    // Keep it exclusively in the opt-in review lab; no prop or collision
    // workaround is spawned in the delivered garden.
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || !FParse::Param(FCommandLine::Get(),TEXT("CourtyardBodyPropLab")))return;
    auto* A=Cast<ACharacter>(Emma());if(!A || !A->GetMesh() || BodyPropMeshes.Num()!=4)return;
    for(auto M:BodyPropMeshes)if(!M)return;
    BodyStation=A->GetMesh()->GetComponentTransform();
    for(int32 I=0;I<4;++I){
        auto* Prop=GetWorld()->SpawnActor<AStaticMeshActor>();Prop->SetMobility(EComponentMobility::Movable);
        Prop->GetStaticMeshComponent()->SetStaticMesh(BodyPropMeshes[I]);
        Prop->GetStaticMeshComponent()->SetCollisionProfileName(I==0?TEXT("BlockAllDynamic"):TEXT("NoCollision"));
        Prop->Tags.Add(TEXT("EmmaBodyProp"));BodyProps.Add(Prop);
    }
    BodyProps[0]->SetActorTransform(FTransform(BodyStation.GetRotation(),BodyStation.TransformPosition(FVector(46.4,7,0))));
    BodyStationReady=true;UpdateBodyProps();
}
void ACourtyardController::UpdateBodyProps(){
    if(!BodyStationReady || BodyProps.Num()!=4)return;
    const FVector Rest[]={FVector(47.7,.13,110),FVector(39.7,11.13,109),FVector(35.7,-8.87,109.5)};
    auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;
    const float T=GetWorld()->GetTimeSeconds()-BodyStarted;
    for(int32 I=1;I<4;++I){
        FVector P=BodyStation.TransformPosition(Rest[I-1]);FQuat Q=BodyStation.GetRotation();
        if(I==2)Q=Q*FQuat(FVector::UpVector,PI);
        if(I==3)Q=Q*FQuat(FVector::RightVector,PI*.5f);
        const int32 V=I+2;
        if(M && BodyBehavior==V && T>1.05f && T<6.8f){
            const float Lift=Ease((T-1.25f)/1.2f)*(1-Ease((T-5.4f)/1.05f));
            const float Carry=Ease((T-1.05f)/.3f)*(1-Ease((T-6.4f)/.35f));
            FVector W=M->GetSocketLocation(TEXT("手首_L"));FVector Offset;
            FQuat HeldQ=BodyStation.GetRotation();
            if(I==1){const float Angle=PI*Lift;Offset=FVector(9*FMath::Cos(Angle),9*FMath::Sin(Angle),0);HeldQ=HeldQ*FQuat(FVector::UpVector,-Angle);}
            else if(I==2){Offset=FVector(-7*Lift,-4+2*Lift,-6);HeldQ=HeldQ*FQuat(FVector::UpVector,PI-PI*.5f*Lift)*FQuat(FVector::ForwardVector,-.12f*Ease((T-2.7f)/.5f)*(1-Ease((T-4.5f)/.5f)));}
            else {Offset=FVector(0,-2*Lift,FMath::Lerp(-1.5f,7.f,Lift));HeldQ=HeldQ*FQuat(FVector::RightVector,PI*.5f*(1-Lift));}
            P=FMath::Lerp(P,W+M->GetComponentTransform().TransformVectorNoScale(Offset),Carry);Q=FQuat::Slerp(Q,HeldQ,Carry);
        }
        BodyProps[I]->SetActorLocationAndRotation(P,Q);
    }
}
bool ACourtyardController::StartBody(int32 V,bool Review){
    if(ContentLayer.IsValid() && ContentLayer->BlocksOrdinaryBehavior())return false;
    // Prop actions deferred: station clearance failed; tea contact also failed.
    // Preserve all three independent candidates without affecting normal play.
    if(V>=3 && V<=5 && !Review)return false;
    // Removed bow stays out of play, including forced review commands.
    // Preserve numeric IDs so saved diagnostics do not invoke another gesture.
    if(V==9)return false; // User removed bow; FK stretch/fold are separately authored replacements.
    const double Now=GetWorld()->GetTimeSeconds();auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;
    if(LivingTrip || V<0 || V>=10 || !I || !BodyMotions.IsValidIndex(V) || !BodyMotions[V] || ActiveAmbient || I->IsAnyMontagePlaying()
        || Page!=ECourtyardPage::Game || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction"))
        || !QueryBool(A,TEXT("CanHeadPat")) || A->GetVelocity().Size()>2 || ReadBodyNumber(A,TEXT("DueTime"),Now)-Now<9)return false;
    if(!Review && (Now<BodyCooldown[V] || Now-AmbientStillSince<5 || Now-ReadBodyNumber(A,TEXT("LastGlanceTime"),-100)<10))return false;
    if(V>=3 && V<=5){
        if(!BodyStationReady || FVector::Dist2D(A->GetMesh()->GetComponentLocation(),BodyStation.GetLocation())>8
            || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetMesh()->GetComponentRotation().Yaw,BodyStation.Rotator().Yaw))>5)return false;
    }
    float Y=0;const bool Face=CompanionContext(Y);
    if(!Review){
        if((V==0 || V==8) && (!GreetingArmed || !Face || FMath::Abs(Y)>24))return false;
        if(V==2 && Now-AmbientStillSince<25)return false;
    }
    BodyAllow(A,false);
    UAnimMontage* Motion=LivelyBodyVariant(V,BodyMotions[V],Review);
    if(I->Montage_Play(Motion,1.f,EMontagePlayReturnType::MontageLength,0.f,false)<=0){BodyAllow(A,true);return false;}
    BodyBehavior=V;BodyStarted=Now;ActiveAmbient=Motion;AmbientActor=A;AmbientBehavior=20+V;LivelyBodyBegan(V);
    LastBody=V;BodyCooldown[V]=Now+FMath::FRandRange(130.f,220.f);
    if(V==0 || V==8){GreetingArmed=false;GreetingArrival=-1;NextGreeting=Now+100;}
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_BODY begin id=%d name=%s review=%d seen=%d time=%.2f"),V,BodyNames[V],Review,PlayerWatching,Now);return true;
}
void ACourtyardController::TickBodyBehavior(){
    const double Now=GetWorld()->GetTimeSeconds();UpdateBodyProps();
    if(BodyBehavior>=0){
        if(!ActiveAmbient){UE_LOG(LogTemp,Display,TEXT("COURTYARD_BODY end id=%d time=%.2f"),BodyBehavior,Now);BodyBehavior=-1;NextBody=Now+(LivelyEnabled?MoodBodyGap():FMath::FRandRange(32.f,60.f));UpdateBodyProps();}
        return;
    }
    if(ReviewBodyHold)return;
    // Event responses should not lose their brief eligibility window to the idle timer.
    if(!ActiveAmbient && Now>=NextAmbientCheck && Page==ECourtyardPage::Game){
        if(GreetingArmed){const int32 V=FMath::FRand()<.75f?0:8;if(StartBody(V))return;}
    }
    if(Now<NextBody)return;
    if(ActiveAmbient || Page!=ECourtyardPage::Game){NextBody=Now+2;return;}
    // A stretch or a yawn is part of her life only if someone can see it.
    if(!SeenOrWaited(BodyDefer)){NextBody=Now+3;return;}
    // While the player looks at the sky or crouches, her reaction to them comes before a random gesture.
    if(LookUpSince>=0 || Pose==ECourtyardPose::Crouched || Pose==ECourtyardPose::Prone){NextBody=Now+3;return;}
    TArray<int32> Bag={0,1,2,6,7,8};
    while(Bag.Num()){const int32 Pick=FMath::RandHelper(Bag.Num()),V=Bag[Pick];Bag.RemoveAt(Pick);if(V!=LastBody && StartBody(V)){BodyDefer=-1;return;}}
    NextBody=Now+FMath::FRandRange(4.f,7.f);
}
void ACourtyardController::CourtyardReviewBody(int32 V){
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_BODY review id=%d accepted=%d"),V,StartBody(V,true));
}
void ACourtyardController::CourtyardReviewBodySetup(bool Enabled){
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    auto* A=Emma();if(!A || BPBool(A,TEXT("TravelActive")) || ActiveAmbient)return;
    for(const TCHAR* N:{TEXT("ActivityEnabled"),TEXT("GlanceEnabled")})if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,!Enabled);
    if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),TEXT("DueTime")))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),GetWorld()->GetTimeSeconds()+(Enabled?10000:4));
    NextBody=NextAmbient=NextCompanion=NextLiving=NextReading=GetWorld()->GetTimeSeconds()+(Enabled?10000:25);
    ReviewBodyHold=Enabled;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_BODY showcase=%d default_waits_unchanged=1 one_departure_shortened_on_release=1"),Enabled);
}
