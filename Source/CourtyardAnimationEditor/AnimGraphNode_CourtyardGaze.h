#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_CourtyardGaze.h"
#include "AnimGraphNode_CourtyardGaze.generated.h"
UCLASS()
class COURTYARDANIMATIONEDITOR_API UAnimGraphNode_CourtyardGaze:public UAnimGraphNode_SkeletalControlBase
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category=Settings) FAnimNode_CourtyardGaze Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type)const override{return FText::FromString(TEXT("Courtyard bounded gaze"));}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode()const override{return &Node;}
 virtual FText GetControllerDescription()const override{return FText::FromString(TEXT("Courtyard gaze"));}
};
