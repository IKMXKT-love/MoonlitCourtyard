#include "CourtyardController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "UObject/UnrealType.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace {
void WelcomeBool(UObject* A,FName N,bool V){if(A)if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,V);}
}

// One greeting per visit: she notices the arriving player and turns her upper body
// toward them while waving. Feet and actor yaw never change, so stations stay valid.
void ACourtyardController::TickWelcome(){
 if(WelcomeDone)return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now<2.4)return;
 if(Now>25){WelcomeDone=true;UE_LOG(LogTemp,Display,TEXT("COURTYARD_WELCOME skipped no_eligible_view t=%.2f"),Now);return;}
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;auto* I=M?M->GetAnimInstance():nullptr;
 if(!I || !PlayerCharacter || Page!=ECourtyardPage::Game || LivingTrip || ActiveAmbient || I->IsAnyMontagePlaying() || PoseTransition || IsSeatBusy()
  || BPBool(A,TEXT("TravelActive")) || BPBool(A,TEXT("Interrupted")) || !BPBool(A,TEXT("AllowInteraction")) || A->GetVelocity().Size()>2)return;
 FVector E,F,V;FRotator R;if(!FaceFrame(E,F))return;GetPlayerViewPoint(V,R);
 const FVector D=V-E;const float Distance=D.Size2D();
 if(Distance<120 || Distance>420)return;
 // Only greet when the player is actually looking her way, so the gesture is seen.
 if(FVector::DotProduct(R.Vector().GetSafeNormal2D(),(-D).GetSafeNormal2D())<FMath::Cos(FMath::DegreesToRadians(32.f)) || !VisibleTarget(V,PlayerCharacter))return;
 const FVector N=D.GetSafeNormal2D();
 // Choose the greeting from her body's facing, not her head: the gaze layer already turns her head toward
 // the player, which made every opening look "straight ahead" and replaced the accepted turning wave.
 const FVector B=A->GetActorForwardVector().GetSafeNormal2D();
 const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(B,N).Z,FVector::DotProduct(B,N)));
 const float HeadYaw=FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F,N).Z,FVector::DotProduct(F,N)));
 UAnimMontage* Motion=nullptr;int32 Variant=INDEX_NONE;
 // The turning wave is authored for a player about 45 degrees to her left.
 if(WelcomeMotion && Yaw<-28 && Yaw>-62){Motion=WelcomeMotion;Variant=1;}
 else if(FMath::Abs(Yaw)<24 && BodyMotions.IsValidIndex(0) && BodyMotions[0]){Motion=BodyMotions[0];Variant=0;}
 if(!Motion)return;
 if(LivelyOn(TEXT("D6")) && Variant==1 && LastVisitHours>=72)Motion=LivelyMotion(TEXT("Reunion"),Motion);
 else if(LivelyOn(TEXT("B1")) && Variant==1 && Mood==TEXT("lonely"))Motion=LivelyMotion(TEXT("WelcomeLonely"),Motion);
 else if(LivelyOn(TEXT("B1")) && Variant==1 && Mood==TEXT("calm") && FMath::FRand()<.4f)Motion=LivelyMotion(TEXT("WelcomeShy"),Motion);
 WelcomeBool(A,TEXT("AllowInteraction"),false);
 if(I->Montage_Play(Motion,1.f,EMontagePlayReturnType::MontageLength,0.f,false)<=0){WelcomeBool(A,TEXT("AllowInteraction"),true);return;}
 ActiveAmbient=Motion;AmbientActor=A;AmbientBehavior=40+Variant;WelcomeDone=true;
 if(LivelyOn(TEXT("D6")) && Motion==LivelyMotion(TEXT("Reunion"),nullptr))LivelyStarted(TEXT("D6"),TEXT("welcome_reunion"),true);
 else if(LivelyOn(TEXT("B1")) && (Motion==LivelyMotion(TEXT("WelcomeLonely"),nullptr) || Motion==LivelyMotion(TEXT("WelcomeShy"),nullptr)))LivelyStarted(TEXT("B1"),TEXT("welcome_mood"),true);
 GreetingArmed=false;GreetingArrival=-1;NextGreeting=Now+FMath::FRandRange(130.f,200.f);
 NextBody=FMath::Max(NextBody,Now+45);BodyCooldown[0]=Now+FMath::FRandRange(130.f,220.f);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_WELCOME begin variant=%d yaw=%.1f head_yaw=%.1f distance=%.1f t=%.2f"),Variant,Yaw,HeadYaw,Distance,Now);
}

