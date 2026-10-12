#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HAL/CriticalSection.h"
#include "CourtyardGazeAnimInstance.generated.h"

// Copied on the game thread; evaluation never queries the world or writes a UObject.
struct FCourtyardGazeFrame
{
 float HeadYaw=0,HeadPitch=0,EyeYaw=0,EyePitch=0,HeadWeight=0,EyeWeight=0;
 double Time=0;
 bool Review=false,Authored=false,Walking=false;
 bool Additive=false;
 float AppliedYaw=0,AppliedPitch=0,TorsoYaw=0,TorsoPitch=0;
 float Distance=0,RawYaw=0,RawPitch=0;
 // 1 while a clip that already turns her toward the player plays (turning greeting family): the additive gaze then
 // adds only what the animation has not turned yet, instead of turning a second time.
 float TurnCompensation=0,TurnKeepYaw=0;  // TurnKeepYaw: authored head yaw the gaze must leave in place (shy glance away)
 bool Visible=false;
 int32 Category=0;
 // S3 posture, anatomical degrees (see ECourtyardPosture), already blended; scaled by IdleWeight in the node.
 float IdleAngles[11]={0,0,0,0,0,0,0,0,0,0,0};
 float IdleWeight=0;
 int32 IdleVariant=0;
 // New-map-only contact correction copied on the game thread; defaults inert.
 FVector TerracePalmOffsetCS=FVector::ZeroVector;
 float TerracePalmWeight=0,TerracePalmYaw=0;
};
// Posture channels, all in the body's own frame. Positive = the named direction.
enum ECourtyardPosture : int32 {ChestForward,ShoulderDropL,ShoulderDropR,ShoulderForwardL,ShoulderForwardR,ArmOutL,ArmOutR,ArmForwardL,ArmForwardR,HeadDown,HeadTiltRight,PostureCount};
struct FCourtyardGazeProbe
{
 FCriticalSection Mutex;
 double Time=-1;
 FQuat Input[4],Output[4];
 float HeadYaw=0,HeadPitch=0,EyeDegrees=0,FootDelta=0,HeadWeight=0,EyeWeight=0;
 bool Authored=false,Walking=false;
 float Distance=0,RawYaw=0,RawPitch=0;
 bool Visible=false;
 int32 Category=0;
 float IdleWeight=0,IdleAngles[11]={0,0,0,0,0,0,0,0,0,0,0};
 int32 IdleVariant=0;
 FQuat IdleInput[5],IdleOutput[5];
};
UCLASS(Transient,Blueprintable)
class MOONLITCOURTYARD_API UCourtyardGazeAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 FCourtyardGazeFrame Frame;
 TSharedPtr<FCourtyardGazeProbe,ESPMode::ThreadSafe> Probe=MakeShared<FCourtyardGazeProbe,ESPMode::ThreadSafe>();
};
