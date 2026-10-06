#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "CourtyardGazeAnimInstance.h"
#include "AnimNode_CourtyardGaze.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOONLITCOURTYARD_API FAnimNode_CourtyardGaze : public FAnimNode_SkeletalControlBase
{
 GENERATED_BODY()
 FAnimNode_CourtyardGaze();
 virtual bool HasPreUpdate() const override {return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
protected:
 virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
 virtual bool IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones) override;
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
private:
 FBoneReference Neck,Head,LeftEye,RightEye,LeftFoot,RightFoot;
 FBoneReference TorsoLower,TorsoUpper;
 FBoneReference IdleBones[5];
 FBoneReference ElbowL,ElbowR,HeadTop;
 FVector RefRight,RefForward,RefUp,HeadLocalForward;
 FCourtyardGazeFrame Cached;
 TSharedPtr<FCourtyardGazeProbe,ESPMode::ThreadSafe> Probe;
};
