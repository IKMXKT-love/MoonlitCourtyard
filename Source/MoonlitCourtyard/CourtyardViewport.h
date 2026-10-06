#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "CourtyardViewport.generated.h"
UCLASS()
class MOONLITCOURTYARD_API UCourtyardViewport : public UGameViewportClient
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& Args) override;
};
