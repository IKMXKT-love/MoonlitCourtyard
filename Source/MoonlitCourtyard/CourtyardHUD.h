#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CourtyardHUD.generated.h"
class ACourtyardController;
struct FCourtyardButton
{
    FVector2D Min, Max;
    TFunction<void(float)> Action;
    bool Slider = false;
};
UCLASS()
class MOONLITCOURTYARD_API ACourtyardHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void Click(float X, float Y);
    void Drag(float X);
    void Release() { DragAction=nullptr; }
private:
    TFunction<void(float)> DragAction;
    float DragMin=0, DragWidth=1;
    TArray<FCourtyardButton> Buttons;
    ACourtyardController* C = nullptr;
    float Scale = 1, OffsetX = 0, OffsetY = 0;
    void Text(const FString& S, float X, float Y, float Size=20, FLinearColor Color=FLinearColor(.84f,.88f,1.f));
    void Rect(float X,float Y,float W,float H,FLinearColor Color);
    void Button(const FString& Label,float X,float Y,float W,TFunction<void(float)> Action,bool Enabled=true);
    void Setting(const FString& Label,const FString& Value,int32 Row,float Y,bool Slider=false,float Fraction=0,bool Enabled=true);
    void DrawSettings();
    void DrawMap();
    void Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Width=1);
    void Disk(float X,float Y,float Radius,FLinearColor Color);
    void Ring(float X,float Y,float Radius,FLinearColor Color,float Width=1);
    void Panel(float X,float Y,float W,float H,FLinearColor Color);
    bool Hover(float X,float Y,float W,float H) const;
    void MenuCard(const FString& Title,const FString& Subtitle,float Y,TFunction<void(float)> Action);
};
