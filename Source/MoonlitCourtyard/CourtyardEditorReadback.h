#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardEditorReadback.generated.h"
class UBlueprint;
UCLASS()
class MOONLITCOURTYARD_API UCourtyardEditorReadback : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Courtyard|Editor")
    static FString ReadBlueprintGraphs(UBlueprint* Blueprint);
};
