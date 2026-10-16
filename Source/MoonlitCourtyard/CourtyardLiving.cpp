#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "LivingRouteData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace {
void LivingBool(UObject* A,FName N,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,V);}
double LivingNumber(UObject* A,FName N,double Default=0){if(A)if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N)){auto* V=P->ContainerPtrToValuePtr<void>(A);return P->IsFloatingPoint()?P->GetFloatingPointPropertyValue(V):P->GetSignedIntPropertyValue(V);}return Default;}
void LivingTime(UObject* A,FName N,double V){if(A)if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N))if(P->IsFloatingPoint())P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),V);}
float Soft(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);}
FVector Palm(USkeletalMeshComponent* M,const TCHAR* Side){
 const FVector W=M->GetSocketLocation(*FString::Printf(TEXT("手首_%s"),Side));
 const FVector I=M->GetSocketLocation(*FString::Printf(TEXT("人指１_%s"),Side));
 const FVector K=M->GetSocketLocation(*FString::Printf(TEXT("小指１_%s"),Side));
 FVector N=FVector::CrossProduct(I-W,K-W).GetSafeNormal();if(N.Z<0)N=-N;
 return W*.4f+(I+K)*.3f+N*1.5f;
}
const TCHAR* Names[]={TEXT("CatchPetal"),TEXT("Chime"),TEXT("TraceStars"),TEXT("HighFive"),TEXT("BlowFeather"),TEXT("BookmarkRead"),TEXT("HighFiveReply")};
FString StationChain(int32 Purpose,bool Return){
 if(Purpose==0)return Return?TEXT("USSSUAASSSSSSASSAUA"):TEXT("USSSSASSSSSUASASAS");
 if(Purpose==5)return Return?TEXT("AAASAUASASSAAA"):TEXT("UAUSAUASASUASSSA");
 return Return?TEXT("USSUAUA"):TEXT("UAUSS");
}
}

void ACourtyardController::InitializeLiving(){
 if(LivingInitialized || !Emma())return;
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  if(It->ActorHasTag(TEXT("LivingChime"))){LivingChime=*It;ChimeRest=It->GetActorTransform();}
  if(It->ActorHasTag(TEXT("LivingCherryTree")))LivingTree=*It;
  if(It->ActorHasTag(TEXT("LivingAncientTree")))AncientTree=*It;
  if(It->ActorHasTag(TEXT("LivingAncientAnchor")))AncientAnchor=*It;
  if(It->ActorHasTag(TEXT("LivingReadingAnchor")))ReadingAnchor=*It;
  if(It->ActorHasTag(TEXT("LivingChimeAnchor")))ChimeAnchor=*It;
  if(It->ActorHasTag(TEXT("LivingReadingBook"))){ReadingBook=*It;BookRest=It->GetActorTransform();}
  if(It->ActorHasTag(TEXT("LivingReadingBookmark"))){ReadingBookmark=*It;BookmarkRest=It->GetActorTransform();if(auto* S=Cast<AStaticMeshActor>(*It))S->SetMobility(EComponentMobility::Movable);}
 }
 // Missing scene dependencies keep the stable map behavior unchanged.
 if(!LivingTree.IsValid() || !LivingChime.IsValid() || !ReadingAnchor.IsValid() || !ChimeAnchor.IsValid())return;
 for(int32 I=0;I<2;++I){
  auto* A=GetWorld()->SpawnActor<AStaticMeshActor>();A->SetMobility(EComponentMobility::Movable);
  A->GetStaticMeshComponent()->SetStaticMesh(LivingMeshes[I]);A->SetActorEnableCollision(false);A->SetActorHiddenInGame(true);LivingProps.Add(A);
  if(auto* EM=Emma()->FindComponentByClass<USkeletalMeshComponent>()){
   const auto Channels=EM->LightingChannels;A->GetStaticMeshComponent()->SetLightingChannels(Channels.bChannel0,Channels.bChannel1,Channels.bChannel2);
  }
 }
 LivingInitialized=true;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING initialized assets=6 intervals=50-95 reading=300-480"));
}

