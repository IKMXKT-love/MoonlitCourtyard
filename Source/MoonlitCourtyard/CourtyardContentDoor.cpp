#include "CourtyardContentDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
ACourtyardContentDoor::ACourtyardContentDoor()
{
 PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 Leaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));Leaf->SetupAttachment(RootComponent);Leaf->SetMobility(EComponentMobility::Movable);Leaf->SetCollisionProfileName(TEXT("BlockAll"));
 OccupancyFlag=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OccupancyFlag"));OccupancyFlag->SetupAttachment(RootComponent);OccupancyFlag->SetCollisionEnabled(ECollisionEnabled::NoCollision);OccupancyFlag->SetCastShadow(false);
 PrivateVolume=CreateDefaultSubobject<UBoxComponent>(TEXT("PrivateVolume"));PrivateVolume->SetupAttachment(RootComponent);PrivateVolume->SetCollisionProfileName(TEXT("BlockAll"));PrivateVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);PrivateVolume->SetHiddenInGame(true);
}
void ACourtyardContentDoor::BeginPlay()
{
 Super::BeginPlay();Leaf->SetRelativeRotation(FRotator(0,ClosedYaw,0));
 if(SlidingDoor)Leaf->SetRelativeLocation(SlideClosedOffset);
 PrivateVolume->SetRelativeLocation(PrivateCenter);PrivateVolume->SetBoxExtent(PrivateExtent);
 FlagMaterial=OccupancyFlag->CreateDynamicMaterialInstance(0);if(FlagMaterial)FlagMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.14,.25,.19));
}
bool ACourtyardContentDoor::SweepClear(float From,float To,AActor* Ignore) const
{
 if(SlidingDoor)
 {
  FVector Min,Max;Leaf->GetLocalBounds(Min,Max);const FVector Scale=Leaf->GetRelativeScale3D();Min*=Scale;Max*=Scale;
  const FRotator Rotation(0,ClosedYaw,0);
  for(TActorIterator<ACharacter> I(GetWorld());I;++I)
  {
   if(I->IsHidden())continue;
   const FVector P=GetActorTransform().InverseTransformPosition(I->GetActorLocation());
   const float Radius=I->GetCapsuleComponent()->GetScaledCapsuleRadius()+2;
   const float Half=I->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   const FVector Initial=Rotation.UnrotateVector(P-SlideClosedOffset-SlideOffset*(From/88.f));
   const float InitialGap=FVector2D(Initial.X-FMath::Clamp(Initial.X,Min.X,Max.X),Initial.Y-FMath::Clamp(Initial.Y,Min.Y,Max.Y)).Size();
   // A handle enlarges the bounds beyond the flat panel. Permit motion along/away
   // from existing contact, while rejecting any increase in overlap with a player.
   const float RequiredGap=FMath::Min(Radius,FMath::Max(0.f,InitialGap-.05f));
   for(int K=0;K<=12;++K)
   {
    const FVector Offset=SlideClosedOffset+SlideOffset*(FMath::Lerp(From,To,K/12.f)/88.f);
    const FVector Q=Rotation.UnrotateVector(P-Offset);
    if(Q.Z+Half<Min.Z || Q.Z-Half>Max.Z)continue;
    const FVector2D Gap(Q.X-FMath::Clamp(Q.X,Min.X,Max.X),Q.Y-FMath::Clamp(Q.Y,Min.Y,Max.Y));
    if(Gap.SizeSquared()<RequiredGap*RequiredGap)return false;
   }
  }
  return true;
 }
 // Door never pushes or sweeps through a character. Tested leaf spans hinge->100 cm in -Y when closed.
 for(TActorIterator<ACharacter> I(GetWorld());I;++I){if((!FixEnabled && *I==Ignore) || I->IsHidden())continue;const FVector P=FixEnabled?GetActorTransform().InverseTransformPosition(I->GetActorLocation()):I->GetActorLocation()-GetActorLocation();if(FMath::Abs(P.Z-118)>I->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+120)continue;
  for(int K=0;K<=4;++K){const float A=FMath::DegreesToRadians(FMath::Lerp(From,To,K/4.f));const FVector V=FixEnabled?FRotator(0,ClosedYaw+(PrivacyRoom?-1:1)*FMath::RadiansToDegrees(A),0).Vector():FVector(-FMath::Sin(A),-FMath::Cos(A),0);const float Along=FMath::Clamp(FVector::DotProduct(P,V),0.,100.);const FVector Q=P-V*Along;
   if(FVector2D(Q).Size()<I->GetCapsuleComponent()->GetScaledCapsuleRadius()+9)return false;}}
 return true;
}
bool ACourtyardContentDoor::RequestToggle(AActor* User)
{
 if(Occupied){++HandleAttempts;UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT door_locked bath=%d attempts=%d"),Bathroom,HandleAttempts);return false;}
 if(!SweepClear(Angle,Open?0:88,User)){UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT door_blocked bath=%d"),Bathroom);return false;}
 Open=!Open;BlockedSeconds=0;return true;
}
bool ACourtyardContentDoor::SetOccupied(bool Value,AActor* Occupant)
{
 if(Value && !PrivacyRoom)return false;
 if(Value){if(Open || Angle>.1f || !Occupant)return false;
  auto* P=UGameplayStatics::GetPlayerPawn(this,0);if(P && PrivateVolume->Bounds.GetBox().IsInsideOrOn(P->GetActorLocation()))return false;
  // Privacy is both physical and render-level. No intimate activity asset or state exists.
  PrivateOccupant=Occupant;Occupant->SetActorHiddenInGame(true);PrivateVolume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);KnockCount=HandleAttempts=0;WaitedOutside=0;
 }else{PrivateVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);if(PrivateOccupant.IsValid())PrivateOccupant->SetActorHiddenInGame(false);PrivateOccupant.Reset();}
 Occupied=Value;if(FlagMaterial)FlagMaterial->SetVectorParameterValue(TEXT("Tint"),Value?FLinearColor(.48,.16,.07):FLinearColor(.14,.25,.19));
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT occupancy bath=%d locked=%d"),Bathroom,Occupied);return true;
}
FVector ACourtyardContentDoor::HandlePoint() const
{
 if(SlidingDoor)return GetActorTransform().TransformPosition(SlideClosedOffset+SlideOffset*(Angle/88.f)+FRotator(0,ClosedYaw,0).RotateVector(FVector(82,8,112)));
 if(!FixEnabled)return GetActorLocation()+FVector(0,-75,110);
 return GetActorTransform().TransformPosition(FRotator(0,ClosedYaw+(PrivacyRoom?-Angle:Angle),0).RotateVector(FVector(82,8,112)));
}
FVector ACourtyardContentDoor::InteractionPoint() const
{
 // Keep the operable target in the opening, on both sides, even while the leaf is parked.
 return SlidingDoor?GetActorTransform().TransformPosition(FRotator(0,ClosedYaw,0).RotateVector(FVector(50,0,145))):HandlePoint();
}
void ACourtyardContentDoor::Knock(){if(!Occupied)return;++KnockCount;LastKnock=GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("COURTYARD_CONTENT knock bath=%d count=%d"),Bathroom,KnockCount);}
void ACourtyardContentDoor::Tick(float Dt)
{
 Super::Tick(Dt);const float Target=Open?88.f:0.f;
 if(InMotion()) {const float Next=FMath::FInterpConstantTo(Angle,Target,Dt,45);
  if(SweepClear(Angle,Next)){Angle=Next;if(SlidingDoor)Leaf->SetRelativeLocation(SlideClosedOffset+SlideOffset*(Angle/88.f));else Leaf->SetRelativeRotation(FRotator(0,ClosedYaw+(FixEnabled && !PrivacyRoom?Angle:-Angle),0));BlockedSeconds=0;}
  else {BlockedSeconds+=Dt;if(BlockedSeconds>1.5){Open=Angle>44;BlockedSeconds=0;}}}
 if(Occupied){auto* P=UGameplayStatics::GetPlayerPawn(this,0);const FVector Waiting=FixEnabled?GetActorLocation()+FRotator(0,ClosedYaw,0).RotateVector(FVector(60,65,80)):GetActorLocation()+FVector(60,-45,80);if(P && FVector::Dist(P->GetActorLocation(),Waiting)<170)WaitedOutside+=Dt;}
}
