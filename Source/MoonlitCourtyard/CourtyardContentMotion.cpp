#include "CourtyardContentLife.h"
#include "CourtyardController.h"
#include "CourtyardContentDoor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "SkeletalRenderPublic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
void ACourtyardContentLife::SampleContactSurface(const FString& Label)
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!A)return;
 auto* M=A->GetMesh();TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);TArray<FVector3f> P;
 for(const auto& X:V)P.Add(FVector3f(M->GetComponentTransform().TransformPosition(FVector(X.Position))));
 const FString Dir=FPaths::ProjectDir()/TEXT("Audit/ContentFix20261007/Continue20261008");
 const FString File=Dir/FString::Printf(TEXT("%s_%05d_body.bin"),*Label,FMath::RoundToInt(DoorLastPhase*100));
 FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)P.GetData(),P.Num()*sizeof(FVector3f)),*File);
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX contact_surface name=%s phase=%.3f vertices=%d actor=%s mesh=%s handle=%s"),*Label,DoorLastPhase,V.Num(),*A->GetActorTransform().ToString(),*M->GetComponentTransform().ToString(),ActionDoor.IsValid()?*ActionDoor->HandlePoint().ToString():TEXT("none"));
}
void ACourtyardContentLife::ReviewMotion(int32 Kind)
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!A || ReviewFlight)return;
 auto* I=A->GetMesh()->GetAnimInstance();if(!I)return;
 if(C->LocoState!=ACourtyardController::ELocoState::Idle)C->LocoEnd(false,TEXT("content_review"));
 I->Montage_Stop(.2f);C->ActiveAmbient=nullptr;C->LivingBehavior=-1;C->LivingTrip=false;
 ReviewWasPaused=C->LocoLifePaused;C->LocoLifePaused=true;C->WelcomeDone=true;
 for(const TCHAR* N:{TEXT("TravelActive"),TEXT("AllowInteraction")})if(auto* P=FindFProperty<FBoolProperty>(A->GetClass(),N))P->SetPropertyValue_InContainer(A,false);
 ReviewCapsuleHalf=A->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();ReviewMeshOffset=A->GetMesh()->GetRelativeLocation();
 A->GetCapsuleComponent()->SetCapsuleHalfHeight(40);A->GetMesh()->SetRelativeLocation(ReviewMeshOffset);A->GetCharacterMovement()->SetMovementMode(MOVE_Flying);A->GetCharacterMovement()->StopMovementImmediately();
 const bool Down=Kind==1;A->SetActorLocation(Kind==2?FVector(-155,82,82):Down?FVector(102,1910,305):FVector(102,1546,82),false);A->SetActorRotation(FRotator(0,(Down || Kind==2)?-90:90,0));
 if(A->GetController())A->GetController()->SetControlRotation(A->GetActorRotation());
 const FString Path=Kind==2?FString(TEXT("/Game/Content20261007/Motions/AM_SitCycle_r3")):TEXT("/Game/Content20261007/Motions/AM_StairFlight")+(Down?FString(TEXT("Down_r3")):FString(TEXT("Up_r3")));
 ReviewFlight=LoadObject<UAnimMontage>(nullptr,*Path);ReviewFlightStart=GetWorld()->GetTimeSeconds();A->SetAnimRootMotionTranslationScale(1);
 if(ReviewFlight)I->Montage_Play(ReviewFlight,1);UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT stair_review_start down=%d pos=%s montage=%s"),Down,*A->GetActorLocation().ToString(),*GetNameSafe(ReviewFlight));
}
void ACourtyardContentLife::SurfaceProbe(FName Name)
{
 if(Name==TEXT("Gift")){
  UStaticMesh* S=Gift?Gift->GetStaticMesh():nullptr;if(!S || !S->GetRenderData() || !S->GetRenderData()->LODResources.Num())return;
  const FString Dir=FPaths::ProjectDir()/TEXT("Audit/Content20261007/Surface/Gift");if(IFileManager::Get().DirectoryExists(*Dir))return;IFileManager::Get().MakeDirectory(*Dir,true);
  const auto& L=S->GetRenderData()->LODResources[0];TArray<FVector3f> V;for(uint32 K=0;K<L.VertexBuffers.PositionVertexBuffer.GetNumVertices();++K)V.Add(L.VertexBuffers.PositionVertexBuffer.VertexPosition(K));TArray<uint32> T;for(int32 K=0;K<L.IndexBuffer.GetNumIndices();++K)T.Add(L.IndexBuffer.GetIndex(K));
  FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)V.GetData(),V.Num()*sizeof(FVector3f)),*(Dir/TEXT("vertices.bin")));FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)T.GetData(),T.Num()*sizeof(uint32)),*(Dir/TEXT("triangles.bin")));UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT gift_real_mesh vertices=%d triangles=%d"),V.Num(),T.Num()/3);return;
 }
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!A)return;
 auto* Anim=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Content20261007/Motions/A_")+Name.ToString()));if(!Anim)return;
 const FString Dir=FPaths::ProjectDir()/TEXT("Audit/Content20261007/Surface")/Name.ToString();if(IFileManager::Get().DirectoryExists(*Dir))return;IFileManager::Get().MakeDirectory(*Dir,true);
 auto* Host=GetWorld()->SpawnActor<AActor>();auto* M=NewObject<USkeletalMeshComponent>(Host);Host->SetRootComponent(M);M->SetSkeletalMeshAsset(A->GetMesh()->GetSkeletalMeshAsset());M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCastShadow(false);M->RegisterComponent();M->SetWorldLocation(FVector(0,0,-5000));M->PlayAnimation(Anim,false);
 auto* RD=M->GetSkeletalMeshRenderData();if(!RD || !RD->LODRenderData.Num()){Host->Destroy();return;}
 const auto& L=RD->LODRenderData[0];const auto* Weights=L.GetSkinWeightVertexBuffer();const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();FString Labels=TEXT("index,bone,material\n");
 for(const auto& S:L.RenderSections)for(uint32 J=0;J<S.NumVertices;++J){uint32 V=S.BaseVertexIndex+J;uint16 Best=0;int32 Bone=0;for(uint32 K=0;K<Weights->GetMaxBoneInfluences();++K){auto W=Weights->GetBoneWeight(V,K);if(W>Best){Best=W;auto B=Weights->GetBoneIndex(V,K);if(S.BoneMap.IsValidIndex(B))Bone=S.BoneMap[B];}}Labels+=FString::Printf(TEXT("%u,%s,%d\n"),V,*Ref.GetBoneName(Bone).ToString(),S.MaterialIndex);}
 FFileHelper::SaveStringToFile(Labels,*(Dir/TEXT("labels.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 TArray<uint32> Indices;auto* IB=L.MultiSizeIndexContainer.GetIndexBuffer();for(int32 J=0;J<IB->Num();++J)Indices.Add(IB->Get(J));FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)Indices.GetData(),Indices.Num()*sizeof(uint32)),*(Dir/TEXT("triangles.bin")));
 FString Times=TEXT("frame,time\n");int32 Frames=FMath::CeilToInt(Anim->GetPlayLength()*5);
 for(int32 F=0;F<=Frames;++F){float T=FMath::Min(F*.2f,Anim->GetPlayLength()-.001f);M->SetPosition(T,false);M->TickAnimation(0,false);M->RefreshBoneTransforms();TArray<FFinalSkinVertex> V;M->GetCPUSkinnedVertices(V,0);TArray<FVector3f> P;P.Reserve(V.Num());for(const auto& X:V)P.Add(X.Position);FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)P.GetData(),P.Num()*sizeof(FVector3f)),*(Dir/FString::Printf(TEXT("%03d.bin"),F)));Times+=FString::Printf(TEXT("%d,%.4f\n"),F,T);}
 FFileHelper::SaveStringToFile(Times,*(Dir/TEXT("times.csv")));Host->Destroy();UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT surface=%s samples=%d real_skinning=1"),*Name.ToString(),Frames+1);
}

