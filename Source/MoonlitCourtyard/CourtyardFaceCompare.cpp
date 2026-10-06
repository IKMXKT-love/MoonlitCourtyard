// G6 isolated review comparison. No persistent scene or default material change.
#include "CourtyardController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LightComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

void ACourtyardController::CourtyardReviewFaceLight(int32 Mode)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 auto* A=Cast<ACharacter>(Emma());auto* M=A?A->GetMesh():nullptr;if(!M)return;
 if(ReviewFaceLight.IsValid()){ReviewFaceLight->Destroy();ReviewFaceLight.Reset();M->SetLightingChannels(ReviewFaceChannels&1,ReviewFaceChannels&2,ReviewFaceChannels&4);}
 if(ReviewFaceOriginals.Num()){
  for(const auto& P:ReviewFaceOriginals)M->SetMaterial(P.Key,P.Value);
  FaceShadingInstances=ReviewFaceInstances;ReviewFaceInstances.Empty();ReviewFaceOriginals.Empty();
 }
 if(Mode==1){
  // Channels are shared project-wide. Refuse to reuse channel 2 if any unrelated component uses it.
  for(TActorIterator<AActor> It(GetWorld());It;++It){TInlineComponentArray<UPrimitiveComponent*> Prims;It->GetComponents(Prims);
   for(auto* C:Prims)if(C!=M && C->LightingChannels.bChannel2){UE_LOG(LogTemp,Warning,TEXT("COURTYARD_FACE_COMPARE refused channel2 primitive=%s"),*C->GetName());return;}
   TInlineComponentArray<ULightComponent*> Lights;It->GetComponents(Lights);for(auto* C:Lights)if(C->LightingChannels.bChannel2){UE_LOG(LogTemp,Warning,TEXT("COURTYARD_FACE_COMPARE refused channel2 light=%s"),*C->GetName());return;}
  }
  ReviewFaceChannels=(M->LightingChannels.bChannel0?1:0)|(M->LightingChannels.bChannel1?2:0)|(M->LightingChannels.bChannel2?4:0);
  M->SetLightingChannels(M->LightingChannels.bChannel0,M->LightingChannels.bChannel1,true);
  auto* Host=GetWorld()->SpawnActor<AActor>();auto* Light=NewObject<UPointLightComponent>(Host);Host->SetRootComponent(Light);Light->SetMobility(EComponentMobility::Movable);Light->SetLightingChannels(false,false,true);
  Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(95);Light->SetAttenuationRadius(240);Light->SetSourceRadius(35);Light->SetCastShadows(false);Light->SetLightColor(FLinearColor(.75f,.83f,1.f));Light->RegisterComponent();
  FVector E,F;if(FaceFrame(E,F))Host->SetActorLocation(E+F*90+FVector(0,0,35));ReviewFaceLight=Host;
 }else if(Mode==2){
  ReviewFaceInstances=FaceShadingInstances;FaceShadingInstances.Empty();
  const TCHAR* Names[]={TEXT("Face"),TEXT("Face_NoneEdge"),TEXT("Face_Eyelid"),TEXT("Face_Tere")};
  for(int32 Slot=0;Slot<M->GetNumMaterials();++Slot){auto* Old=M->GetMaterial(Slot);const UMaterial* Base=Old?Old->GetMaterial():nullptr;if(!Base)continue;
   for(const TCHAR* N:Names)if(Base->GetName()==FString(TEXT("M_WelcomeFace_"))+N){
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/Emma/Animations/Lively20261004/FaceCompare2/M_Compare_"))+N));if(!Mat)continue;
    auto* Instance=UMaterialInstanceDynamic::Create(Mat,M);Instance->SetScalarParameterValue(TEXT("FaceFlatten"),FaceFlatten);Instance->SetScalarParameterValue(TEXT("ReviewFaceGain"),1.22f);
    ReviewFaceOriginals.Add(Slot,Old);M->SetMaterial(Slot,Instance);FaceShadingInstances.Add(Instance);
   }
  }
 }
 TickFaceShading();UE_LOG(LogTemp,Display,TEXT("COURTYARD_FACE_COMPARE mode=%d changed_slots=%d review_only=1"),Mode,ReviewFaceOriginals.Num());
}