bool ACourtyardController::StartLiving(int32 V,bool Review){
 if(ContentLayer.IsValid() && ContentLayer->BlocksOrdinaryBehavior())return false;
 InitializeLiving();auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;auto* I=M?M->GetAnimInstance():nullptr;
 const double Now=GetWorld()->GetTimeSeconds();
 if(!LivingInitialized || V<0 || V>5 || LivingBehavior>=0 || !I || ActiveAmbient || I->IsAnyMontagePlaying() || Page!=ECourtyardPage::Game
  || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction")) || !QueryBool(A,TEXT("CanHeadPat")) || A->GetVelocity().Size()>2) return false;
 // Acceptance is per behavior, not a side effect of finding a tree. Station
 // actions also require their own approved route; unresolved gestures stay labs-only.
 if(!Review && V!=0 && V!=4 && !(V==1 && !ChimeAnchor->ActorHasTag(TEXT("LivingRoutePending"))) && !(V==3 && LivelyReady(TEXT("B10")) && Mood==TEXT("happy")))return false;
 if(!Review && (Now<LivingCooldown[V] || Now-AmbientStillSince<5 || Now-LivingNumber(A,TEXT("LastGlanceTime"),-100)<10 || (!LivingTrip && LivingNumber(A,TEXT("DueTime"),Now)-Now<9)))return false;
 float Y=0;
 if(V==0 && FVector::Dist2D(A->GetActorLocation(),LivingTree->GetActorLocation())>245
  && !(AncientTree.IsValid() && FVector::Dist2D(A->GetActorLocation(),AncientTree->GetActorLocation())<570))return false;
 if(V==0 || V==4){
  FHitResult Roof;FCollisionQueryParams Q(SCENE_QUERY_STAT(LivingOutside),false,A);
  const FVector Head=M->GetSocketLocation(TEXT("頭"));
  // Petals and drifting feathers belong outdoors, never through an interior ceiling.
  if(GetWorld()->LineTraceSingleByChannel(Roof,Head,Head+FVector(0,0,400),ECC_Visibility,Q))return false;
 }
 if(V==1 && (FVector::Dist2D(A->GetActorLocation(),ChimeAnchor->GetActorLocation())>8 || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,ChimeAnchor->GetActorRotation().Yaw))>3))return false;
 if(V==3 && !Review && (!P2PlayerContext(170,Y) || FMath::Abs(Y)>20 || Now-P2GazeSince<2 || P2GazeSince<0 || FVector::Dist2D(A->GetActorLocation(),PlayerCharacter->GetActorLocation())>150))return false;
 if(V==5 && (FVector::Dist2D(A->GetActorLocation(),ReadingAnchor->GetActorLocation())>12 || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,ReadingAnchor->GetActorRotation().Yaw))>3))return false;

 if(V==2){
  const FVector Head=M->GetSocketLocation(TEXT("頭"));const FVector Target=Head+A->GetActorForwardVector()*350+FVector(0,0,650);
  if(!VisibleTarget(Target))return false;
 }
 if(LivelyEnabled && V==0){
  const bool Ready=LivelyReady(TEXT("C9"),Review && !LivingTrip);
  LivingPetalFailed=LivelyStage>=3 && Ready && (ReviewPetalOutcome>=0?ReviewPetalOutcome==1:FMath::FRand()<.3f);ReviewPetalOutcome=-1;P3PetalReleased=false;
  LivingMotions[0]=LivelyMotion(LivingPetalFailed?TEXT("P3PetalMiss"):Ready && PlayerWatching?TEXT("PetalSuccess"):TEXT("OriginalPetal"),LivingMotions[0]);
 }
 if(LivelyEnabled && V==1)LivingMotions[1]=LivelyMotion(LivelyReady(TEXT("C15"),LivelyForceChime) && (LivelyForceChime || FMath::FRand()<.25f)?TEXT("ChimeStartled"):TEXT("OriginalChime"),LivingMotions[1]);
 if(LivelyStage>=2 && V==3){LivingMotions[3]=LivelyMotion(TEXT("HighFiveInvite"),LivingMotions[3]);LivingMotions[6]=LivelyMotion(TEXT("HighFiveResponse"),LivingMotions[6]);}
 if(!LivingMotions.IsValidIndex(V) || !LivingMotions[V] || I->Montage_Play(LivingMotions[V],1,EMontagePlayReturnType::MontageLength,0,false)<=0)return false;
 LivingBool(A,TEXT("AllowInteraction"),false);LivingBehavior=V;LivingStarted=Now;LivingSoundPlayed=false;
 if(V==5){BookmarkPicked=false;BookmarkPlacedDown=false;if(ReadingBookmark.IsValid())ReadingBookmark->SetActorTransform(BookmarkRest);}
 ActiveAmbient=LivingMotions[V];AmbientActor=A;AmbientBehavior=100+V;LastLiving=V;
 LivelyLivingBegan(V);if(V==3 && LivelyStage>=2)LivelyStarted(TEXT("B10"),TEXT("high_five_invite"),true);
 LivingCooldown[V]=Now+FMath::FRandRange(180.f,300.f);
 LivingStartPosition=M->GetComponentTransform().TransformPosition(V==0?FVector(-10,32,190):FVector(-10,30,170));
 if(V==0 || V==4){auto P=LivingProps[V==0?0:1];P->SetActorScale3D(FVector::OneVector);P->SetActorLocation(LivingStartPosition);P->SetActorHiddenInGame(false);}
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING begin id=%d name=%s review=%d seen=%d t=%.2f"),V,Names[V],Review,PlayerWatching,Now);return true;
}

