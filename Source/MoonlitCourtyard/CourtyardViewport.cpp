#include "CourtyardViewport.h"
#include "CourtyardController.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Engine/World.h"
bool UCourtyardViewport::InputKey(const FInputKeyEventArgs& Args)
{
    if(GetWorld() && !GetWorld()->IsPlayInEditor() && Args.Key==EKeys::Enter && Args.Event==IE_Pressed && FSlateApplication::Get().GetModifierKeys().IsAltDown())
    {
        if(auto* C=Cast<ACourtyardController>(GetWorld()->GetFirstPlayerController()))
        {
            if(C->Page!=ECourtyardPage::DisplayConfirm && C->CaptureAction==INDEX_NONE)
            {C->Draft=C->Applied;C->Draft.WindowMode=C->Applied.WindowMode==2?1:2;C->ApplyDraft();}
            return true;
        }
    }
    return Super::InputKey(Args);
}