// Expression morphs carry normals recomputed from the deformed face, so closed eyes and an open
// mouth shaded as grey bulges under the scene lights. The face copies blend toward a spherical
// head normal (classic anime face shading); originals are untouched and only this mesh is swapped.
void ACourtyardController::TickFaceShading(){
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;if(!M)return;
 if(!FaceShadingReady){
  FaceShadingReady=true;
  const TCHAR* Originals[]={TEXT("M_Probe_SakurabaEma_Face"),TEXT("M_Probe_SakurabaEma_Face_NoneEdge"),TEXT("M_Probe_SakurabaEma_Face_Eyelid"),TEXT("M_Probe_SakurabaEma_Face_Tere")};
  for(int32 Slot=0;Slot<M->GetNumMaterials();++Slot){
   UMaterialInterface* Current=M->GetMaterial(Slot);const UMaterial* Base=Current?Current->GetMaterial():nullptr;if(!Base)continue;
   for(int32 K=0;K<4;++K)if(Base->GetName()==Originals[K] && FaceShadingMaterials.IsValidIndex(K) && FaceShadingMaterials[K]){
    auto* Instance=UMaterialInstanceDynamic::Create(FaceShadingMaterials[K],M);Instance->SetScalarParameterValue(TEXT("FaceFlatten"),FaceFlatten);
    M->SetMaterial(Slot,Instance);FaceShadingInstances.Add(Instance);
   }
  }
  UE_LOG(LogTemp,Display,TEXT("COURTYARD_FACESHADING slots=%d flatten=%.2f"),FaceShadingInstances.Num(),FaceFlatten);
 }
 if(FaceShadingInstances.IsEmpty())return;
 // Centre of the skull: the head bone sits at the top of the neck.
 const FVector Centre=M->GetSocketLocation(TEXT("頭"))+FVector(0,0,FaceCentreHeight);
 for(UMaterialInstanceDynamic* Instance:FaceShadingInstances)if(Instance)Instance->SetVectorParameterValue(TEXT("HeadCenter"),FLinearColor(Centre.X,Centre.Y,Centre.Z,0));
}
void ACourtyardController::CourtyardReviewFaceFlat(float Amount){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 FaceFlatten=FMath::Clamp(Amount,0.f,1.f);for(UMaterialInstanceDynamic* Instance:FaceShadingInstances)if(Instance)Instance->SetScalarParameterValue(TEXT("FaceFlatten"),FaceFlatten);
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_FACESHADING review flatten=%.2f"),FaceFlatten);
}

void ACourtyardController::CourtyardReviewFaceCenter(float Height){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 FaceCentreHeight=FMath::Clamp(Height,-60.f,30.f);UE_LOG(LogTemp,Display,TEXT("COURTYARD_FACESHADING review centre=%.1f"),FaceCentreHeight);
}

// Review only: play a lab montage on Emma (expression previews); never used by normal play.
void ACourtyardController::CourtyardReviewPlay(FString Path){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 auto* A=Cast<ACharacter>(Emma());auto* I=A&&A->GetMesh()?A->GetMesh()->GetAnimInstance():nullptr;if(!I)return;
 if(Path==TEXT("Stop")){I->StopAllMontages(.3f);return;}
 // "Path@seconds" holds the montage paused at that time so screenshot stalls cannot drift it.
 FString Asset=Path,At;float Hold=-1;if(Path.Split(TEXT("@"),&Asset,&At))Hold=FCString::Atof(*At);
 auto* Motion=LoadObject<UAnimMontage>(nullptr,*Asset);bool Played=false;
 if(Motion&&(ExpressionRemapOn||BodyRemapOn))ApplyExpressionRemap(ExpressionRemapLevel); // a montage loaded only now still gets the remapped face
 if(Motion && I->Montage_IsActive(Motion) && Hold>=0){I->Montage_SetPosition(Motion,Hold);I->Montage_Pause(Motion);Played=true;}
 else{I->StopAllMontages(0.f);Played=Motion && I->Montage_Play(Motion,1.f,EMontagePlayReturnType::MontageLength,FMath::Max(0.f,Hold),true)>0;if(Played && Hold>=0)I->Montage_Pause(Motion);}
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_REVIEWPLAY %s hold=%.2f played=%d pos=%.2f t=%.2f"),*Asset,Hold,Played,Motion?I->Montage_GetPosition(Motion):-1.f,GetWorld()->GetTimeSeconds());
}
// Review only: close camera in front of her eyes.
void ACourtyardController::CourtyardReviewFaceCam(float Distance){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 FVector E,F;if(!FaceFrame(E,F))return;
 if(!BodyReviewCamera)BodyReviewCamera=GetWorld()->SpawnActor<ACameraActor>();if(!BodyReviewCamera)return;
 const FVector Pos=E+F*FMath::Clamp(Distance,30.f,300.f)+FVector(0,0,3);
 BodyReviewCamera->SetActorLocationAndRotation(Pos,(E-FVector(0,0,4)-Pos).Rotation());BodyReviewCamera->GetCameraComponent()->SetFieldOfView(32);SetViewTargetWithBlend(BodyReviewCamera,0);
}