bool ACourtyardController::CanHighFiveReply() const {
 if(LivingBehavior!=3 || Page!=ECourtyardPage::Game)return false;
 auto* Ch=Cast<ACharacter>(Emma());auto* Inst=Ch&&Ch->GetMesh()?Ch->GetMesh()->GetAnimInstance():nullptr;
 if(!Inst || !LivingMotions.IsValidIndex(3) || !Inst->Montage_IsPlaying(LivingMotions[3]))return false;
 const double T=Inst->Montage_GetPosition(LivingMotions[3]);if(T<2.6 || T>4.8)return false;
 float Y=0;if(!P2PlayerContext(170,Y) || FMath::Abs(Y)>20)return false;
 auto* A=Cast<ACharacter>(Emma());FVector V;FRotator R;GetPlayerViewPoint(V,R);
 return A && FVector::Dist(V,A->GetMesh()->GetSocketLocation(TEXT("手首_R")))<155;
}
bool ACourtyardController::ReplyHighFive(){
 if(!CanHighFiveReply())return false;
 auto* A=Cast<ACharacter>(Emma());auto* I=A->GetMesh()->GetAnimInstance();
 if(!LivingMotions[6] || I->Montage_Play(LivingMotions[6],1,EMontagePlayReturnType::MontageLength,0,true)<=0)return false;
 ActiveAmbient=LivingMotions[6];LivingBehavior=6;AmbientBehavior=106;LivingStarted=GetWorld()->GetTimeSeconds();
 MoodEvent(TEXT("shared"));UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING highfive accepted"));return true;
}

