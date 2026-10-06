// Review-only deformed surface export. Never called in normal play or performance runs.
#include "CourtyardController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "SkeletalRenderPublic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void ACourtyardController::CourtyardReviewMeshProbe(FString Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
 const TSet<FString> Allowed={TEXT("P3Prayer"),TEXT("P3Firefly"),TEXT("P3PointMeteor"),TEXT("P3PinchMoon"),TEXT("P3SmellFlower"),TEXT("P3Sneeze"),TEXT("TraceStars"),TEXT("AcceptedClap"),TEXT("P3Prayer2"),TEXT("P3Firefly2"),TEXT("P3PointMeteor2"),TEXT("P3PinchMoon2"),TEXT("P3PointMeteor3"),TEXT("P3PinchMoon3")};if(!Allowed.Contains(Name))return;
 auto* A=Cast<ACharacter>(Emma());if(!A || !A->GetMesh())return;
 FString Path=TEXT("/Game/Emma/Animations/Lively20261003/A_Emma_Lively_")+Name;
 if(Name==TEXT("TraceStars"))Path=TEXT("/Game/Emma/Animations/LivingSix20261001/A_Emma_TraceStars_Station2");
 if(Name==TEXT("AcceptedClap"))Path=TEXT("/Game/Emma/Animations/GestureRepair20261001/A_Emma_BodyClap_Intent2");
 auto* Anim=LoadObject<UAnimSequence>(nullptr,*Path);if(!Anim)return;
 const FString Dir=FPaths::ProjectDir()/TEXT("Audit/Lively20261003/Surface")/Name;
 if(IFileManager::Get().DirectoryExists(*Dir))return;
 IFileManager::Get().MakeDirectory(*Dir,true);
 auto* Host=GetWorld()->SpawnActor<AActor>();auto* M=NewObject<USkeletalMeshComponent>(Host);Host->SetRootComponent(M);
 M->SetSkeletalMeshAsset(A->GetMesh()->GetSkeletalMeshAsset());M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCastShadow(false);M->RegisterComponent();
 M->SetWorldLocation(FVector(0,0,-5000));M->PlayAnimation(Anim,false);
 auto* RD=M->GetSkeletalMeshRenderData();if(!RD || !RD->LODRenderData.Num()){Host->Destroy();return;}
 const auto& L=RD->LODRenderData[0];const auto* Weights=L.GetSkinWeightVertexBuffer();const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();
 FString Labels=TEXT("index,bone\n");
 for(const auto& S:L.RenderSections)for(uint32 J=0;J<S.NumVertices;++J){const uint32 V=S.BaseVertexIndex+J;uint16 Best=0;int32 Bone=0;
  for(uint32 K=0;K<Weights->GetMaxBoneInfluences();++K){const auto W=Weights->GetBoneWeight(V,K);if(W>Best){Best=W;const auto B=Weights->GetBoneIndex(V,K);if(S.BoneMap.IsValidIndex(B))Bone=S.BoneMap[B];}}
  Labels+=FString::Printf(TEXT("%u,%s\n"),V,*Ref.GetBoneName(Bone).ToString());
 }
 FFileHelper::SaveStringToFile(Labels,*(Dir/TEXT("labels.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 TArray<uint32> Indices;const auto* IB=L.MultiSizeIndexContainer.GetIndexBuffer();for(int32 I=0;I<IB->Num();++I)Indices.Add(IB->Get(I));
 FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)Indices.GetData(),Indices.Num()*sizeof(uint32)),*(Dir/TEXT("triangles.bin")));
 for(int32 Frame=0;Frame<=40;++Frame){const float Time=FMath::Min(Frame*.2f,7.99f);M->SetPosition(Time,false);M->TickAnimation(0,false);M->RefreshBoneTransforms();
  TArray<FFinalSkinVertex> Vertices;M->GetCPUSkinnedVertices(Vertices,0);TArray<FVector3f> Pos;Pos.Reserve(Vertices.Num());for(const auto& V:Vertices)Pos.Add(V.Position);
  FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)Pos.GetData(),Pos.Num()*sizeof(FVector3f)),*(Dir/FString::Printf(TEXT("%03d.bin"),Frame)));
 }
 Host->Destroy();UE_LOG(LogTemp,Display,TEXT("COURTYARD_SURFACE name=%s samples=41 dir=%s"),*Name,*Dir);
}