// Review only: radial visibility rays at picture height on both floors.
void ACourtyardController::CourtyardReviewWallProbe(){
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(WallProbe),true);if(GetPawn())Q.AddIgnoredActor(GetPawn());if(Emma())Q.AddIgnoredActor(Emma());
 int32 Points=0;
 for(float Floor:{0.f,446.f})for(float H:{140.f,215.f})for(int32 X=-1100;X<=1100;X+=150)for(int32 Y=380;Y<=2130;Y+=150){
  const FVector P(X,Y,Floor+H);
  if(GetWorld()->OverlapAnyTestByChannel(P,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Q))continue;
  ++Points;
  for(int32 Angle=0;Angle<360;Angle+=10){
   const float Rad=FMath::DegreesToRadians(float(Angle));const FVector Dir(FMath::Cos(Rad),FMath::Sin(Rad),0);FHitResult Hit;
   if(GetWorld()->LineTraceSingleByChannel(Hit,P,P+Dir*900,ECC_Visibility,Q))
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_PROBE %d %d %d %d %.1f %s %s %.3f %.3f %.1f %.1f"),X,Y,int32(Floor+H),Angle,Hit.Distance,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.ImpactNormal.X,Hit.ImpactNormal.Y,Hit.ImpactPoint.X,Hit.ImpactPoint.Y);
  }
 }
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_PROBE done points=%d"),Points);
}

// Review only: verify candidate wall rectangles from -WallSpots=<file>.
// Each line: name x y z nx ny width height (point on the wall, normal into the room).
void ACourtyardController::CourtyardReviewWallSpots(){
 FString Path;if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")) || !FParse::Value(FCommandLine::Get(),TEXT("WallSpots="),Path))return;
 TArray<FString> Lines;if(!FFileHelper::LoadFileToStringArray(Lines,*Path)){UE_LOG(LogTemp,Warning,TEXT("COURTYARD_SPOT missing %s"),*Path);return;}
 FCollisionQueryParams Q(SCENE_QUERY_STAT(WallSpots),true);if(GetPawn())Q.AddIgnoredActor(GetPawn());if(Emma())Q.AddIgnoredActor(Emma());
 for(const FString& L:Lines){
  TArray<FString> T;L.ParseIntoArrayWS(T);if(T.Num()<8)continue;
  const FVector C(FCString::Atof(*T[1]),FCString::Atof(*T[2]),FCString::Atof(*T[3]));const FVector N=FVector(FCString::Atof(*T[4]),FCString::Atof(*T[5]),0).GetSafeNormal();
  const float W=FCString::Atof(*T[6]),H=FCString::Atof(*T[7]);const FVector Side=FVector::CrossProduct(FVector::UpVector,N);
  for(int32 Row=0;Row<5;++Row)for(int32 Col=0;Col<7;++Col){
   const FVector S=C+Side*(W*(Col/6.f-.5f))+FVector(0,0,H*(Row/4.f-.5f));FHitResult In,Out;
   const bool HitIn=GetWorld()->LineTraceSingleByChannel(In,S+N*45,S-N*35,ECC_Visibility,Q);
   const bool HitOut=GetWorld()->LineTraceSingleByChannel(Out,S+N*4,S+N*90,ECC_Visibility,Q);
   UE_LOG(LogTemp,Display,TEXT("COURTYARD_SPOT %s %d %d %d %.1f %s %d %.1f %s"),*T[0],Row,Col,HitIn,HitIn?In.Distance:-1.f,*GetNameSafe(In.GetActor()),HitOut,HitOut?Out.Distance:-1.f,*GetNameSafe(Out.GetActor()));
  }
 }
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_SPOT done"));
}