void ACourtyardController::TickLiving(float Dt){
 InitializeLiving();if(!LivingInitialized)return;
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;auto* I=M?M->GetAnimInstance():nullptr;if(!I)return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(LivingBehavior>=0){
  const int32 V=LivingBehavior;const float T=I->Montage_IsActive(LivingMotions[V])?I->Montage_GetPosition(LivingMotions[V]):LivingMotions[V]->GetPlayLength();
  if(V==0 || V==4){
   auto P=LivingProps[V==0?0:1];const FVector Hand=Palm(M,TEXT("R"));FVector Pos;
   const float Catch=V==0?2.7f:0.f,Release=V==0?4.9f:3.95f;
   if(T<Catch){const float U=Soft(T/Catch);Pos=FMath::Lerp(LivingStartPosition,Hand,U);Pos+=M->GetRightVector()*FMath::Sin(U*PI*2)*4*(1-U);}
   else if(V==0 && LivingPetalFailed){
    if(!P3PetalReleased){LivingReleased=Hand;P3PetalReleased=true;}
    const float U=T-Catch;Pos=LivingReleased+A->GetActorForwardVector()*U*13+M->GetRightVector()*FMath::Sin(U*1.3f)*4+FVector(0,0,-6*U-8*U*U);
   }
   else if(T<Release){Pos=Hand;LivingReleased=Hand;}
   else {float U=T-Release;Pos=LivingReleased+A->GetActorForwardVector()*(U*(V==4?25:13))+M->GetRightVector()*(FMath::Sin(U*1.3)*4)+FVector(0,0,12*U-8*U*U);}
   // FBX feathers point along local -Y; align their tip from wrist toward fingers.
   const FVector W=M->GetSocketLocation(TEXT("手首_R")),IX=M->GetSocketLocation(TEXT("人指１_R")),PK=M->GetSocketLocation(TEXT("小指１_R"));
   const FVector Along=((IX+PK)*.5f-W).GetSafeNormal();FVector Normal=FVector::CrossProduct(IX-W,PK-W).GetSafeNormal();if(Normal.Z<0)Normal=-Normal;
   const FVector RestDirection=(Along*.7071f+FVector::CrossProduct(Normal,Along)*.7071f).GetSafeNormal();
   const FQuat PalmRotation=FRotationMatrix::MakeFromYZ(-RestDirection,Normal).ToQuat();
   if(V==4){Pos-=RestDirection*2.15f;Pos+=Normal*1.2f;}
   const FQuat Drift=(PalmRotation*FQuat(FVector::ForwardVector,FMath::Sin(T*1.3f)*.18f));
   P->SetActorLocationAndRotation(Pos,T>=Catch && T<Release?PalmRotation:Drift);
   if(T>=Catch && !LivingSoundPlayed && FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview"))){
    FVector BO,BE;P->GetActorBounds(false,BO,BE);
    LivingSoundPlayed=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING prop_contact id=%d pos=%s mesh=%s hidden=%d visible=%d registered=%d bounds=%s extent=%s wrist=%s"),V,*Pos.ToString(),*GetNameSafe(P->GetStaticMeshComponent()->GetStaticMesh()),P->IsHidden(),P->GetStaticMeshComponent()->IsVisible(),P->GetStaticMeshComponent()->IsRegistered(),*BO.ToString(),*BE.ToString(),*M->GetSocketLocation(TEXT("手首_R")).ToString());
   }
  }
  if(V==1 && LivingChime.IsValid()){
   if(T>=2.85 && !LivingSoundPlayed){
    LivingSoundPlayed=true;const int32 S=FMath::RandHelper(ChimeSounds.Num());
    if(ChimeSounds[S])UGameplayStatics::PlaySoundAtLocation(this,ChimeSounds[S],LivingChime->GetActorLocation(),.6f*Applied.Environment,1,0,ChimeAttenuation);
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING chime_contact sound=%d"),S);
   }
   const float U=FMath::Max(0.f,T-2.85f);FRotator R=ChimeRest.Rotator();R.Roll+=U>0?FMath::Sin(U*6)*5*FMath::Exp(-U*.8f):0;LivingChime->SetActorRotation(R);
  }
  if(V==5 && ReadingBookmark.IsValid()){
   if(LivingMotions[V]->GetName().Contains(TEXT("BookmarkRead_Contact"))){
    // A prop remains on the page until the actual grasp, then follows the
    // captured grip frame. Release leaves it exactly where it was put down.
    const FTransform Finger=M->GetSocketTransform(TEXT("人指３_R"));
    if(T>=2.6f && !BookmarkPicked){
     BookmarkGripLocal=ReadingBookmark->GetActorTransform().GetRelativeTransform(Finger);BookmarkPicked=true;
     UE_LOG(LogTemp,Display,TEXT("COURTYARD_READING grasp t=%.3f finger=%s bookmark=%s"),T,*Finger.GetLocation().ToString(),*ReadingBookmark->GetActorLocation().ToString());
    }
    if(BookmarkPicked && !BookmarkPlacedDown){
     ReadingBookmark->SetActorTransform(BookmarkGripLocal*Finger);
     if(T>=5.15f){BookmarkPlaced=ReadingBookmark->GetActorTransform();BookmarkPlacedDown=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_READING release t=%.3f bookmark=%s"),T,*BookmarkPlaced.GetLocation().ToString());}
    }
   }else{
    const FVector Finger=M->GetSocketLocation(TEXT("人指３_R"));
    const float Grip=Soft((T-2.4f)/.6f)*(1-Soft((T-11.8f)/1.1f));
    ReadingBookmark->SetActorLocation(FMath::Lerp(BookmarkRest.GetLocation(),Finger+FVector(0,0,.4),Grip));
   }
  }
  if(Now-LivingStarted>.5 && !I->Montage_IsActive(LivingMotions[V])){
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING end id=%d t=%.2f"),V,Now);
   LivingBehavior=INDEX_NONE;NextLiving=Now+FMath::FRandRange(50.f,95.f);
   for(auto P:LivingProps)P->SetActorHiddenInGame(true);
   if(LivingChime.IsValid())LivingChime->SetActorTransform(ChimeRest);
   if(ReadingBookmark.IsValid() && !(V==5 && BookmarkPlacedDown))ReadingBookmark->SetActorTransform(BookmarkRest);
   if(V==LivingTripPurpose && LivingTrip){ReadingReturning=true;ReadingChain=StationChain(LivingTripPurpose,true);ReadingSegment=0;ReadingPhase=0;ReadingCurrent=nullptr;ReadingDue=Now+FMath::FRandRange(7.f,13.f);}
  }
  return;
 }
 if(LivingTrip){TickReadingTrip();return;}
 if(ReviewBodyHold)return;
 // A chosen trip waits for her look at the player (S2) before she sets off.
 if(!LocoOn && TripPendingStation>=0){   // old segment trips: off while the new walking runs
  if(Now-TripLookAt<1.6)return;
  const int32 Station=TripPendingStation;TripPendingStation=-1;
  if(StartReadingTrip(false,Station)){TripDefer=-1;return;}
  NextReading=Now+FMath::FRandRange(70.f,140.f);
 }
 // Trips and small moments wait (up to a limit) until the player can see her.
 if(!LocoOn && Now>=NextReading && FVector::Dist2D(A->GetActorLocation(),FVector(-195,0,82.4))<8 && !KeepingCompany() && SeenOrWaited(TripDefer)){
  TArray<int32> Stations;
  if(AncientAnchor.IsValid() && !AncientAnchor->ActorHasTag(TEXT("LivingRoutePending")))Stations.Add(0);
  if(!ChimeAnchor->ActorHasTag(TEXT("LivingRoutePending")))Stations.Add(1);
  if(!ReadingAnchor->ActorHasTag(TEXT("LivingRoutePending")))Stations.Add(5);
  if(Stations.Num() && FMath::FRand()<.75f){
   const int32 Station=Stations[FMath::RandHelper(Stations.Num())];
   // The player is close in front of her: she looks at them and smiles before heading off.
   float Yaw=0,Distance=0;
   if(GazeEnabled && GazeLayerEnabled && !I->IsAnyMontagePlaying() && PlayerBearing(Yaw,Distance) && Distance<600.f && FMath::Abs(Yaw)<=100.f){
    TripPendingStation=Station;TripLookAt=Now;GazeDepartLookUntil=Now+1.6;
    FLivelyFacePhase Smile;Smile.Start=.1f;Smile.End=1.4f;Smile.Fade=.25f;Smile.Values={{45,.3f},{18,.3f}};StartFace(TEXT("GazeDepart"),{Smile},1.6f,TEXT(""),1);  // no generic #24 (expression remap batch 3)
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPANION depart_look trip=%d yaw=%.0f distance=%.0f t=%.2f"),Station,Yaw,Distance,Now);return;
   }
   if(StartReadingTrip(false,Station)){TripDefer=-1;return;}
  }
  NextReading=Now+FMath::FRandRange(70.f,140.f);
 }
 if(Now<NextLiving || ActiveAmbient || Page!=ECourtyardPage::Game)return;
 NextLiving=Now+FMath::FRandRange(8.f,14.f);
 if(!SeenOrWaited(LivingDefer)){NextLiving=Now+3;return;}
 TArray<int32> Bag={0,4};
 while(Bag.Num()){const int32 N=FMath::RandHelper(Bag.Num()),V=Bag[N];Bag.RemoveAt(N);if(V!=LastLiving && StartLiving(V)){LivingDefer=-1;return;}}
}

bool ACourtyardController::StartReadingTrip(bool Review,int32 Purpose){
 InitializeLiving();auto* A=Cast<ACharacter>(Emma());const double Now=GetWorld()->GetTimeSeconds();
 if(!LivingInitialized || LivingTrip || LivingBehavior>=0 || ActiveAmbient || !A || A->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying()
  || Page!=ECourtyardPage::Game || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction"))
  || FVector::Dist2D(A->GetActorLocation(),FVector(-195,0,82.4))>8 || FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw,105))>3)return false;
 if(!Review && (Now-AmbientStillSince<8 || LivingNumber(A,TEXT("DueTime"),Now)-Now<9))return false;
 if(Purpose!=0 && Purpose!=1 && Purpose!=5)return false;
 if(Purpose==0 && (!AncientAnchor.IsValid() || AncientAnchor->ActorHasTag(TEXT("LivingRoutePending"))))return false;
 if((Purpose==5 && ReadingAnchor->ActorHasTag(TEXT("LivingRoutePending"))) || (Purpose==1 && ChimeAnchor->ActorHasTag(TEXT("LivingRoutePending"))))return false;
 AncientLookTried=false;LivingTrip=true;LivingTripPurpose=Purpose;ReadingReturning=false;ReadingPaused=false;ReadingPhase=0;ReadingSegment=0;ReadingChain=StationChain(Purpose,false);ReadingCurrent=nullptr;ReadingDue=Now;
 ReadingOldActivity=BPBool(A,TEXT("ActivityEnabled"));LivingBool(A,TEXT("ActivityEnabled"),false);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING station_depart purpose=%d review=%d seen=%d t=%.2f"),Purpose,Review,PlayerWatching,Now);return true;
}
bool ACourtyardController::ReadingSegmentClear(float From,float To) const {
 auto* A=Cast<ACharacter>(Emma());if(!A || !ReadingChain.IsValidIndex(ReadingSegment))return false;
 const auto& P=LivingRoute::Points(ReadingChain[ReadingSegment]);const int32 Begin=FMath::Clamp(FMath::FloorToInt(From*(P.Num()-1)),0,P.Num()-1),End=FMath::Clamp(FMath::CeilToInt(To*(P.Num()-1)),Begin,P.Num()-1);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ReadingRoute),false,A);FHitResult Hit;
 FVector Last=A->GetActorLocation();
 for(int32 N=Begin;N<=End;++N){
  const FVector Next=ReadingSegmentStart.TransformPosition(FVector(P[N].X,P[N].Y,0));
  if(GetWorld()->SweepSingleByProfile(Hit,Last,Next,FQuat::Identity,TEXT("Pawn"),FCollisionShape::MakeCapsule(A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-1.f),Q)){
   if(FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING route_hit actor=%s component=%s segment=%d phase=%.3f point=%s initial=%d"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),ReadingSegment,From,*Hit.ImpactPoint.ToString(),Hit.bStartPenetrating);
   return false;
  }
  Last=Next;
 }
 return true;
}
void ACourtyardController::TickReadingTrip(){
 auto* A=Cast<ACharacter>(Emma());auto* I=A->GetMesh()->GetAnimInstance();const double Now=GetWorld()->GetTimeSeconds();
 if(Page!=ECourtyardPage::Game || ActiveAmbient || LivingBehavior>=0 || PendingLivelyPat || LivelyResponseActive)return;
 if(ReadingCurrent && I->Montage_IsPlaying(ReadingCurrent)){
  const float Phase=I->Montage_GetPosition(ReadingCurrent),Length=ReadingCurrent->GetPlayLength();
  if(!ReadingSegmentClear(Phase/Length,FMath::Min(1.f,(Phase+.7f)/Length))){
   ReadingPhase=Phase;I->Montage_Stop(.35f,ReadingCurrent);ReadingPaused=true;ReadingDue=Now+FMath::FRandRange(5.f,9.f);
   LivingBool(A,TEXT("TravelActive"),false);LivingBool(A,TEXT("AllowInteraction"),false);
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING reading_blocked segment=%d phase=%.3f"),ReadingSegment,Phase);
  }
  return;
 }
 if(I->IsAnyMontagePlaying() || Now<ReadingDue)return;
 if(ReadingCurrent && !ReadingPaused){
  LivingBool(A,TEXT("TravelActive"),false);LivingBool(A,TEXT("AllowInteraction"),true);
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING reading_segment_end index=%d pos=%s yaw=%.3f"),ReadingSegment,*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw);
  ReadingSegment++;ReadingPhase=0;ReadingCurrent=nullptr;ReadingDue=Now+.65;return;
 }
 if(ReadingSegment>=ReadingChain.Len()){
  if(!ReadingReturning){
   if(LivelyStage>=2 && LivingTripPurpose==0 && !AncientLookTried){
    if(PoseTransition || IsSeatBusy())return;
    if(LivelyOn(TEXT("C10")) && AncientCanopyVisible()){
     if(PlayLivelyHead(TEXT("C10"),TEXT("AncientGaze"),true)){AncientLookTried=true;return;}
     return;
    }
    AncientLookTried=true;
   }
   if(StartLiving(LivingTripPurpose,true))return;
   UE_LOG(LogTemp,Warning,TEXT("COURTYARD_LIVING reading_alignment_failed pos=%s yaw=%.3f; returning without snapping"),*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw);
   ReadingReturning=true;ReadingChain=StationChain(LivingTripPurpose,true);ReadingSegment=0;ReadingPhase=0;ReadingCurrent=nullptr;return;
  }
  LivingTrip=false;LivingBool(A,TEXT("TravelActive"),false);LivingBool(A,TEXT("AllowInteraction"),true);LivingBool(A,TEXT("ActivityEnabled"),ReadingOldActivity);
  LivingTime(A,TEXT("DueTime"),Now+FMath::FRandRange(55.f,115.f));NextReading=Now+FMath::FRandRange(300.f,480.f);NextLiving=Now+45;
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING reading_return pos=%s yaw=%.3f"),*A->GetActorLocation().ToString(),A->GetActorRotation().Yaw);return;
 }
 if(!ReadingPaused)ReadingSegmentStart=FTransform(A->GetActorRotation(),A->GetActorLocation());
 TCHAR C=ReadingChain[ReadingSegment];ReadingCurrent=ReadingWalkMotions[C=='S'?0:C=='A'?1:2];
 if(!ReadingSegmentClear(ReadingPhase/ReadingCurrent->GetPlayLength(),1)){
  if(ReadingSegment==0 && ReadingPhase==0 && !ReadingReturning){
   LivingTrip=false;ReadingCurrent=nullptr;LivingBool(A,TEXT("TravelActive"),false);LivingBool(A,TEXT("AllowInteraction"),true);LivingBool(A,TEXT("ActivityEnabled"),ReadingOldActivity);
   LivingTime(A,TEXT("DueTime"),Now+FMath::FRandRange(55.f,115.f));NextReading=Now+FMath::FRandRange(180.f,300.f);
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING departure_cancelled blocked; stable activity restored"));return;
  }
  ReadingPaused=true;ReadingDue=Now+FMath::FRandRange(5.f,9.f);LivingBool(A,TEXT("AllowInteraction"),true);return;
 }
 LivingBool(A,TEXT("AllowInteraction"),false);LivingBool(A,TEXT("TravelActive"),true);
 if(I->Montage_Play(ReadingCurrent,1,EMontagePlayReturnType::MontageLength,ReadingPhase,false)>0){
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING reading_segment_start index=%d code=%c resume=%.3f"),ReadingSegment,C,ReadingPhase);ReadingPaused=false;
 }else {LivingBool(A,TEXT("TravelActive"),false);LivingBool(A,TEXT("AllowInteraction"),true);ReadingPaused=true;ReadingDue=Now+8;}
}

