#include "CourtyardEditorReadback.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#endif
FString UCourtyardEditorReadback::ReadBlueprintGraphs(UBlueprint* Blueprint)
{
#if WITH_EDITOR
    if(!Blueprint)return FString();
    TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);FString Result;
    for(UEdGraph* Graph:Graphs)
    {
        TSet<UObject*> Nodes;for(UEdGraphNode* Node:Graph->Nodes)if(Node)Nodes.Add(Node);
        FString Text;FEdGraphUtilities::ExportNodesToText(Nodes,Text);
        Result+=TEXT("\nGRAPH ")+Graph->GetName()+TEXT("\n")+Text;
    }
    return Result;
#else
    return FString();
#endif
}
