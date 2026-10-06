#include "AnimNode_CourtyardGaze.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "Misc/ScopeLock.h"
FAnimNode_CourtyardGaze::FAnimNode_CourtyardGaze()
{
 Alpha=1;Neck.BoneName=TEXT("首");Head.BoneName=TEXT("頭");LeftEye.BoneName=TEXT("目_L");RightEye.BoneName=TEXT("目_R");LeftFoot.BoneName=TEXT("足首_L");RightFoot.BoneName=TEXT("足首_R");
 TorsoLower.BoneName=TEXT("上半身");TorsoUpper.BoneName=TEXT("上半身2");
 const FName Names[]={TEXT("上半身2"),TEXT("肩_L"),TEXT("肩_R"),TEXT("腕_L"),TEXT("腕_R")};for(int I=0;I<5;++I)IdleBones[I].BoneName=Names[I];
 ElbowL.BoneName=TEXT("ひじ_L");ElbowR.BoneName=TEXT("ひじ_R");HeadTop.BoneName=TEXT("頭先");
}
void FAnimNode_CourtyardGaze::PreUpdate(const UAnimInstance* I)
{
 Cached={};Probe.Reset();if(auto* A=Cast<UCourtyardGazeAnimInstance>(I)){Cached=A->Frame;Probe=A->Probe;}
}
void FAnimNode_CourtyardGaze::InitializeBoneReferences(const FBoneContainer& B)
{
 for(auto* R:{&Neck,&Head,&LeftEye,&RightEye,&LeftFoot,&RightFoot,&TorsoLower,&TorsoUpper,&ElbowL,&ElbowR,&HeadTop})R->Initialize(B);
 for(auto& R:IdleBones)R.Initialize(B);
 const FReferenceSkeleton& S=B.GetReferenceSkeleton();
 auto T=[&](FName N){return FAnimationRuntime::GetComponentSpaceTransformRefPose(S,S.FindBoneIndex(N));};
 if(S.FindBoneIndex(TEXT("頭先"))==INDEX_NONE)return;
 const FTransform H=T(Head.BoneName);const FVector L=T(LeftEye.BoneName).GetLocation(),R=T(RightEye.BoneName).GetLocation();
 RefRight=(R-L).GetSafeNormal();RefUp=(T(TEXT("頭先")).GetLocation()-H.GetLocation()).GetSafeNormal();
 RefUp=(RefUp-RefRight*FVector::DotProduct(RefUp,RefRight)).GetSafeNormal();RefForward=FVector::CrossProduct(RefRight,RefUp).GetSafeNormal();
 if(FVector::DotProduct(RefForward,(L+R)*.5-H.GetLocation())<0)RefForward=-RefForward;
 HeadLocalForward=H.InverseTransformVectorNoScale(RefForward);
}
bool FAnimNode_CourtyardGaze::IsValidToEvaluate(const USkeleton*,const FBoneContainer& B)
{
 return Neck.IsValidToEvaluate(B)&&Head.IsValidToEvaluate(B)&&LeftEye.IsValidToEvaluate(B)&&RightEye.IsValidToEvaluate(B)&&!RefForward.IsNearlyZero();
}
void FAnimNode_CourtyardGaze::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& O,TArray<FBoneTransform>& Out)
{
 const auto& B=O.Pose.GetPose().GetBoneContainer();FCSPose<FCompactPose> W;W.InitPose(&B);W.CopyPose(O.Pose);
 FCompactPoseBoneIndex Indices[]={Neck.GetCompactPoseIndex(B),Head.GetCompactPoseIndex(B),LeftEye.GetCompactPoseIndex(B),RightEye.GetCompactPoseIndex(B)};
 auto Rotate=[&](FCompactPoseBoneIndex I,FQuat Q){FTransform T=W.GetComponentSpaceTransform(I);T.SetRotation((Q*T.GetRotation()).GetNormalized());FBoneTransform X(I,T);W.SafeSetCSBoneTransforms(MakeArrayView(&X,1));};
 auto Dir=[&](float Y,float P){Y=FMath::DegreesToRadians(Y);P=FMath::DegreesToRadians(P);return RefForward*FMath::Cos(Y)*FMath::Cos(P)+RefRight*FMath::Sin(Y)*FMath::Cos(P)+RefUp*FMath::Sin(P);};
 auto Angles=[&](FVector V){return FVector2D(FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(V,RefRight),FVector::DotProduct(V,RefForward))),FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(V,RefUp),-1.,1.))));};
 // S3 posture: upper-body channels in the body's own frame (no leg or translation channels). Each rotation's sign
 // is fixed by its effect on the child joint (e.g. "chest forward" moves the neck toward RefForward), so no local
 // bone-axis convention is assumed.
 auto Bend=[&](const FBoneReference& Bone,const FBoneReference& Child,const FVector& Axis,const FVector& Toward,float Degrees){
  if(FMath::Abs(Degrees)<.001f || !Bone.IsValidToEvaluate(B) || !Child.IsValidToEvaluate(B))return;
  const auto BI=Bone.GetCompactPoseIndex(B);
  const FVector C=W.GetComponentSpaceTransform(Child.GetCompactPoseIndex(B)).GetLocation()-W.GetComponentSpaceTransform(BI).GetLocation();
  FQuat Q(Axis,FMath::DegreesToRadians(FMath::Abs(Degrees)));
  if((FVector::DotProduct(Q.RotateVector(C)-C,Toward)>=0)!=(Degrees>0))Q=Q.Inverse();
  Rotate(BI,Q);
 };
 if(Cached.IdleWeight>0){
  const float* P=Cached.IdleAngles;const float K=Cached.IdleWeight;
  Bend(IdleBones[0],Neck,RefRight,RefForward,P[ChestForward]*K);
  Bend(IdleBones[1],IdleBones[3],RefForward,-RefUp,P[ShoulderDropL]*K);
  Bend(IdleBones[2],IdleBones[4],RefForward,-RefUp,P[ShoulderDropR]*K);
  Bend(IdleBones[1],IdleBones[3],RefUp,RefForward,P[ShoulderForwardL]*K);
  Bend(IdleBones[2],IdleBones[4],RefUp,RefForward,P[ShoulderForwardR]*K);
  Bend(IdleBones[3],ElbowL,RefForward,-RefRight,P[ArmOutL]*K);
  Bend(IdleBones[4],ElbowR,RefForward,RefRight,P[ArmOutR]*K);
  Bend(IdleBones[3],ElbowL,RefRight,RefForward,P[ArmForwardL]*K);
  Bend(IdleBones[4],ElbowR,RefRight,RefForward,P[ArmForwardR]*K);
  Bend(Head,HeadTop,RefRight,RefForward,P[HeadDown]*K);
  Bend(Head,HeadTop,RefForward,RefRight,P[HeadTiltRight]*K);
 }
 const FQuat OriginalHead=W.GetComponentSpaceTransform(Indices[1]).GetRotation();
 if(Cached.Additive){
  // World-anatomical correction composed on top of authored animation; never replace its head aim.
  float TorsoYaw=Cached.TorsoYaw,AppliedYaw=Cached.AppliedYaw;
  if(Cached.TurnCompensation>0){
   // The turning greetings rotate chest+neck+head toward the player (and drift ~10 deg near their end); subtract the
   // input head's own yaw so the final head stays on the player. TurnKeepYaw leaves a deliberate glance away in place.
   const float Authored=Angles(O.Pose.GetComponentSpaceTransform(Indices[1]).GetRotation().RotateVector(HeadLocalForward)).X-Cached.TurnKeepYaw;
   const float R=Cached.AppliedYaw+Cached.TorsoYaw-Authored*Cached.HeadWeight*Cached.TurnCompensation;
   TorsoYaw=FMath::Sign(R)*FMath::Clamp((FMath::Abs(R)-30.f)*.8f,0.f,20.f);AppliedYaw=R-TorsoYaw;
  }
  const FQuat Torso=FQuat::FindBetweenNormals(RefForward,Dir(TorsoYaw,Cached.TorsoPitch));
  if(TorsoLower.IsValidToEvaluate(B))Rotate(TorsoLower.GetCompactPoseIndex(B),FQuat::Slerp(FQuat::Identity,Torso,.4));
  if(TorsoUpper.IsValidToEvaluate(B))Rotate(TorsoUpper.GetCompactPoseIndex(B),FQuat::Slerp(FQuat::Identity,Torso,.6));
  const FQuat Aim=FQuat::FindBetweenNormals(RefForward,Dir(AppliedYaw,Cached.AppliedPitch));
  Rotate(Indices[0],FQuat::Slerp(FQuat::Identity,Aim,.35));Rotate(Indices[1],FQuat::Slerp(FQuat::Identity,Aim,.65));
 }else if(Cached.HeadWeight>0){
  const FVector F=OriginalHead.RotateVector(HeadLocalForward);const FVector2D A=Angles(F);
  // Desired final aim is bounded; idle roll and all untranslated channels remain intact.
  const FVector Goal=Dir(FMath::Lerp(float(A.X),FMath::Clamp(Cached.HeadYaw,-35.f,35.f),Cached.HeadWeight),FMath::Lerp(float(A.Y),FMath::Clamp(Cached.HeadPitch,-15.f,15.f),Cached.HeadWeight));
  const FQuat Q=FQuat::FindBetweenNormals(F,Goal);Rotate(Indices[0],FQuat::Slerp(FQuat::Identity,Q,.35));
  const FVector Now=W.GetComponentSpaceTransform(Indices[1]).GetRotation().RotateVector(HeadLocalForward);Rotate(Indices[1],FQuat::FindBetweenNormals(Now,Goal));
 }
 const FVector HeadFront=W.GetComponentSpaceTransform(Indices[1]).GetRotation().RotateVector(HeadLocalForward);const FVector2D HA=Angles(HeadFront);
 float EyeDegrees=0;
 if(Cached.EyeWeight>0){
  FVector2D E(FMath::FindDeltaAngleDegrees(float(HA.X),Cached.EyeYaw),Cached.EyePitch-float(HA.Y));
  if(E.Size()>12)E*=12/E.Size();EyeDegrees=E.Size()*Cached.EyeWeight;
  const FQuat Q=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(HeadFront,Dir(HA.X+E.X,HA.Y+E.Y)),Cached.EyeWeight);
  Rotate(Indices[2],Q);Rotate(Indices[3],Q);
 }
 TArray<FCompactPoseBoneIndex> Changed;
 if(Cached.Additive || Cached.HeadWeight>0 || Cached.EyeWeight>0)for(auto I:Indices)Changed.AddUnique(I);
 if(Cached.Additive)for(auto* R:{&TorsoLower,&TorsoUpper})if(R->IsValidToEvaluate(B))Changed.AddUnique(R->GetCompactPoseIndex(B));
 if(Cached.IdleWeight>0){for(auto& R:IdleBones)if(R.IsValidToEvaluate(B))Changed.AddUnique(R.GetCompactPoseIndex(B));Changed.AddUnique(Indices[1]);}
 for(auto I:Changed)Out.Add(FBoneTransform(I,W.GetComponentSpaceTransform(I)));Out.Sort(FCompareBoneTransformIndex());
 if(Cached.Review&&Probe){FScopeLock Lock(&Probe->Mutex);Probe->Time=Cached.Time;Probe->HeadYaw=HA.X;Probe->HeadPitch=HA.Y;Probe->EyeDegrees=EyeDegrees;Probe->FootDelta=0;Probe->HeadWeight=Cached.HeadWeight;Probe->EyeWeight=Cached.EyeWeight;Probe->Authored=Cached.Authored;Probe->Walking=Cached.Walking;
  Probe->Distance=Cached.Distance;Probe->RawYaw=Cached.RawYaw;Probe->RawPitch=Cached.RawPitch;Probe->Visible=Cached.Visible;Probe->Category=Cached.Category;
  for(int32 I=0;I<4;++I){Probe->Input[I]=O.Pose.GetComponentSpaceTransform(Indices[I]).GetRotation();Probe->Output[I]=W.GetComponentSpaceTransform(Indices[I]).GetRotation();}
  Probe->IdleWeight=Cached.IdleWeight;Probe->IdleVariant=Cached.IdleVariant;
  for(int J=0;J<PostureCount;++J)Probe->IdleAngles[J]=Cached.IdleAngles[J];
  for(int J=0;J<5;++J)if(IdleBones[J].IsValidToEvaluate(B)){auto I=IdleBones[J].GetCompactPoseIndex(B);Probe->IdleInput[J]=O.Pose.GetComponentSpaceTransform(I).GetRotation();Probe->IdleOutput[J]=W.GetComponentSpaceTransform(I).GetRotation();}
  for(auto* R:{&LeftFoot,&RightFoot})if(R->IsValidToEvaluate(B)){auto I=R->GetCompactPoseIndex(B);Probe->FootDelta=FMath::Max(Probe->FootDelta,float(FVector::Dist(O.Pose.GetComponentSpaceTransform(I).GetLocation(),W.GetComponentSpaceTransform(I).GetLocation())));}
 }
}