// Authoring-only, bounded search over existing root curves in a live physics
// world. This is never called by autonomous play and never moves any actor.
void ACourtyardController::CourtyardReviewStationPlan(int32 Purpose){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 InitializeLiving();auto* A=Cast<ACharacter>(Emma());if(!A || !LivingInitialized)return;
 struct Node {FVector P;float Y;FString Chain;double Cost,Score;};
 FCollisionQueryParams Q(SCENE_QUERY_STAT(LivingAuthorPlan),false,A);if(GetPawn())Q.AddIgnoredActor(GetPawn());
 FVector Target=(Purpose==1 || Purpose==2)?ChimeAnchor->GetActorLocation():Purpose==5?ReadingAnchor->GetActorLocation():FVector(-450,-2200,82.4);
 float TargetYaw=(Purpose==1 || Purpose==2)?ChimeAnchor->GetActorRotation().Yaw:105;
 auto Key=[](const FVector& P,float Y){return FString::Printf(TEXT("%d:%d:%d"),FMath::RoundToInt(P.X/8),FMath::RoundToInt(P.Y/8),FMath::RoundToInt(Y/60));};
 auto Search=[&](FVector Start,float SY,FVector Goal,float GY,bool Flexible,FVector& End,float& EY)->FString{
  TArray<Node> Open;Open.Add({Start,SY,TEXT(""),0,FVector::Dist2D(Start,Goal)/194});TMap<FString,float> Seen;
  const double Begin=FPlatformTime::Seconds();int32 Count=0;
  while(Open.Num() && Count++<14000 && FPlatformTime::Seconds()-Begin<18){
   int32 Best=0;for(int32 J=1;J<Open.Num();++J)if(Open[J].Score<Open[Best].Score)Best=J;
   Node N=Open[Best];Open.RemoveAtSwap(Best,1,EAllowShrinking::No);
   if(N.Cost>Seen.FindRef(Key(N.P,N.Y)) && N.Chain.Len())continue;
   if(FVector::Dist2D(N.P,Goal)<(Flexible?125:7) && (Flexible || FMath::Abs(FMath::FindDeltaAngleDegrees(N.Y,GY))<1)){
    End=N.P;EY=N.Y;UE_LOG(LogTemp,Display,TEXT("COURTYARD_PLAN found purpose=%d chain=%s end=%s yaw=%.3f expanded=%d"),Purpose,*N.Chain,*End.ToString(),EY,Count);return N.Chain;
   }
   if(N.Chain.Len()>=38)continue;
   for(TCHAR C:{TEXT('S'),TEXT('A'),TEXT('U')}){
    if(C=='U' && N.Chain.EndsWith(TEXT("U")))continue;
    const auto& Points=LivingRoute::Points(C);FTransform Frame(FRotator(0,N.Y,0),N.P);FVector Prev=N.P;bool Clear=true;
    for(int32 J=0;J<Points.Num();J+=2){
     const FVector Next=Frame.TransformPosition(FVector(Points[J].X,Points[J].Y,0));FHitResult H;
     if(Next.X< -1450 || Next.X>900 || Next.Y< -2950 || Next.Y>1300 || GetWorld()->SweepSingleByProfile(H,Prev,Next,FQuat::Identity,TEXT("Pawn"),FCollisionShape::MakeCapsule(A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-1.f),Q)){if(N.Chain.IsEmpty())UE_LOG(LogTemp,Display,TEXT("COURTYARD_PLAN root_reject code=%c actor=%s radius=%.2f half=%.2f point=%s"),C,*GetNameSafe(H.GetActor()),A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),*Next.ToString());Clear=false;break;}
     Prev=Next;
    }
   if(!Clear)continue;
    FVector NP=Frame.TransformPosition(FVector(Points.Last().X,Points.Last().Y,0));const float NY=FRotator::NormalizeAxis(N.Y+(C=='A'?-60:C=='U'?-180:0));
    const float Cost=N.Cost+(C=='U'?1.5f:1.f);const FString K=Key(NP,NY);if(Seen.Contains(K) && Seen[K]<=Cost)continue;Seen.Add(K,Cost);
    Open.Add({NP,NY,N.Chain+FString::Chr(C),Cost,Cost+FVector::Dist2D(NP,Goal)/194});
   }
  }
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_PLAN not_found purpose=%d expanded=%d seconds=%.2f"),Purpose,Count,FPlatformTime::Seconds()-Begin);return TEXT("");
 };
 FVector End;float EY=0;const FVector Home(-195,0,82.4);
 const FString Out=Search(Home,105,Target,TargetYaw,Purpose!=1,End,EY);
 if(!Out.IsEmpty()){FVector Back;float BY;Search(End,EY,Home,105,false,Back,BY);}
}

