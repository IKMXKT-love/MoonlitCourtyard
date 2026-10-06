#include "CourtyardAnimationAuthor.h"
#include "AnimGraphNode_CourtyardGaze.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
template<class T>static T* Add(UEdGraph& G,int32 X){FGraphNodeCreator<T>C(G);T* N=C.CreateNode(false);N->NodePosX=X;C.Finalize();return N;}
static UEdGraphPin* Pin(UEdGraphNode* N,EEdGraphPinDirection D){for(auto* P:N->Pins)if(P&&P->Direction==D&&UAnimationGraphSchema::IsPosePin(P->PinType))return P;return nullptr;}
FString UCourtyardAnimationAuthor::BuildGazeGraph(UAnimBlueprint* BP)
{
 if(!BP||!BP->GetPathName().StartsWith(TEXT("/Game/Emma/Animations/LivelyGaze20261004/")))return TEXT("Refused: new gaze directory only");
 UEdGraph* G=nullptr;for(const auto& Graph:BP->FunctionGraphs)if(Graph->GetFName()==TEXT("AnimGraph"))G=Graph.Get();
 if(!G||G->Nodes.Num()!=1)return TEXT("Expected untouched AnimGraph");
 auto* Root=Cast<UAnimGraphNode_Root>(G->Nodes[0]);auto* S=Cast<UAnimationGraphSchema>(G->GetSchema());if(!Root||!S)return TEXT("Missing root/schema");
 auto* In=Add<UAnimGraphNode_LinkedInputPose>(*G,-800);In->Node.Name=FAnimNode_LinkedInputPose::DefaultInputPoseName;
 auto* LC=Add<UAnimGraphNode_LocalToComponentSpace>(*G,-600);auto* N=Add<UAnimGraphNode_CourtyardGaze>(*G,-400);auto* CL=Add<UAnimGraphNode_ComponentToLocalSpace>(*G,-200);
 UEdGraphNode* Chain[]={In,LC,N,CL,Root};for(int32 I=0;I<4;++I)if(!Pin(Chain[I],EGPD_Output)||!Pin(Chain[I+1],EGPD_Input)||!S->TryCreateConnection(Pin(Chain[I],EGPD_Output),Pin(Chain[I+1],EGPD_Input)))return TEXT("Connection failed");
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);
 if(Results.NumErrors||!BP->GeneratedClass)return TEXT("Compilation failed");
 auto* A=IAnimClassInterface::GetFromClass(BP->GeneratedClass);if(A)for(const auto& F:A->GetAnimBlueprintFunctions())if(F.Name==G->GetFName()){
  int32 I=F.InputPoseNames.IndexOfByKey(FAnimNode_LinkedInputPose::DefaultInputPoseName);if(F.InputPoseNodeProperties.IsValidIndex(I)&&F.InputPoseNodeProperties[I])return TEXT("OK: 5 nodes, compiled InPose");}
 return TEXT("Missing compiled input pose");
}
