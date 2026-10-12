#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentDoor.h"
#include "CourtyardContentEnvironment.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "UObject/UnrealType.h"
#include "Engine/OverlapResult.h"
#include "SkeletalRenderPublic.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
namespace {void RV(AActor* A,const TCHAR* N,double V){if(auto* P=FindFProperty<FNumericProperty>(A->GetClass(),N))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),V);}void RB(AActor* A,const TCHAR* N,bool V){if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,V);}}
void ACourtyardContentLife::ReviewSetup(int32 Place)
{
 auto* A=Cast<ACharacter>(C->Emma());if(!A)return;const double Now=GetWorld()->GetTimeSeconds();
 FloorJourney=0;NextFloorVisit=Now+10000;
 if(C->LocoState!=ACourtyardController::ELocoState::Idle)C->LocoEnd(false,TEXT("review_setup"));
 A->GetMesh()->GetAnimInstance()->Montage_Stop(.2f);A->GetCharacterMovement()->StopMovementImmediately();C->ActiveAmbient=nullptr;C->LivingBehavior=-1;C->LivingTrip=false;C->WelcomeDone=true;C->LocoLifePaused=true;ReleaseTravel();
 C->LivelyBodyId=NAME_None;C->CancelFace();Emotion=NAME_None;C->ReviewBodyHold=true;C->LivelyCheckAt=Now+10000;C->NextBody=C->NextAmbient=C->NextCompanion=C->NextLiving=C->NextReading=Now+10000;C->AmbientStillSince=Now-30;
 RB(A,TEXT("TravelActive"),false);RB(A,TEXT("Interrupted"),false);RB(A,TEXT("AllowInteraction"),true);RB(A,TEXT("ActivityEnabled"),false);RB(A,TEXT("GlanceEnabled"),false);RV(A,TEXT("DueTime"),Now+10000);RV(A,TEXT("LastGlanceTime"),Now-30);
 const FVector Points[]={FVector(-195,0,82.4),FVector(350,1205,82.4),FVector(-300,-2440,82.4),FVector(0,-1820,82.4),FVector(-65,-60,82.4),FVector(0,650,82.4)};
 const float Yaw[]={105,90,-170,90,90,90};const int K=FMath::Clamp(Place,0,5);A->SetActorLocationAndRotation(Points[K],FRotator(0,Yaw[K],0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
 if(C->GetPawn()){C->GetPawn()->SetActorLocation(Points[K]+A->GetActorForwardVector()*180,false);C->SetControlRotation(FRotator(0,Yaw[K]+180,0));}C->CourtyardReviewBodyView(0);
 NextDecision=Now+10000;NextSeat=Now+10000;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT review_setup place=%d"),K);
}
void ACourtyardContentLife::ReviewExtra(int32 Event)
{
 if(Event==987 && WindowPauseEnabled && C->GetPawn() && C->Emma()){
  if(FVector::Dist2D(C->Emma()->GetActorLocation(),WindowPauseDock)>35){UE_LOG(LogTemp,Display,TEXT("CONTENT44 observer_not_at_window"));return;}
  const FVector P(-265,650,82.4);auto* Player=Cast<ACharacter>(C->GetPawn());auto* Capsule=Player?Player->GetCapsuleComponent():nullptr;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(WindowObserverSpace),false,C->Emma());Q.AddIgnoredActor(C->GetPawn());
  if(!Capsule || GetWorld()->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Q)){UE_LOG(LogTemp,Display,TEXT("CONTENT44 observer_blocked"));return;}
  C->GetPawn()->SetActorLocation(P,false);Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);C->CourtyardReviewBodyView(0);C->CourtyardReviewLookAtEmma(0);
  UE_LOG(LogTemp,Display,TEXT("CONTENT44 player_only_observer placed=%s npc_unchanged=1"),*P.ToString());return;
 }
 if(Event==986 && WindowPauseEnabled){
  ReviewWindowUntil=GetWorld()->GetTimeSeconds()+90;
  UE_LOG(LogTemp,Display,TEXT("CONTENT44 window_review_wait actual_safe_state=1 no_placement=1 no_timer_speedup=1"));return;
 }
 if(Event==985 && HearthPauseEnabled){
  ReviewHearthUntil=GetWorld()->GetTimeSeconds()+90;
  UE_LOG(LogTemp,Display,TEXT("CONTENT43 hearth_review_wait actual_safe_state=1 no_placement=1 no_timer_speedup=1"));return;
 }
 if(Event==984 && FlowerBedPauseEnabled){
  ReviewFlowerBedUntil=GetWorld()->GetTimeSeconds()+90;
  UE_LOG(LogTemp,Display,TEXT("CONTENT42 flower_pause_review_wait actual_safe_state=1 no_placement=1 no_timer_speedup=1"));return;
 }
 if(Event>=980 && Event<=983 && Environment.IsValid()){
  Environment->ReviewArmBodyPetals(C->Emma(),Event>=982,Event==980 || Event==982);return;
 }
 if(Event==978){ReviewSetup(0);UE_LOG(LogTemp,Display,TEXT("CONTENT41 explicit_pose_fixture no_source_asset_writes=1 NOT_natural_observation=1"));return;}
 if(Event==979 && C->Emma()){
  // Read-only finite diagnostic: the accepted motion and real capsule are untouched.
  auto* A=Cast<ACharacter>(C->Emma());if(!A || !A->GetMesh())return;auto* M=A->GetMesh();auto* Cap=A->GetCapsuleComponent();
  TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);TArray<FVector3f> W;W.Reserve(V.Num());
  const FTransform X=M->GetComponentTransform();const FVector Root=A->GetActorLocation();
  const float R=Cap->GetScaledCapsuleRadius()+14,H=Cap->GetScaledCapsuleHalfHeight()+6;int32 Outside=0;float MaxRad=0,MaxZ=-1e9f;
  for(const auto& P:V){const FVector Point=X.TransformPosition(FVector(P.Position));W.Add(FVector3f(Point));const float Rad=FVector::Dist2D(Point,Root);MaxRad=FMath::Max(MaxRad,Rad);MaxZ=FMath::Max(MaxZ,float(Point.Z));if(Rad>R || FMath::Abs(Point.Z-Root.Z)>H)++Outside;}
  const FString Dir=FPaths::ProjectDir()/TEXT("Audit/Content41_20261010/ActualSkin");IFileManager::Get().MakeDirectory(*Dir,true);
  const int32 Frame=FMath::RoundToInt(GetWorld()->GetTimeSeconds()*100);const FString Name=FString::Printf(TEXT("pose_%06d"),Frame);
  FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)W.GetData(),W.Num()*sizeof(FVector3f)),*(Dir/(Name+TEXT("_world.bin"))));
  FString J=FString::Printf(TEXT("{\"frame\":%d,\"root\":[%.6f,%.6f,%.6f],\"guard_radius\":%.6f,\"guard_half\":%.6f,\"vertices\":%d,\"outside_capsule_guard\":%d,\"bones\":{\n"),Frame,Root.X,Root.Y,Root.Z,R,H,V.Num(),Outside);
  const TCHAR* Names[]={TEXT("頭"),TEXT("Hair_Root"),TEXT("肩_L"),TEXT("腕_L"),TEXT("ひじ_L"),TEXT("手首_L"),TEXT("中指２_L"),TEXT("肩_R"),TEXT("腕_R"),TEXT("ひじ_R"),TEXT("手首_R"),TEXT("中指２_R")};
  bool First=true;for(const TCHAR* N:Names)if(M->DoesSocketExist(N)){const FVector P=M->GetSocketLocation(N);J+=FString::Printf(TEXT("%s\"%s\":[%.6f,%.6f,%.6f]"),First?TEXT(""):TEXT(",\n"),N,P.X,P.Y,P.Z);First=false;}
  J+=TEXT("}}\n");FFileHelper::SaveStringToFile(J,*(Dir/(Name+TEXT(".json"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  UE_LOG(LogTemp,Display,TEXT("CONTENT41 actual_skin_probe file=%s vertices=%d capsule_radius=%.3f capsule_half=%.3f outside=%d max_rad=%.3f max_z=%.3f no_model_writes=1"),*Name,V.Num(),R-14,H-6,Outside,MaxRad,MaxZ);return;
 }
 if(Event==975 && FlowerScentEnabled){ReviewFlowerVisitUntil=GetWorld()->GetTimeSeconds()+120;NextFlowerScent=GetWorld()->GetTimeSeconds()+10000;UE_LOG(LogTemp,Display,TEXT("CONTENT39 explicit_visit_request waits_for_actual_safe_state=1 no_placement=1"));return;}
 if(Event==976 && FlowerScentEnabled){
  auto* A=Cast<ACharacter>(C->Emma());auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,A->GetActorLocation(),FlowerScentDock,A);
  UE_LOG(LogTemp,Display,TEXT("CONTENT39 actual_visit_probe root=%s pending=%d owns=%d path_valid=%d partial=%d points=%d"),*A->GetActorLocation().ToString(),FlowerVisitPending,OwnsTravel,Path&&Path->IsValid(),Path&&Path->IsPartial(),Path?Path->PathPoints.Num():0);return;
 }
 if(Event==977 && Motions.Contains(TEXT("FlowerScent"))){
  auto* A=Cast<ACharacter>(C->Emma());auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FCollisionQueryParams Q(SCENE_QUERY_STAT(FlowerScentReachable),false,A);
  for(float X:{-720.f,-600.f,-460.f,-340.f,-220.f})for(float Y:{-960.f,-840.f,-755.f,-650.f,-540.f}){
   const FVector P(X,Y,82.4);FNavLocation N;const bool Projected=Nav && Nav->ProjectPointToNavigation(P,N,FVector(24,24,120));
   auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,A->GetActorLocation(),P,A);
   const bool Blocked=GetWorld()->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,79.8f),Q);
   UE_LOG(LogTemp,Display,TEXT("CONTENT39 site_probe x=%.0f y=%.0f blocked=%d projected=%d project_delta=%.2f path_valid=%d partial=%d"),X,Y,Blocked,Projected,Projected?FVector::Dist2D(P,N.Location):-1,Path&&Path->IsValid(),Path&&Path->IsPartial());
  }return;
 }
 if(Event==970 && Motions.Contains(TEXT("FlowerScent"))){
  ReviewSetup(0);auto* A=Cast<ACharacter>(C->Emma());A->GetMesh()->GetAnimInstance()->Montage_Stop(0.f);
  const FVector Dock=FlowerScentDock;FCollisionQueryParams Q(SCENE_QUERY_STAT(FlowerScentFixture),false,A);
  const bool Blocked=GetWorld()->OverlapBlockingTestByChannel(Dock,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-.2f),Q);
  UE_LOG(LogTemp,Display,TEXT("CONTENT39 explicit_fixture=1 NOT_natural_arrival=1 dock_blocked=%d"),Blocked);
  A->SetActorLocationAndRotation(Dock,FRotator(0,FlowerScentYaw,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  C->GetPawn()->SetActorLocation(Dock+FVector(-145,-205,0),false);C->CourtyardReviewBodyView(4);return;
 }
 if(Event==971 && Motions.Contains(TEXT("FlowerScent"))){
  if(FlowerScentEnabled){UE_LOG(LogTemp,Display,TEXT("CONTENT39 explicit_qualified_pose_start accepted=%d NOT_natural_arrival=1"),BeginFlowerScent());return;}
  UAnimMontage* Motion=Motions.FindRef(TEXT("FlowerScent"));auto* A=Cast<ACharacter>(C->Emma());auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  const bool OK=Motion && I && !I->IsAnyMontagePlaying() && I->Montage_Play(Motion)>0;
  UE_LOG(LogTemp,Display,TEXT("CONTENT39 flower_review_start accepted=%d root=%s"),OK,A?*A->GetActorLocation().ToString():TEXT("none"));return;
 }
 if(Event==972 && Motions.Contains(TEXT("FlowerScent"))){
  UAnimMontage* Motion=Motions.FindRef(TEXT("FlowerScent"));auto* A=Cast<ACharacter>(C->Emma());auto* M=A?A->GetMesh():nullptr;auto* I=M?M->GetAnimInstance():nullptr;
  if(!I || !I->Montage_IsActive(Motion))return;
  const float Phase=I->Montage_GetPosition(Motion);TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);TArray<FVector3f> P;
  for(const auto& X:V)P.Add(FVector3f(M->GetComponentTransform().TransformPosition(FVector(X.Position))));
  const FString Dir=FPaths::ProjectDir()/FString::Printf(TEXT("Audit/Content39_20261009/SurfaceR%s"),*Motion->GetName().Right(1));IFileManager::Get().MakeDirectory(*Dir,true);
  const FString File=Dir/FString::Printf(TEXT("scent_%05d_world.bin"),FMath::RoundToInt(Phase*100));FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)P.GetData(),P.Num()*sizeof(FVector3f)),*File);
  UE_LOG(LogTemp,Display,TEXT("CONTENT39 actual_surface phase=%.3f vertices=%d head=%s wrist=%s root=%s"),Phase,P.Num(),*M->GetSocketLocation(TEXT("頭")).ToString(),*M->GetSocketLocation(TEXT("手首_R")).ToString(),*A->GetActorLocation().ToString());return;
 }
 if((Event==973 || Event==974) && Motions.Contains(TEXT("FlowerScent"))){
  const FVector P=FlowerScentDock+(Event==973?FVector(250,25,32.6):FVector(135,-210,35.6));const FVector Target=FlowerScentDock+FVector(15,21,2.6);
  auto* Camera=GetWorld()->SpawnActor<ACameraActor>();if(Camera){Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());Camera->GetCameraComponent()->SetFieldOfView(60);C->SetViewTargetWithBlend(Camera,0);}
  UE_LOG(LogTemp,Display,TEXT("CONTENT39 explicit_full_body_view=%d"),Event);return;
 }
 if(Event==958 && Motions.Contains(TEXT("FountainTouch"))){
  ReviewSetup(0);auto* A=Cast<ACharacter>(C->Emma());A->GetMesh()->GetAnimInstance()->Montage_Stop(0.f);
  A->SetActorLocationAndRotation(FVector(0,-1742,82.4),FRotator(0,90,0),false);
  if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  C->GetPawn()->SetActorLocation(FVector(180,-1930,82),false);C->CourtyardReviewBodyView(4);
  UE_LOG(LogTemp,Display,TEXT("CONTENT38 explicit_water_fixture=1 NOT_natural_arrival=1"));return;
 }
 if(Event==959){
  UAnimMontage* Motion=Motions.FindRef(TEXT("FountainTouch"));auto* A=Cast<ACharacter>(C->Emma());
  auto* I=A?A->GetMesh()->GetAnimInstance():nullptr;
  const bool OK=Motion && I && !I->IsAnyMontagePlaying() && I->Montage_Play(Motion)>0;
  UE_LOG(LogTemp,Display,TEXT("CONTENT38 water_review_start accepted=%d real_root=%s"),OK,A?*A->GetActorLocation().ToString():TEXT("none"));return;
 }
 if(Event==960){
  UAnimMontage* Motion=Motions.FindRef(TEXT("FountainTouch"));auto* A=Cast<ACharacter>(C->Emma());auto* M=A?A->GetMesh():nullptr;auto* I=M?M->GetAnimInstance():nullptr;
  if(!Motion || !I || !I->Montage_IsActive(Motion))return;
  const float Phase=I->Montage_GetPosition(Motion);TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);TArray<FVector3f> P;
  for(const auto& X:V)P.Add(FVector3f(M->GetComponentTransform().TransformPosition(FVector(X.Position))));
  const FString Dir=FPaths::ProjectDir()/(Motion->GetName().EndsWith(TEXT("_r1"))?FString(TEXT("Audit/Content38_20261009/Surface")):FString::Printf(TEXT("Audit/Content38_20261009/SurfaceR%s"),*Motion->GetName().Right(1)));IFileManager::Get().MakeDirectory(*Dir,true);
  const FString File=Dir/FString::Printf(TEXT("water_%05d_world.bin"),FMath::RoundToInt(Phase*100));
  FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)P.GetData(),P.Num()*sizeof(FVector3f)),*File);
  UE_LOG(LogTemp,Display,TEXT("CONTENT38 real_surface phase=%.3f vertices=%d root=%s file=%s"),Phase,P.Num(),*A->GetActorLocation().ToString(),*File);return;
 }
 if((Event==961 || Event==962) && Motions.Contains(TEXT("FountainTouch"))){
  // Explicit contact diagnostic only: bank-side full-body cameras. No runtime
  // camera change and no movement of water, actor, feet, or collision geometry.
  const FVector P=Event==961?FVector(260,-1730,78):FVector(260,-1920,125);
  const FVector Target(0,-1710,60);
  auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
  if(Camera){Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());Camera->GetCameraComponent()->SetFieldOfView(65);C->SetViewTargetWithBlend(Camera,0);}
  UE_LOG(LogTemp,Display,TEXT("CONTENT38 explicit_full_body_view=%d"),Event);return;
 }
 if(Event==950 && TerraceContactEnabled){
  ReviewSetup(0);auto* A=Cast<ACharacter>(C->Emma());
  // This explicit diagnostic fixture cancels the old welcome instantly before
  // placement. Its .2s root-motion fade must not move the measured test dock.
  A->GetMesh()->GetAnimInstance()->Montage_Stop(0.f);
  A->SetActorLocationAndRotation(FVector(-820,153,82.4),FRotator(0,-90,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  C->GetPawn()->SetActorLocation(FVector(-560,73,82),false);C->SetControlRotation(FRotator(0,90,0));NextTerracePalmVisit=GetWorld()->GetTimeSeconds()+10000;
  UE_LOG(LogTemp,Display,TEXT("CONTENT37 explicit_contact_fixture=1 NOT_natural_arrival=1"));return;
 }
 if(Event==951){const bool OK=BeginTerracePalmRest();UE_LOG(LogTemp,Display,TEXT("CONTENT37 review_palm_start accepted=%d actual_root=%s yaw=%.3f safe=%d"),OK,*C->Emma()->GetActorLocation().ToString(),C->Emma()->GetActorRotation().Yaw,Safe());return;}
 if(Event==952){if(TerracePalmMotion)SampleTerracePalm();return;}
 if(Event==954){UE_LOG(LogTemp,Display,TEXT("CONTENT37 review_palm_visit accepted=%d"),BeginTerracePalmVisit(GetWorld()->GetTimeSeconds()));return;}
 if(Event==957 && TerraceContactEnabled){
  ReviewTerraceVisitUntil=GetWorld()->GetTimeSeconds()+90;
  NextTerracePalmVisit=ReviewTerraceVisitUntil+10000;
  UE_LOG(LogTemp,Display,TEXT("CONTENT37 review_visit_armed state_driven=1 timeout_seconds=90 old_behavior_unchanged=1"));return;
 }
 if(Event==956 && TerraceContactEnabled && C.IsValid() && C->GetPawn() && C->Pose==ECourtyardPose::Standing){
  ReviewFollow=false;ReviewRoomPath={FVector(-90,-60,82),FVector(-820,-60,82),FVector(-820,53,82)};
  ReviewRoomLast=C->GetPawn()->GetActorLocation();ReviewRoomBlocked=0;ReviewRoomEndYaw=90;
  UE_LOG(LogTemp,Display,TEXT("CONTENT37 review_player_approach real_input=1 avoid_initial_bench=1 no_teleport=1"));return;
 }
 if(Event==955 && C.IsValid() && C->Emma()){
  auto* A=Cast<ACharacter>(C->Emma());const FVector Dock(-820,153,82.4),Here=A->GetActorLocation();
  TArray<FOverlapResult> Hits;FCollisionQueryParams Q(SCENE_QUERY_STAT(TerracePalmReview),false,A);
  const bool Block=GetWorld()->OverlapMultiByChannel(Hits,Dock,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-.2f),Q);
  UE_LOG(LogTemp,Display,TEXT("CONTENT37 visit_probe safe=%d emotion=%s task=%s owns=%d pending=%d floor=%d root=%s distance=%.2f player_dock=%.2f blocked=%d"),Safe(),*Emotion.ToString(),*ActiveTask.ToString(),OwnsTravel,TerraceVisitPending,FloorJourney,*Here.ToString(),FVector::Dist2D(Here,Dock),C->GetPawn()?FVector::Dist2D(C->GetPawn()->GetActorLocation(),Dock):-1,Block);
  UE_LOG(LogTemp,Display,TEXT("CONTENT37 safe_detail welcome=%d montage=%d ambient=%d living=%d trip=%d bp_travel=%d allow=%d pending_pat=%d loco=%d still_age=%.3f"),C->WelcomeDone,A->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying(),C->ActiveAmbient!=nullptr,C->LivingBehavior,C->LivingTrip,C->BPBool(A,TEXT("TravelActive")),C->BPBool(A,TEXT("AllowInteraction")),C->PendingLivelyPat,int(C->LocoState),GetWorld()->GetTimeSeconds()-C->AmbientStillSince);
  for(const auto& H:Hits)if(H.bBlockingHit)UE_LOG(LogTemp,Display,TEXT("CONTENT37 dock_block actor=%s component=%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()));
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation P;
  const bool Projected=Nav&&Nav->ProjectPointToNavigation(Dock,P,FVector(70,70,140));
  auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,Here,Dock,A);
  UE_LOG(LogTemp,Display,TEXT("CONTENT37 dock_nav projected=%d result=%s path_valid=%d partial=%d points=%d"),Projected,*P.Location.ToString(),Path&&Path->IsValid(),Path&&Path->IsPartial(),Path?Path->PathPoints.Num():0);
  for(const FVector Site:{FVector(-560,63,82.4),FVector(-560,153,82.4),FVector(-820,153,82.4),FVector(-820,273,82.4)}){
   FNavLocation N;const bool NP=Nav&&Nav->ProjectPointToNavigation(Site,N,FVector(70,70,140));
   const bool Obstructed=GetWorld()->OverlapBlockingTestByChannel(Site,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,79.8f),Q);
   auto* Route=UNavigationSystemV1::FindPathToLocationSynchronously(this,Here,Site,A);FHitResult H;
   const bool RunBlocked=GetWorld()->SweepSingleByChannel(H,Site+FVector(0,170,0),Site,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(22),Q);
   UE_LOG(LogTemp,Display,TEXT("CONTENT37 candidate_site position=%s blocked=%d nav=%d nav_delta=%.2f path=%d partial=%d run170_blocked=%d"),*Site.ToString(),Obstructed,NP,NP?FVector::Dist2D(N.Location,Site):-1,Route&&Route->IsValid(),Route&&Route->IsPartial(),RunBlocked);
  }return;
 }
 if(Event==949 && C->Emma() && C->GetPawn() && C->Pose==ECourtyardPose::Standing){
  ReviewFollow=false;const FVector E=C->Emma()->GetActorLocation(),F=C->Emma()->GetActorForwardVector();
  ReviewRoomPath={E+F*135};ReviewRoomLast=C->GetPawn()->GetActorLocation();ReviewRoomBlocked=0;ReviewRoomEndYaw=FMath::UnwindDegrees(C->Emma()->GetActorRotation().Yaw+180);
  UE_LOG(LogTemp,Display,TEXT("CONTENT36 review_player_front_approach direct_real_input=1 no_teleport=1"));return;
 }
 if(Event==945 && LeadScenesEnabled && LeadPhase==2 && C->LocoState!=ACourtyardController::ELocoState::Idle){
  const FVector P=C->LocoPointAt(C->LocoProgress+90);auto* Obstacle=GetWorld()->SpawnActor<AActor>();
  auto* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(30,30,78));
  Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));Box->RegisterComponent();Obstacle->SetActorLocation(P);Obstacle->Tags.Add(TEXT("Content35ReviewObstacle"));
  ReviewFollow=false;UE_LOG(LogTemp,Display,TEXT("CONTENT35 review_obstacle placed=%s transient_fixture=1"),*P.ToString());return;
 }
 if(Event==946){for(TActorIterator<AActor> I(GetWorld());I;++I)if(I->ActorHasTag(TEXT("Content35ReviewObstacle")))I->Destroy();UE_LOG(LogTemp,Display,TEXT("CONTENT35 review_obstacle removed=1"));return;}
 if(Event==948 && C->Emma() && C->GetPawn()){
  ReviewFollow=false;const FVector E=C->Emma()->GetActorLocation(),F=C->Emma()->GetActorForwardVector(),R=C->Emma()->GetActorRightVector();
  ReviewRoomPath={E+R*190,E+F*135+R*100,E+F*135};ReviewRoomLast=C->GetPawn()->GetActorLocation();ReviewRoomBlocked=0;ReviewRoomEndYaw=FMath::UnwindDegrees(C->Emma()->GetActorRotation().Yaw+180);
  UE_LOG(LogTemp,Display,TEXT("CONTENT35 review_player_front_approach real_input_path=1"));return;
 }
 if((Event==940 || Event==941) && C.IsValid() && C->GetPawn() && C->Emma()){
  if(LeadPhase && C->LocoState==ACourtyardController::ELocoState::Idle)EndLeadScene(TEXT("review_fixture_reset"));
  ReviewSetup(3);auto* A=Cast<ACharacter>(C->Emma());
  A->SetActorLocationAndRotation(FVector(-1100,-600,82.4),FRotator(0,0,0),false);
  if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  C->GetPawn()->SetActorLocation(FVector(-860,-600,82),false);
  C->SetControlRotation(FRotator(0,180,0));
  NextQuietVisit=NextQuietSite=GetWorld()->GetTimeSeconds()+10000;
  Cooldowns.Remove(TEXT("LeadScene"));
  ReviewFollow=Event==941;
  UE_LOG(LogTemp,Display,TEXT("CONTENT35 review_fixture explicit_placement=1 follow_input_assist=%d natural_timers_unchanged=1"),ReviewFollow);return;
 }
 if(Event==942){const bool OK=BeginLeadScene(FVector(-850,-1100,82.4),TEXT("InviteView"));UE_LOG(LogTemp,Display,TEXT("CONTENT35 review_start accepted=%d"),OK);return;}
 if(Event==943){
  UE_LOG(LogTemp,Display,TEXT("CONTENT35 review_state phase=%d cancelled=%d leg=%d looked=%d travel=%d paused=%d safe=%d task=%s player=%s emma=%s yaw=%.2f"),LeadPhase,LeadCancelled,LeadLeg,LeadLooked,OwnsTravel,C->LocoLifePaused,Safe(),*ActiveTask.ToString(),*C->GetPawn()->GetActorLocation().ToString(),*C->Emma()->GetActorLocation().ToString(),C->Emma()->GetActorRotation().Yaw);return;
 }
 if(Event==944){ReviewFollow=false;UE_LOG(LogTemp,Display,TEXT("CONTENT35 follow_assist disabled=1"));return;}
 if(Event==936 && SharedSkyTravelOwned && C.IsValid() && C->Emma() && C->LocoState!=ACourtyardController::ELocoState::Idle){
  const FVector P=C->LocoPointAt(C->LocoProgress+90);
  auto* Obstacle=GetWorld()->SpawnActor<AActor>();
  auto* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(30,30,78));
  Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));Box->RegisterComponent();Obstacle->SetActorLocation(P);Obstacle->Tags.Add(TEXT("Content34ReviewObstacle"));
  UE_LOG(LogTemp,Display,TEXT("CONTENT34 review_obstacle placed=%s explicit_transient_collision_fixture=1"),*P.ToString());return;
 }
 if(Event==937){for(TActorIterator<AActor> I(GetWorld());I;++I)if(I->ActorHasTag(TEXT("Content34ReviewObstacle")))I->Destroy();UE_LOG(LogTemp,Display,TEXT("CONTENT34 review_obstacle removed=1"));return;}
 if((Event==934 || Event==935) && C.IsValid() && C->GetPawn() && C->Emma()){
  // Explicit fixtures on the lawn where an earlier normal walk actually arrived.
  // These do not change the map, asset, normal timers or character motion.
  ReviewSetup(3);auto* A=Cast<ACharacter>(C->Emma());
  A->SetActorLocationAndRotation(FVector(-1100,-600,82.4),FRotator(0,-90,0),false);
  if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  C->GetPawn()->SetActorLocation(Event==934?FVector(-920,-600,82):FVector(-920,-850,82),false);
  C->SetControlRotation(FRotator(36,-90,0));
  StillSky=0;SharedSkyWindowUntil=0;ActiveTask=ArriveAction=NAME_None;
  NextQuietVisit=NextQuietSite=GetWorld()->GetTimeSeconds()+10000;
  Cooldowns.Add(TEXT("SkyTogether"),GetWorld()->GetTimeSeconds()-1);
  PreviousDistance=FVector::Dist2D(A->GetActorLocation(),C->GetPawn()->GetActorLocation());
  UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_fixture event=%d enabled=%d explicit_placement=1 old_schedulers_held=1"),Event,SharedSkyScenesEnabled);return;
 }
 if(Event==933 && C.IsValid() && C->GetPawn() && C->Emma()){
  const float Dist=FVector::Dist2D(C->GetPawn()->GetActorLocation(),C->Emma()->GetActorLocation());
  const float Heading=FMath::FindDeltaAngleDegrees(C->Emma()->GetActorRotation().Yaw,C->GetControlRotation().Yaw);
  UE_LOG(LogTemp,Display,TEXT("CONTENT34 sky_context dist=%.2f heading=%.2f pitch=%.2f still=%.2f safe=%d visible=%d open=%d travel=%d task=%s remaining_cooldown=%.2f player=%s emma=%s"),Dist,Heading,C->GetControlRotation().Pitch,StillSky,Safe(),PlayerVisible(),SharedSkyVisible(),OwnsTravel,*ActiveTask.ToString(),Cooldowns.FindRef(TEXT("SkyTogether"))-GetWorld()->GetTimeSeconds(),*C->GetPawn()->GetActorLocation().ToString(),*C->Emma()->GetActorLocation().ToString());return;
 }
 if((Event==931 || Event==932) && Environment.IsValid()){
  Environment->ReviewMeteorFraming=Event==931;
  UE_LOG(LogTemp,Display,TEXT("CONTENT33 meteor_framing enabled=%d review_only=1"),Environment->ReviewMeteorFraming);return;
 }
 if(Event>=927 && Event<=930 && Environment.IsValid()){
  // Deliberately place retained petals in the body volume to reproduce the
  // post-landing case. Never called by normal play; no model or scene writes.
  const bool Use=Event>=929;const bool Seed=Event==927 || Event==929;
  Environment->ReviewStationaryBodyPetals(C->Emma(),Use,Seed);return;
 }
 if(Event==925 && Environment.IsValid()){
  const FVector Goal=Environment->TreeCenter+FVector(430,150,82.4),Focus=Environment->TreeCenter+FVector(170,60,115);
  FCollisionQueryParams Q(SCENE_QUERY_STAT(QuietFlowerSpaceReview),false,C->Emma());
  const bool Blocked=GetWorld()->OverlapBlockingTestByChannel(Goal,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q);
  FHitResult Ground;const bool HasGround=GetWorld()->LineTraceSingleByChannel(Ground,Goal+FVector(0,0,100),Goal-FVector(0,0,150),ECC_Visibility,Q);
  UE_LOG(LogTemp,Display,TEXT("CONTENT31 blossom_space enabled=%d tree=%s goal=%s facing=%.1f blocked=%d ground_hit=%d ground=%s motion=%s"),QuietFlowerVisitsEnabled,*Environment->TreeCenter.ToString(),*Goal.ToString(),(Focus-Goal).Rotation().Yaw,Blocked,HasGround,*Ground.ImpactPoint.ToString(),*GetNameSafe(Motions.FindRef(TEXT("BlossomListen"))));return;
 }
 if(Event==926 && QuietFlowerVisitsEnabled){
  // Review-only isolate the new destination, without moving Emma or changing
  // the shipped wait intervals. This is not evidence of a natural selection.
  const double Now=GetWorld()->GetTimeSeconds();Cooldowns.Add(TEXT("PondWatch"),Now+180);Cooldowns.Add(TEXT("TerracePause"),Now+180);NextQuietVisit=Now+2;
  UE_LOG(LogTemp,Display,TEXT("CONTENT31 review_blossom_due test_only=1"));return;
 }
 if(Event==920){NextQuietVisit=GetWorld()->GetTimeSeconds()+2;UE_LOG(LogTemp,Display,TEXT("CONTENT30 review_quiet_visit_due test_only=1"));return;}
 if(Event==921){UE_LOG(LogTemp,Display,TEXT("CONTENT30 quiet_state enabled=%d pending=%d pause_owned=%d motion=%d site=%s task=%s paused=%d pos=%s yaw=%.1f"),QuietVisitsEnabled,QuietVisitPending,QuietArrivalOwnsPause,QuietMotion!=nullptr,*QuietVisitSite.ToString(),*ActiveTask.ToString(),C->LocoLifePaused,*C->Emma()->GetActorLocation().ToString(),C->Emma()->GetActorRotation().Yaw);return;}
 if(Event==922 && QuietVisitPending && OwnsTravel && C->GetPawn() && C->LocoState!=ACourtyardController::ELocoState::Idle){
  const FVector P=C->LocoPointAt(C->LocoProgress+100);
  C->GetPawn()->SetActorLocation(P,false);if(auto* A=Cast<ACharacter>(C->GetPawn())){A->GetCharacterMovement()->StopMovementImmediately();A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);}
  UE_LOG(LogTemp,Display,TEXT("CONTENT30 review_path_block placed=%s"),*P.ToString());return;
 }
 if(Event==923){
  const FVector Goals[]={FVector(0,-1000,82.4),FVector(0,-1820,82.4),FVector(420,-1410,82.4),FVector(-420,-1410,82.4),FVector(0,260,82.4)};
  FCollisionQueryParams Q(SCENE_QUERY_STAT(QuietVisitSpaceReview),false,C->Emma());
  for(int32 K=0;K<5;++K){
   TArray<FOverlapResult> Hits;GetWorld()->OverlapMultiByChannel(Hits,Goals[K],FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(24,78),Q);
   FString Names;for(const auto& H:Hits)if(H.bBlockingHit)Names+=GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent())+TEXT(";");
   FHitResult Ground;GetWorld()->LineTraceSingleByChannel(Ground,Goals[K]+FVector(0,0,150),Goals[K]-FVector(0,0,150),ECC_Visibility,Q);
   UE_LOG(LogTemp,Display,TEXT("CONTENT30 space_probe goal=%d blockers=%s ground=%s player_distance=%.1f safe=%d"),K,*Names,*Ground.ImpactPoint.ToString(),C->GetPawn()?FVector::Dist2D(Goals[K],C->GetPawn()->GetActorLocation()):-1.f,Safe());
  }return;
 }
 if(Event==924 && !OwnsTravel && C->GetPawn()){
  auto* A=Cast<ACharacter>(C->Emma());const FVector P=A->GetActorLocation()+A->GetActorForwardVector()*75+A->GetActorRightVector()*55;
  C->GetPawn()->SetActorLocation(P,false);if(auto* Player=Cast<ACharacter>(C->GetPawn()))Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  C->CourtyardReviewBodyView(0);C->CourtyardReviewLookAtEmma(0);
  UE_LOG(LogTemp,Display,TEXT("CONTENT30 review_interaction_observer placed=%s"),*P.ToString());return;
 }
 if(Event==913 && Environment.IsValid()){
  ReviewSetup(2);auto* A=Cast<ACharacter>(C->Emma());
  const FVector P=Environment->TreeCenter+FVector(430,150,82.4);
  A->SetActorLocationAndRotation(P,FRotator(0,180,0),false);
  if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  if(C->GetPawn()){C->GetPawn()->SetActorLocation(P+FVector(220,-260,0),false);C->SetControlRotation((P+FVector(0,0,30)-C->PlayerCameraManager->GetCameraLocation()).Rotation());}
  C->CourtyardReviewBodyView(0);
  UE_LOG(LogTemp,Display,TEXT("CONTENT29 petal_body_fixture tree=%s emma=%s no_animation_change=1"),*Environment->TreeCenter.ToString(),*P.ToString());return;
 }
 if(Event==916 && Environment.IsValid()){const bool OK=Travel(Environment->TreeCenter+FVector(300,150,82.4),180,TEXT("PetalWalkReview"),NAME_None);UE_LOG(LogTemp,Display,TEXT("CONTENT29 petal_real_walk accepted=%d"),OK);return;}
 if(Event==914 && Environment.IsValid()){
  const double Now=GetWorld()->GetTimeSeconds();DoorLastPhase=Now;
  SampleContactSurface(TEXT("PetalBody"));
  FString Csv=TEXT("instance,x,y,z,sx,sy,sz,pitch,yaw,roll\n");
  auto* Instances=Environment->Petals.Get();
  for(int32 K=0;K<Instances->GetInstanceCount();++K){FTransform T;if(!Instances->GetInstanceTransform(K,T,true))continue;const FVector P=T.GetLocation(),S=T.GetScale3D();const FRotator R=T.Rotator();Csv+=FString::Printf(TEXT("%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n"),K,P.X,P.Y,P.Z,S.X,S.Y,S.Z,R.Pitch,R.Yaw,R.Roll);}
  const FString File=FPaths::ProjectDir()/FString::Printf(TEXT("Audit/Content29_20261008/petals_%05d.csv"),FMath::RoundToInt(Now*100));
  FFileHelper::SaveStringToFile(Csv,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  auto* A=Cast<ACharacter>(C->Emma());
  UE_LOG(LogTemp,Display,TEXT("CONTENT29 petal_snapshot t=%.3f air=%d ground=%d avoided=%d count=%d body=%s radius=%.2f half=%.2f file=%s"),Now,Environment->Airborne,Environment->Landed,Environment->Avoided,Instances->GetInstanceCount(),*A->GetActorLocation().ToString(),A->GetCapsuleComponent()->GetScaledCapsuleRadius(),A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),*File);return;
 }

 if(Event>=910 && Event<=912){
  for(TActorIterator<ACourtyardContentDoor> It(GetWorld());It;++It){
   if(!It->FixEnabled || !It->PrivacyRoom)continue;
   if(Event==910){auto* Dummy=GetWorld()->SpawnActor<AActor>();const bool OK=It->SetOccupied(true,Dummy);UE_LOG(LogTemp,Display,TEXT("CONTENT29 privacy_fixture room=%s occupied=%d dummy_hidden=%d"),*It->GetName(),OK,Dummy&&Dummy->IsHidden());}
   if(Event==912){It->SetOccupied(false);continue;}
   if(Event!=911)continue;
   UE_LOG(LogTemp,Display,TEXT("CONTENT29 privacy_volume room=%s occupied=%d collision=%d pawn=%d bounds=%s extent=%s"),*It->GetName(),It->Occupied,int(It->PrivateVolume->GetCollisionEnabled()),int(It->PrivateVolume->GetCollisionResponseToChannel(ECC_Pawn)),*It->PrivateVolume->Bounds.Origin.ToString(),*It->PrivateVolume->Bounds.BoxExtent.ToString());
   // Probe the actual opaque shell, not the invisible occupancy blocker. A ray
   // that reaches room furniture is a visible doorway gap, not a privacy pass.
   const float Y=It->GetActorLocation().Y;const auto B=It->Leaf->Bounds;
   const float X[]={float(B.Origin.X),float(B.Origin.X-B.BoxExtent.X-1),float(B.Origin.X+B.BoxExtent.X+1)};
   for(int H=0;H<2;++H)for(int K=0;K<3;++K){
    const float Z=It->GetActorLocation().Z+(H?105.f:145.f);const FVector From(X[K],Y-70,Z),To(X[K],Y+135,Z);
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(Content29Privacy),true);Q.AddIgnoredActor(C->GetPawn());Q.AddIgnoredComponent(It->PrivateVolume.Get());
    const bool Block=GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Q);
    const bool Sealed=Block && Hit.ImpactPoint.Y<=Y+16;
    UE_LOG(LogTemp,Display,TEXT("CONTENT29 privacy_shell room=%s height=%d edge=%d sealed=%d blocked=%d from=%s hit=%s actor=%s component=%s"),*It->GetName(),H,K,Sealed,Block,*From.ToString(),*Hit.ImpactPoint.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
   }
  }
  return;
 }
 if(Event==810 && FixedStairMotion){
  auto* A=Cast<ACharacter>(C->Emma());A->GetMesh()->GetAnimInstance()->Montage_Stop(.10f,FixedStairMotion);
  UE_LOG(LogTemp,Display,TEXT("CONTENT28 injected_stair_clip_loss phase=%.3f"),FixedStairLastPhase);
 }
 if((Event==811 || Event==812) && Environment.IsValid()){
  Environment->ReviewPetalsEnabled=Event==812;Environment->Petals->SetVisibility(Event==812);
  UE_LOG(LogTemp,Display,TEXT("CONTENT28 petals_comparison enabled=%d"),Event==812);
 }
 if(Event==813){SeatingValidated=true;UE_LOG(LogTemp,Display,TEXT("CONTENT28 bench_qualification enabled=1 no_begin_or_reposition=1"));}
 if(Event==814)UE_LOG(LogTemp,Display,TEXT("CONTENT28 site_qualification accepted=%d"),BeginQuietSite());
 if(Event==815){QuietSitesEnabled=true;NextQuietSite=GetWorld()->GetTimeSeconds()+1;UE_LOG(LogTemp,Display,TEXT("CONTENT28 site_scheduler_due shortened_for_test=1"));}
 if(Event==790){ReviewSetup(0);auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(995,890,528.4),FRotator(0,180,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());C->GetPawn()->SetActorLocation(FVector(1100,1100,528.4),false);C->CourtyardReviewBodyView(4);}
 if(Event==791)UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX study_book_review accepted=%d"),BeginStudyBook());
 if(Event==792){DoorLastPhase=StudyPhase;SampleContactSurface(TEXT("StudyBook_r7"));if(StudyBook.IsValid())UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX study_book_transform phase=%.3f transform=%s"),StudyPhase,*StudyBook->GetActorTransform().ToString());if(StudyPage.IsValid())UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX study_page_transform phase=%.3f transform=%s"),StudyPhase,*StudyPage->GetActorTransform().ToString());}
 if(Event==760){ReviewSetup(0);ReadingSeatValidated=true;auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(740,815,82.4),FRotator(0,180,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());C->GetPawn()->SetActorLocation(FVector(800,540,82.4),false);}
 if(Event==761)UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX reading_review accepted=%d"),BeginReadingVisit());
 if(Event==762 && SeatMotion){Cast<ACharacter>(C->Emma())->GetMesh()->GetAnimInstance()->Montage_Stop(.2f,SeatMotion);}
 if(Event==763)UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seating_state reading=%d approach=%d seated=%d waking=%d fault=%d phase=%.3f pos=%s"),ReadingSeat,SeatApproachMotion!=nullptr,SeatMotion!=nullptr,ReadingWaking,SeatFault,SeatLastPhase,*C->Emma()->GetActorLocation().ToString());
 if(Event==764 && C->GetPawn()){C->GetPawn()->SetActorLocation(FVector(570,755,82.4),false);C->SetControlRotation(FRotator(0,180,0));C->SetViewTargetWithBlend(C->GetPawn(),0);}
 if(Event==765 && C->GetPawn()){C->GetPawn()->SetActorLocation(FVector(518,755,82.4),false);C->SetControlRotation(FRotator(-5,180,0));C->SetViewTargetWithBlend(C->GetPawn(),0);}
 if(Event==766){
  ReviewSetup(0);ReadingSeatValidated=true;const double Now=GetWorld()->GetTimeSeconds();
  auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(0,1150,82.4),FRotator(0,90,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
  C->GetPawn()->SetActorLocation(FVector(750,570,82.4),false);C->SetMood(TEXT("sleepy"),TEXT("reading_qualification_fixture"));C->MoodChangedAt=Now;C->MoodShiftAt=Now+360;
  C->LocoLifePaused=false;C->ReviewBodyHold=false;C->LocoNextDecision=C->LocoNextPace=Now+90;NextReadingSeat=0;
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX reading_qualification fixture=hall_n wait=75 no_direct_begin=1 old_depart_delayed=90"));
 }
 if(Event==780){ReviewSetup(0);auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(-1069,1691,528.4),FRotator(0,90,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());C->GetPawn()->SetActorLocation(FVector(-900,1560,528.4),false);C->CourtyardReviewBodyView(4);}
 if(Event==781){for(TActorIterator<ACourtyardContentDoor> It(GetWorld());It;++It)if(It->PrivacyRoom && !It->Bathroom){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX npc_door_review accepted=%d"),BeginDoorAction(*It));break;}}
 if(Event==782)SampleContactSurface(TEXT("DoorLatch_r5"));
 if(Event==783){if(!C->BodyReviewCamera)C->BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();const FVector Eye(-910,1630,607),Focus(-1069,1708,554);C->BodyReviewCamera->SetActorLocationAndRotation(Eye,(Focus-Eye).Rotation());C->BodyReviewCamera->GetCameraComponent()->SetFieldOfView(57);C->SetViewTargetWithBlend(C->BodyReviewCamera,0);}
 if(Event==784){if(!C->BodyReviewCamera)C->BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();const FVector Eye(-1185,1625,592),Focus(-1077,1709,558);C->BodyReviewCamera->SetActorLocationAndRotation(Eye,(Focus-Eye).Rotation());C->BodyReviewCamera->GetCameraComponent()->SetFieldOfView(57);C->SetViewTargetWithBlend(C->BodyReviewCamera,0);}
 if(Event>=300 && Event<=305){ReviewSetup(Event-300);return;}
 const TCHAR* Names[]={TEXT("WarmHands"),TEXT("SkyTogether"),TEXT("BlossomListen"),TEXT("WindowReflect"),TEXT("InviteView"),TEXT("Sneeze"),TEXT("RunnerChuckle"),TEXT("RunnerWave"),TEXT("RearNotice"),TEXT("RunnerChuckle")};
 if(Event>=50 && Event<=59){const FName Task=Event==59?FName(TEXT("FlowerOffering")):FName(Names[Event-50]);const bool OK=Play(Names[Event-50],Task);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT review_action event=%d accepted=%d safe=%d"),Event,OK,Safe());}
 if(Event==310){LeadGoal=FVector(350,1205,C->Emma()->GetActorLocation().Z);const FVector Q=C->Emma()->GetActorLocation();if(Travel(Q+(LeadGoal-Q).GetClampedToMaxSize(240),(LeadGoal-Q).Rotation().Yaw,TEXT("LeadInvitation"),TEXT("InviteView"))){LeadLeg=1;LeadExpires=TaskUntil=GetWorld()->GetTimeSeconds()+150;}}
 if(Event==311)ReviewFollow=true;if(Event==312)ReviewFollow=false;
 if(Event==320){SeatingValidated=true;Travel(FVector(-155,82,C->Emma()->GetActorLocation().Z),-90,TEXT("BenchApproach"),TEXT("BenchSit"));}
 if(Event==321){SeatingValidated=true;BeginSeat();}
 if(Event==324)ReviewSeatCamera=true;
 if(Event==325)SeatDue=GetWorld()->GetTimeSeconds();
 if(Event==326){ReviewSetup(0);SeatingValidated=true;auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(-95,-30,82.4),FRotator(0,90,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());}
 if(Event==327)BeginSeatApproach();
 if(Event==328){ReviewSetup(0);SeatingValidated=true;auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(-95,-390,82.4),FRotator(0,90,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());}
 if(Event==329)Travel(FVector(-95,-30,82.4),90,TEXT("BenchApproach"),TEXT("SeatApproach"));
 if(Event==330){HoldingFlower=true;ShowGift(true);}
 if(Event==331){BeginReceiveGift();}
 if(Event==332){FinishReceiveGift(false,TEXT("review_cleanup"));HoldingFlower=false;Gift->SetVisibility(false);}
 if(Event==333){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_state receiving=%d returning=%d held_by_player=%d visible=%d task=%s"),ReceiveGiftSince>=0,GiftReturning,HoldingFlower,Gift->IsVisible(),*ActiveTask.ToString());}
 if(Event==334){ReviewGiftWear=true;BeginReceiveGift();}
 if(Event==337){
  auto* A=Cast<ACharacter>(C->Emma());DoorLastPhase=ReceiveGiftMotion?A->GetMesh()->GetAnimInstance()->Montage_GetPosition(ReceiveGiftMotion):14;
  SampleContactSurface(TEXT("ReceivePin_r4"));
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX gift_surface phase=%.3f pinned=%d prop=%s head=%s"),DoorLastPhase,GiftPinned,*(GiftPinned?WornFlower:Gift)->GetComponentTransform().ToString(),*A->GetMesh()->GetSocketTransform(TEXT("頭")).ToString());
 }
 if(Event==335)SampleSeatSurface();
 if(Event==336)NextSeat=GetWorld()->GetTimeSeconds();
 if(Event==401 || Event==402){C->LastVisitHours=Event==401?96:192;ReunionPending=true;}
 if(Event==403){Company=481;Away=179;Cooldowns.Remove(TEXT("Abandoned"));}
 if(Event==405 && C->GetPawn()){Company=481;Away=179;CompanyPlayerAnchor=C->GetPawn()->GetActorLocation();Cooldowns.Remove(TEXT("Abandoned"));UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT review_stationary_company_fixture=1"));}
 if(Event==404){LastBody=TEXT("C9");C->LivingPetalFailed=true;C->LivelyBodyId=NAME_None;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT review_petal_result injected_failure=1"));}
 if(Event==393){
  APawn* P=C->GetPawn();if(!P)return;
  if(auto* A=Cast<ACharacter>(P))A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("ContentPickableFlower"))){
   P->SetActorLocation(It->GetActorLocation()+FVector(0,95,66),false);C->SetControlRotation(FRotator(-52,-90,0));break;
  }
 }
 if(Event==394){
  const FVector P=C->PlayerCameraManager->GetCameraLocation();
  for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("ContentPickableFlower"))){
   FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(C->GetPawn());const FVector V=It->GetActorLocation()-P;
   const bool Hit=GetWorld()->LineTraceSingleByChannel(H,P,It->GetActorLocation(),ECC_Visibility,Q);
   UE_LOG(LogTemp,Display,TEXT("CONTENT_FLOWER_DIAG name=%s distance=%.2f dot=%.4f hit=%s hitdistance=%.2f hidden=%d"),*It->GetName(),V.Size(),FVector::DotProduct(C->GetControlRotation().Vector(),V.GetSafeNormal()),*GetNameSafe(H.GetActor()),H.Distance,It->IsHidden());
  }
 }
 if(Event==392){
  APawn* P=C->GetPawn();if(!P)return;
  for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("PlayerSeatAnchor"))){
   FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(P);
   const FVector Cam=C->PlayerCameraManager->GetCameraLocation();
   const bool Hit=GetWorld()->SweepSingleByChannel(H,Cam,It->GetActorLocation(),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Q);
   auto* A=Cast<ACharacter>(P);
   UE_LOG(LogTemp,Display,TEXT("CONTENT_SEAT_DIAG distance=%.2f dot=%.3f los=%d sweep=%d hit=%s actorYaw=%.1f grounded=%d camera=%s"),P->GetDistanceTo(*It),P->GetHorizontalDotProductTo(*It),C->LineOfSightTo(*It),Hit,*GetNameSafe(H.GetActor()),P->GetActorRotation().Yaw,A&&A->GetCharacterMovement()->IsMovingOnGround(),*Cam.ToString());
  }
 }
 if(Event==391){APawn* P=C->GetPawn();if(P)for(TFieldIterator<FProperty> It(P->GetClass());It;++It){const FString N=It->GetName();if(N.Contains(TEXT("Seat"))){FString V;It->ExportText_InContainer(0,V,P,P,P,PPF_None);UE_LOG(LogTemp,Display,TEXT("CONTENT_SEAT_PROPERTY %s=%s"),*N,*V);}}}
 if(Event>=80 && Event<=85){for(TActorIterator<ACourtyardContentDoor> It(GetWorld());It;++It){if(It->Bathroom || !It->PrivacyRoom)continue;
   if(Event==80)It->RequestToggle(C->GetPawn());
   if(Event==81){auto* Dummy=GetWorld()->SpawnActor<AActor>();const bool OK=It->SetOccupied(true,Dummy);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT privacy_occupy accepted=%d"),OK);}
   if(Event==82){It->RequestToggle(C->GetPawn());It->Knock();}
   if(Event==83)It->SetOccupied(false);
   if(Event==84){FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(C->GetPawn());const FVector From=It->GetActorLocation()+FVector(90,-45,155),To=It->GetActorLocation()+FVector(-150,-45,155);const bool Block=GetWorld()->LineTraceSingleByChannel(H,From,To,ECC_Visibility,Q);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT privacy_ray blocked=%d actor=%s locked=%d"),Block,*GetNameSafe(H.GetActor()),It->Occupied);}
  }}
 if(Event==390){UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT state safe=%d task=%s travel=%d pose=%d emma=%s emotion=%s company=%.1f away=%.1f player_departure_cm=%.1f"),Safe(),*ActiveTask.ToString(),OwnsTravel,(int)C->Pose,*C->Emma()->GetActorLocation().ToString(),*Emotion.ToString(),Company,Away,C->GetPawn()?FVector::Dist2D(C->GetPawn()->GetActorLocation(),CompanyPlayerAnchor):0.);C->CourtyardReviewState(TEXT("content23"));}
 if(Event==410){
  auto* P=Cast<ACharacter>(C->GetPawn());if(!P)return;
  auto* Cap=P->GetCapsuleComponent();auto* Move=P->GetCharacterMovement();const FVector Start=P->GetActorLocation();FCollisionQueryParams Q;Q.AddIgnoredActor(P);
  FHitResult H;const bool Hit=GetWorld()->SweepSingleByChannel(H,Start,Start+C->GetControlRotation().Vector()*110,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(),Cap->GetScaledCapsuleHalfHeight()),Q);
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_diag pos=%s mode=%d step=%.2f hit=%d actor=%s comp=%s point=%s normal=%s stepup=%d penetrating=%d"),*Start.ToString(),int(Move->MovementMode),Move->MaxStepHeight,Hit,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString(),H.GetComponent()?int(H.GetComponent()->CanCharacterStepUpOn):-1,H.bStartPenetrating);
  for(int J=0;J<8;++J){const FVector V(102,1548+J*28,400);FHitResult Floor;GetWorld()->LineTraceSingleByChannel(Floor,V,V-FVector(0,0,500),ECC_Visibility,Q);UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX tread x=102 y=%.1f z=%.2f actor=%s"),V.Y,Floor.ImpactPoint.Z,*GetNameSafe(Floor.GetActor()));}
 }
 if(Event==411){auto Near=NearDoor();for(TActorIterator<ACourtyardContentDoor> It(GetWorld());It;++It){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX door_diag name=%s actor=%s leaf=%s bounds=%s extent=%s handle=%s near=%d open=%d angle=%.2f"),*It->GetName(),*It->GetActorLocation().ToString(),*It->Leaf->GetComponentTransform().ToString(),*It->Leaf->Bounds.Origin.ToString(),*It->Leaf->Bounds.BoxExtent.ToString(),*It->HandlePoint().ToString(),Near.Get()==*It,It->Open,It->Angle);}}
 if(Event==412 && Environment.IsValid()){auto* E=Environment.Get();UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX fire_diag actor=%s fire=%s light=%s intensity=%.2f visible=%d"),*E->GetActorTransform().ToString(),*E->FireVolume->GetComponentTransform().ToString(),*E->HearthLight->GetComponentLocation().ToString(),E->HearthLight->Intensity,E->HearthLight->IsVisible());}
 if((Event==413 || Event==414) && Environment.IsValid())Environment->HearthLight->SetCastShadows(Event==414);
 if((Event==430 || Event==431) && Environment.IsValid())Environment->HearthLight->SetVisibility(Event==431);
 if(Event==420){auto* A=Cast<ACharacter>(C->Emma());if(!A)return;auto* M=A->GetMesh();const auto& MorphNames=M->GetSkeletalMeshAsset()->GetMorphTargets();for(int N=0;N<FMath::Min(72,MorphNames.Num());++N){const int32* Index=M->ActiveMorphTargets.Find(MorphNames[N]);const float W=Index&&M->MorphTargetWeights.IsValidIndex(*Index)?M->MorphTargetWeights[*Index]:0.f;if(W>.001 || N==17 || N==38 || N==44)UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX final_morph number=%d name=%s weight=%.4f"),N+1,*MorphNames[N]->GetName(),W);}}
 if(Event==500 || Event==501){ReviewSetup(0);auto* A=Cast<ACharacter>(C->Emma());const bool Down=Event==501;A->SetActorLocationAndRotation(Down?FVector(-102,1574,528.4):FVector(102,1530,82.4),FRotator(0,90,0),false);A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());}
 if(Event==502 || Event==503){const bool OK=BeginFixedStairs(Event==503);UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_review_accepted=%d"),OK);}
 if(Event==504 || Event==505)ReviewStairCamera=Event==504;
 if(Event==524){ReviewStairCamera=true;ReviewStairFeetCamera=true;}
 if(Event==506){ReviewSetup(0);StairsValidated=true;auto* A=Cast<ACharacter>(C->Emma());A->SetActorLocationAndRotation(FVector(102,1150,82.4),FRotator(0,90,0),false);if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());}
 if(Event==507)BeginFloorJourney();
 if(Event==508 && FloorJourney==6)FloorDue=GetWorld()->GetTimeSeconds();
 if(Event==520 && FixedStairMotion && C->GetPawn()){
  auto* A=Cast<ACharacter>(C->Emma());auto* I=A->GetMesh()->GetAnimInstance();
  auto* Seq=Cast<UAnimSequence>(FixedStairMotion->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference());
  const float T=I->Montage_GetPosition(FixedStairMotion);
  FVector Delta;bool ClearFixture=false;
  for(float Ahead=2;Ahead<8;Ahead+=.25f){
   const float Future=FMath::Min(T+Ahead,Seq->GetPlayLength()-.01f);
   Delta=FixedStairRootToWorld.RotateVector(Seq->ExtractRootMotionFromRange(0,Future).GetTranslation()-Seq->ExtractRootMotionFromRange(0,T).GetTranslation());
   if(Delta.Size2D()>95){ClearFixture=true;break;}
  }
  if(!ClearFixture){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX review_player_block refused=no_clear_ahead_fixture"));return;}
  C->GetPawn()->SetActorLocation(A->GetActorLocation()+Delta,false);
  if(auto* P=Cast<ACharacter>(C->GetPawn()))P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX review_player_block placed phase=%.3f at=%s"),T,*C->GetPawn()->GetActorLocation().ToString());
 }
 if(Event==521 && C->GetPawn()){C->GetPawn()->SetActorLocation(FVector(400,1000,82.4),false);UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX review_player_block removed"));}
 if(Event==522){UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX recovery_state floor=%d failures=%d stairs=%d paused=%d phase=%.3f pos=%s"),FloorJourney,FloorFailures,FixedStairMotion!=nullptr,FixedStairWaiting,FixedStairLastPhase,*C->Emma()->GetActorLocation().ToString());}
 if(Event==523 && C->Emma()->GetActorLocation().Z>400){EndFloorJourney(TEXT("review_persistent_path_failure"));}
 if(Event==510){
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return;
  const FVector Here=C->Emma()->GetActorLocation();
  for(const FVector Point:{Here,Here-FVector(0,0,80),FVector(-102,1460,446),FVector(-350,1480,446),FVector(350,1200,446)}){
   FNavLocation P;const bool OK=Nav->ProjectPointToNavigation(Point,P,FVector(70,70,140));
   UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_nav_probe point=%s projected=%d result=%s"),*Point.ToString(),OK,*P.Location.ToString());
  }
  auto* P=UNavigationSystemV1::FindPathToLocationSynchronously(this,Here,FVector(-350,1480,528.4),C->Emma());
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_nav_path valid=%d partial=%d count=%d"),P&&P->IsValid(),P&&P->IsPartial(),P?P->PathPoints.Num():0);
  if(P)for(const FVector V:P->PathPoints)UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX floor_nav_vertex %s"),*V.ToString());
 }
 if(Event==600 && C->GetPawn()){
  ReviewRoomEndYaw=90;
  ReviewRoomPath={FVector(102,1985,305),FVector(-102,1985,305),FVector(-102,1480,528),FVector(-350,1480,528),FVector(-350,1668,528),FVector(-748,1668,528)};
  ReviewRoomLast=C->GetPawn()->GetActorLocation();ReviewRoomBlocked=0;
 }
 if(Event>=601 && Event<=606 && C->GetPawn()){
  ReviewRoomEndYaw=90;
  if(Event==601)ReviewRoomPath={FVector(-748,1668,528),FVector(-1062,1668,528)};
  if(Event==602)ReviewRoomPath={FVector(-1062,1850,528)};
  if(Event==603)ReviewRoomPath={FVector(-1062,1668,528)};
  if(Event==604){ReviewRoomPath={FVector(-784,1668,528)};ReviewRoomEndYaw=-90;}
  if(Event==605){ReviewRoomPath={FVector(-784,1480,528)};ReviewRoomEndYaw=90;}
  if(Event==606)ReviewRoomPath={FVector(-784,1668,528)};
  ReviewRoomLast=C->GetPawn()->GetActorLocation();ReviewRoomBlocked=0;
 }
}
void ACourtyardContentLife::TickReviewRoomWalk(float Dt)
{
 if(!ReviewRoomPath.Num() || !C.IsValid() || !C->GetPawn())return;
 auto* P=Cast<ACharacter>(C->GetPawn());if(!P)return;
 const FVector Here=P->GetActorLocation(),To=ReviewRoomPath[0]-Here;const float Distance=To.Size2D();
 if(Distance<6 && FMath::Abs(To.Z)<8){
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX player_room_waypoint remaining=%d position=%s mode=%d"),ReviewRoomPath.Num(),*Here.ToString(),int(P->GetCharacterMovement()->MovementMode));
  ReviewRoomPath.RemoveAt(0);ReviewRoomBlocked=0;
  if(!ReviewRoomPath.Num()){P->GetCharacterMovement()->StopMovementImmediately();C->SetControlRotation(FRotator(-28,ReviewRoomEndYaw,0));}return;
 }
 if(FVector::Dist2D(Here,ReviewRoomLast)<Dt*2)ReviewRoomBlocked+=Dt;else ReviewRoomBlocked=0;
 ReviewRoomLast=Here;
 if(ReviewRoomBlocked>2){UE_LOG(LogTemp,Warning,TEXT("CONTENT_FIX player_room_walk_blocked position=%s target=%s"),*Here.ToString(),*ReviewRoomPath[0].ToString());ReviewRoomPath.Empty();return;}
 C->SetControlRotation(FMath::RInterpTo(C->GetControlRotation(),FRotator(0,To.Rotation().Yaw,0),Dt,3));
 P->AddMovementInput(To.GetSafeNormal2D(),FMath::Clamp(Distance/32.f,.2f,1.f));
}