void ACourtyardController::CourtyardReviewLiving(int32 V){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING review id=%d accepted=%d"),V,StartLiving(V,true));
}
void ACourtyardController::CourtyardReviewReading(){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING review_reading accepted=%d"),StartReadingTrip(true));
}
void ACourtyardController::CourtyardReviewTrip(int32 Purpose){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING review_trip purpose=%d accepted=%d"),Purpose,StartReadingTrip(true,Purpose));
}
void ACourtyardController::CourtyardReviewLifeSchedule(){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || LivingTrip || ActiveAmbient)return;
 NextReading=GetWorld()->GetTimeSeconds()+2;
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING review_wait_shortened only; runtime chooses eligible destination or stays"));
}
void ACourtyardController::CourtyardReviewLivingPlace(int32 Point){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || LivingTrip || LivingBehavior>=0 || ActiveAmbient)return;
 InitializeLiving();auto* A=Cast<ACharacter>(Emma());if(!A || A->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying())return;
 if(Point==4 && PlayerCharacter){
  // Observer setup only; Emma and the station stay untouched. Check a real
  // standing capsule before positioning the review player beside the lectern.
  const FVector Observer=A->GetActorLocation()+A->GetActorForwardVector()*100-A->GetActorRightVector()*45;
  const auto* Capsule=PlayerCharacter->GetCapsuleComponent();FCollisionQueryParams Q(SCENE_QUERY_STAT(ReadingObserver),false,A);Q.AddIgnoredActor(PlayerCharacter);
  if(GetWorld()->OverlapBlockingTestByProfile(Observer,FQuat::Identity,TEXT("Pawn"),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1),Q)){
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_READING observer_rejected collision"));return;
  }
  PlayerCharacter->SetActorLocation(Observer,false,nullptr,ETeleportType::TeleportPhysics);SetViewTargetWithBlend(GetPawn(),0);
  FVector Eye;FRotator R;GetPlayerViewPoint(Eye,R);SetControlRotation((A->GetMesh()->GetSocketLocation(TEXT("頭"))-Eye).Rotation());
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_READING observer_setup npc_unchanged=1 pos=%s"),*Observer.ToString());return;
 }
 FVector P(-195,0,82.4);float Y=105;
 if(Point==1){P=FVector(-113.75,-182.31,82.4);Y=-74.9859;}
 if(Point==2 && ReadingAnchor.IsValid()){P=ReadingAnchor->GetActorLocation();Y=ReadingAnchor->GetActorRotation().Yaw;}
 if(Point==3 && ChimeAnchor.IsValid()){P=ChimeAnchor->GetActorLocation();Y=ChimeAnchor->GetActorRotation().Yaw;}
 A->SetActorLocationAndRotation(P,FRotator(0,Y,0),false,nullptr,ETeleportType::TeleportPhysics);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_LIVING review_fixture point=%d NOT_ROUTE_EVIDENCE"),Point);
}



