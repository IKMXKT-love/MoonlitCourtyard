#include "CourtyardController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "UObject/UObjectIterator.h"
#include "Misc/CommandLine.h"

// Expression remap (2026-10-05, plan Builds/Review/Expression_Remap_Plan.html). The copies only differ in face curves, so
// the original montages are re-pointed to them in memory: slots, notifies, sections and blend times stay exactly as
// authored. Pat responses are bound through the response pointers, which are swapped as well. Off restores the originals.
void ACourtyardController::ApplyExpressionRemap(int32 Level)
{
    // Level 0 = original faces, 1..3 = batches 1..3 (a clip without a copy at that level uses the newest earlier one).
    TMap<UAnimSequenceBase*,UAnimSequenceBase*> Map;
    for(int32 I=0;I<RemapOld.Num();++I){
        UAnimSequence* O=RemapOld[I];if(!O)continue;
        UAnimSequence* V[3]={RemapNew.IsValidIndex(I)?RemapNew[I].Get():nullptr,RemapNew2.IsValidIndex(I)?RemapNew2[I].Get():nullptr,RemapNew3.IsValidIndex(I)?RemapNew3[I].Get():nullptr};
        UAnimSequence* To=O;for(int32 K=0;K<FMath::Min(Level,3);++K)if(V[K])To=V[K];
        const int32 Body=BodyOld.Find(O);UAnimSequence* BodyCopy=Body!=INDEX_NONE?BodyNew[Body].Get():nullptr;
        if(BodyRemapOn&&BodyCopy)To=BodyCopy;
        for(UAnimSequence* From:{O,V[0],V[1],V[2],BodyCopy})if(From&&From!=To)Map.Add(From,To);
    }
    int32 Segments=0,Responses=0;
    for(TObjectIterator<UAnimMontage> It;It;++It)for(auto& Slot:It->SlotAnimTracks)for(auto& Segment:Slot.AnimTrack.AnimSegments)
        if(auto* const* To=Map.Find(Segment.GetAnimReference().Get())){Segment.SetAnimReference(*To,false);++Segments;}
    auto Swap=[&](TObjectPtr<UAnimSequence>& S){if(auto* const* To=Map.Find(S.Get())){S=Cast<UAnimSequence>(*To);++Responses;}};
    for(auto& P:LivelyResponses)Swap(P.Value);for(auto& P:TunedResponses)Swap(P.Value);
    Swap(AttentiveResponse);Swap(BrightResponse);Swap(TenderResponse);
    ExpressionRemapLevel=Level;ExpressionRemapOn=Level>0;
    UE_LOG(LogTemp,Display,TEXT("COURTYARD_EXPRESSION level=%d body=%d bodyclips=%d clips=%d segments=%d responses=%d"),Level,BodyRemapOn?1:0,BodyOld.Num(),RemapOld.Num(),Segments,Responses);
}
void ACourtyardController::CourtyardExpressionRemap(int32 Level)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    ApplyExpressionRemap(FMath::Clamp(Level,0,3));
}
void ACourtyardController::CourtyardBodyRemap(int32 On)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CourtyardReview")))return;
    BodyRemapOn=On>0;ApplyExpressionRemap(ExpressionRemapLevel);
}
