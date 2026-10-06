#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardAnimationAuthor.generated.h"
UCLASS()
class COURTYARDANIMATIONEDITOR_API UCourtyardAnimationAuthor:public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="CourtyardAuthor") static FString BuildGazeGraph(class UAnimBlueprint* Blueprint);
};