void ACourtyardContentLife::SampleSeatSurface()
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!A || !SeatMotion)return;
 auto* M=A->GetMesh();TArray<FFinalSkinVertex> Vert;M->GetCPUSkinnedVertices(Vert,0);TArray<FVector3f> World;
 for(const auto& V:Vert)World.Add(FVector3f(M->GetComponentTransform().TransformPosition(FVector(V.Position))));
 const FString Dir=FPaths::ProjectDir()/(ReadingSeat?TEXT("Audit/ContentFix20261007/Continue20261008/ReadingSurfaceR2"):TEXT("Audit/ContentFix20261007/SeatSurface"));IFileManager::Get().MakeDirectory(*Dir,true);
 const float T=M->GetAnimInstance()->Montage_GetPosition(SeatMotion);
 const FString Name=FString::Printf(TEXT("seat_%05d_world.bin"),FMath::RoundToInt(T*100));
 FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)World.GetData(),World.Num()*sizeof(FVector3f)),*(Dir/Name));
 UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX seat_surface real_skinned=1 vertices=%d t=%.3f file=%s"),World.Num(),T,*Name);
 SampleFixedStairFeet(T);
}
void ACourtyardContentLife::SampleFixedStairFeet(float Time)
{
 auto* A=C.IsValid()?Cast<ACharacter>(C->Emma()):nullptr;if(!A)return;auto* M=A->GetMesh();
 auto* RD=M->GetSkeletalMeshRenderData();if(!RD || !RD->LODRenderData.Num())return;
 const auto& L=RD->LODRenderData[0];const auto* Weights=L.GetSkinWeightVertexBuffer();const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();
 if(!FixedFootIndices[0].Num())for(const auto& S:L.RenderSections)for(uint32 J=0;J<S.NumVertices;++J){
  const uint32 V=S.BaseVertexIndex+J;uint16 Best=0;int32 Bone=0;
  for(uint32 K=0;K<Weights->GetMaxBoneInfluences();++K)if(Weights->GetBoneWeight(V,K)>Best){Best=Weights->GetBoneWeight(V,K);const auto B=Weights->GetBoneIndex(V,K);if(S.BoneMap.IsValidIndex(B))Bone=S.BoneMap[B];}
  const FString N=Ref.GetBoneName(Bone).ToString();if(N.Contains(TEXT("足首")) || N.Contains(TEXT("つま先"))){if(N.EndsWith(TEXT("_L")))FixedFootIndices[0].Add(V);else if(N.EndsWith(TEXT("_R")))FixedFootIndices[1].Add(V);}
 }
 TArray<FFinalSkinVertex> Vert;M->GetCPUSkinnedVertices(Vert,0);FCollisionQueryParams Query;Query.AddIgnoredActor(A);
 for(int Side=0;Side<2;++Side){
  TArray<FVector> Points;for(int Idx:FixedFootIndices[Side])if(Vert.IsValidIndex(Idx))Points.Add(M->GetComponentTransform().TransformPosition(FVector(Vert[Idx].Position)));
  if(!Points.Num())continue;Points.Sort([](const FVector& P,const FVector& Q){return P.Z<Q.Z;});
  const float Bottom=Points[0].Z;FVector Sum=FVector::ZeroVector;int Count=0;float MinGap=1000,MaxGap=-1000;int Hits=0;FHitResult LowestHit;FVector LowestPoint=FVector::ZeroVector;
  for(const FVector& P:Points){if(P.Z>Bottom+.6f)break;Sum+=P;++Count;
   FHitResult H;if(GetWorld()->LineTraceSingleByChannel(H,P+FVector(0,0,24),P-FVector(0,0,45),ECC_Visibility,Query)){
    if(P.Z-H.ImpactPoint.Z<MinGap){LowestHit=H;LowestPoint=P;}MinGap=FMath::Min(MinGap,float(P.Z-H.ImpactPoint.Z));MaxGap=FMath::Max(MaxGap,float(P.Z-H.ImpactPoint.Z));++Hits;
   }
  }
  UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_sole t=%.3f side=%d vertices=%d sampled=%d hits=%d center=%s bottom=%.3f min_gap=%.3f max_gap=%.3f"),Time,Side,Points.Num(),Count,Hits,*(Sum/FMath::Max(Count,1)).ToString(),Bottom,MinGap,MaxGap);
  if(MinGap<-1)UE_LOG(LogTemp,Display,TEXT("CONTENT_FIX stair_sole_hit t=%.3f side=%d point=%s hit=%s normal=%s actor=%s component=%s"),Time,Side,*LowestPoint.ToString(),*LowestHit.ImpactPoint.ToString(),*LowestHit.ImpactNormal.ToString(),*GetNameSafe(LowestHit.GetActor()),*GetNameSafe(LowestHit.GetComponent()));
 }
}
