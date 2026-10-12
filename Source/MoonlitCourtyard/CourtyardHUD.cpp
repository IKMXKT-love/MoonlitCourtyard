#include "CourtyardHUD.h"
#include "CourtyardController.h"
#include "CourtyardContentLife.h"
#include "CourtyardContentDoor.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformTime.h"
#include "CanvasItem.h"
#include "Styling/CoreStyle.h"
#include "HAL/IConsoleManager.h"

void ACourtyardHUD::Text(const FString& S,float X,float Y,float Size,FLinearColor Color)
{ DrawText(S,Color,OffsetX+X*Scale,OffsetY+Y*Scale,GEngine->GetMediumFont(),Size/20.f*Scale); }
void ACourtyardHUD::Rect(float X,float Y,float W,float H,FLinearColor Color)
{ DrawRect(Color,OffsetX+X*Scale,OffsetY+Y*Scale,W*Scale,H*Scale); }
namespace
{
    const FLinearColor Ink(.035f,.045f,.080f,.96f), Gold(.64f,.55f,.36f), Ivory(.90f,.88f,.81f), Muted(.49f,.55f,.63f);
}
void ACourtyardHUD::Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Width)
{DrawLine(OffsetX+X*Scale,OffsetY+Y*Scale,OffsetX+X2*Scale,OffsetY+Y2*Scale,Color,Width*Scale);}
void ACourtyardHUD::Disk(float X,float Y,float Radius,FLinearColor Color)
{Canvas->K2_DrawPolygon(nullptr,FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(Radius*Scale,Radius*Scale),32,Color);}
void ACourtyardHUD::Ring(float X,float Y,float Radius,FLinearColor Color,float Width)
{for(int I=0;I<64;++I){float A=I*2*PI/64,B=(I+1)*2*PI/64;Line(X+FMath::Cos(A)*Radius,Y+FMath::Sin(A)*Radius,X+FMath::Cos(B)*Radius,Y+FMath::Sin(B)*Radius,Color,Width);}}
void ACourtyardHUD::Panel(float X,float Y,float W,float H,FLinearColor Color)
{
    constexpr float R=9;Rect(X+R,Y,W-2*R,H,Color);Rect(X,Y+R,R,H-2*R,Color);Rect(X+W-R,Y+R,R,H-2*R,Color);
    for(float A:{X+R,X+W-R})for(float B:{Y+R,Y+H-R})Disk(A,B,R,Color);
}
bool ACourtyardHUD::Hover(float X,float Y,float W,float H) const
{float MX,MY;return C&&C->GetMousePosition(MX,MY)&&(MX-OffsetX)/Scale>=X&&(MX-OffsetX)/Scale<=X+W&&(MY-OffsetY)/Scale>=Y&&(MY-OffsetY)/Scale<=Y+H;}
void ACourtyardHUD::Button(const FString& Label,float X,float Y,float W,TFunction<void(float)> Action,bool Enabled)
{
    const bool Over=Enabled&&Hover(X,Y,W,36);
    Panel(X,Y,W,36,Over?FLinearColor(.15f,.17f,.24f,.99f):FLinearColor(.075f,.091f,.135f,.94f));
    Line(X+10,Y+35,X+W-10,Y+35,Over?Gold:FLinearColor(.17f,.20f,.26f));
    Text(Label,X+14,Y+7,18,Enabled?(Over?Ivory:FLinearColor(.74f,.79f,.84f)):Muted);
    if(Enabled)Buttons.Add({FVector2D(X,Y),FVector2D(X+W,Y+36),MoveTemp(Action)});
}
void ACourtyardHUD::MenuCard(const FString& Title,const FString& Subtitle,float Y,TFunction<void(float)> Action)
{
    constexpr float X=738,W=365,H=78;const bool Over=Hover(X,Y,W,H);
    Panel(X,Y,W,H,Over?FLinearColor(.13f,.16f,.23f,.98f):FLinearColor(.064f,.08f,.13f,.96f));
    Line(X+2,Y+18,X+2,Y+60,Over?Gold:FLinearColor(.30f,.31f,.34f),2);
    Text(Title,X+25,Y+13,23,Over?Ivory:FLinearColor(.78f,.82f,.88f));Text(Subtitle,X+25,Y+46,14,Muted);
    Line(X+W-30,Y+33,X+W-23,Y+39,Gold);Line(X+W-23,Y+39,X+W-30,Y+45,Gold);
    Buttons.Add({FVector2D(X,Y),FVector2D(X+W,Y+H),MoveTemp(Action)});
}
void ACourtyardHUD::Click(float X,float Y)
{
    DragAction=nullptr;
    const FVector2D P((X-OffsetX)/Scale,(Y-OffsetY)/Scale);
    for(const auto& B:Buttons)if(P.X>=B.Min.X&&P.Y>=B.Min.Y&&P.X<=B.Max.X&&P.Y<=B.Max.Y)
    {auto Action=B.Action;if(B.Slider){DragAction=B.Action;DragMin=OffsetX+B.Min.X*Scale;DragWidth=(B.Max.X-B.Min.X)*Scale;}Action(FMath::Clamp((P.X-B.Min.X)/(B.Max.X-B.Min.X),0.f,1.f));return;}
}
void ACourtyardHUD::Drag(float X){if(DragAction)DragAction(FMath::Clamp((X-DragMin)/DragWidth,0.f,1.f));}
void ACourtyardHUD::Setting(const FString& Label,const FString& Value,int32 Row,float Y,bool Slider,float Fraction,bool Enabled)
{
    Line(300,Y+43,1005,Y+43,FLinearColor(.11f,.14f,.19f));Text(Label,300,Y+7,19,Ivory);Text(Value,610,Y+7,18,Enabled?FLinearColor(.65f,.72f,.84f):Muted);
    if(Slider)
    {
        Rect(795,Y+17,210,3,FLinearColor(.16f,.20f,.26f));Rect(795,Y+17,210*Fraction,3,Gold);Disk(795+210*Fraction,Y+18,6,Ivory);
        Buttons.Add({FVector2D(795,Y),FVector2D(1005,Y+36),[this,Row](float T){C->ActivateSetting(Row,T,true);},true});
    }
    else
    {
        Button(TEXT("−"),870,Y,54,[this,Row](float){C->ActivateSetting(Row,-1,false);},Enabled);
        Button(TEXT("+"),950,Y,54,[this,Row](float){C->ActivateSetting(Row,1,false);},Enabled);
    }
}
void ACourtyardHUD::DrawSettings()
{
    const TCHAR* Tabs[]={TEXT("声音"),TEXT("操作"),TEXT("显示"),TEXT("画质"),TEXT("按键")};
    for(int32 I=0;I<5;++I){Button(Tabs[I],280+I*146,180,132,[this,I](float){C->SettingsTab=I;C->CaptureAction=INDEX_NONE;});if(C->SettingsTab==I)Line(294+I*146,219,398+I*146,219,Gold,2);}
    Text(TEXT("偏好设置"),122,192,20,Ivory);Text(TEXT("让夜晚更合心意"),122,229,14,Muted);Line(122,272,231,272,Gold);Text(TEXT("修改后选择应用"),122,301,14,Muted);Text(TEXT("Esc 返回上一级"),122,326,14,Muted);
    const auto& D=C->Draft;
    if(C->SettingsTab==0)
    {
        Setting(TEXT("主音量"),FString::Printf(TEXT("%d%%"),FMath::RoundToInt(D.Master*100)),0,255,true,D.Master);
        Setting(TEXT("背景音乐"),FString::Printf(TEXT("%d%%"),FMath::RoundToInt(D.Music*100)),1,315,true,D.Music);
        Setting(TEXT("环境声音"),FString::Printf(TEXT("%d%%"),FMath::RoundToInt(D.Environment*100)),2,375,true,D.Environment);
        Setting(TEXT("随机间歇音乐"),D.MusicEnabled?TEXT("开启"):TEXT("关闭"),3,435);
        Text(TEXT("应用时改变音量；曲目与曲间计时不会重置。"),300,510,17);
    }
    if(C->SettingsTab==1)
    {
        Setting(TEXT("鼠标灵敏度"),FString::Printf(TEXT("%.2f"),D.Sensitivity),4,260,true,(D.Sensitivity-.2f)/2.8f);
        Setting(TEXT("视野 FOV"),FString::Printf(TEXT("%.0f°"),D.FOV),5,330,true,(D.FOV-60)/40);
        Text(TEXT("姿态与座椅切换保留设定视野，不额外改变 FOV。"),300,435,17);
    }
    if(C->SettingsTab==2)
    {
        const TCHAR* Modes[]={TEXT("独占全屏"),TEXT("无边框全屏"),TEXT("窗口")};
        Setting(TEXT("显示模式"),Modes[D.WindowMode],6,250);
        const FIntPoint R=D.WindowMode==1||D.Resolution==FIntPoint::ZeroValue?GEngine->GetGameUserSettings()->GetDesktopResolution():D.Resolution;
        Setting(TEXT("输出分辨率"),FString::Printf(TEXT("%d × %d"),R.X,R.Y),7,310,false,0,D.WindowMode!=1);
        Setting(TEXT("垂直同步"),D.VSync?TEXT("开启"):TEXT("关闭"),8,370);
        Setting(TEXT("帧率上限"),D.FrameLimit?FString::FromInt(D.FrameLimit):TEXT("无限制"),9,430);
        Text(TEXT("无边框使用当前桌面尺寸。显示切换需在 15 秒内确认。"),300,510,17);
        Text(TEXT("Alt + Enter：窗口 / 无边框切换。"),300,540,17);
        if(auto* P=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
            Text(FString::Printf(TEXT("当前场景渲染比例 %.0f%%，界面使用完整输出尺寸。"),P->GetFloat()),300,570,16);
    }
    if(C->SettingsTab==3)
    {
        auto Quality=[](int32 V)->FString{const TCHAR* N[]={TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")};return V<0?TEXT("自定义"):N[FMath::Clamp(V,0,3)];};
        Setting(TEXT("整体画质"),Quality(D.Preset),10,240);
        Setting(TEXT("阴影"),Quality(D.Shadows),11,292);
        Setting(TEXT("抗锯齿"),Quality(D.AntiAliasing),12,344);
        Setting(TEXT("后处理"),Quality(D.Post),13,396);
        Setting(TEXT("植被"),Quality(D.Foliage),14,448);
        Setting(TEXT("亮度 / Gamma"),FString::Printf(TEXT("%.2f"),D.Gamma),15,500,true,(D.Gamma-1.8f)/.8f);
    }
    if(C->SettingsTab==4)
    {
        for(int32 I=0;I<12;++I)
        {
            const float X=290+(I/6)*385,Y=240+(I%6)*52;
            Text(FCourtyardPreferences::ActionName(I),X,Y+8,18);
            Button(C->CaptureAction==I?TEXT("请按键…"):D.Keys[I].GetDisplayName().ToString(),X+195,Y,155,[this,I](float){C->CaptureAction=I;C->SetNotice(TEXT("按新键，Esc 取消；应用后生效"));});
        }
    }
    Button(TEXT("恢复默认"),290,625,190,[this](float){C->Draft=FCourtyardPreferences();C->CaptureAction=INDEX_NONE;C->SetNotice(TEXT("默认值已载入，应用后生效"));});
    Button(TEXT("取消"),580,625,190,[this](float){C->Draft=C->Applied;C->CaptureAction=INDEX_NONE;C->OpenPage(ECourtyardPage::Home);});
    Button(TEXT("应用"),805,625,190,[this](float){C->CaptureAction=INDEX_NONE;C->ApplyDraft();});
}
void ACourtyardHUD::DrawMap()
{
    // World-fixed +Y north. Geometry follows the actual courtyard path formula.
    // Every grand-manor map family (GrandManor, ManorFitout, ManorBehavior, ManorLife, ManorWelcome) shares this layout.
    const bool Manor=GetWorld()->GetMapName().Contains(TEXT("Manor"));
    const bool Upstairs=Manor&&C->GetPawn()&&C->GetPawn()->GetActorLocation().Z>350;
    if(Upstairs && C->ContentLayer.IsValid() && C->ContentLayer->FixEnabled)
    {
        // Only the merged-room candidate opts in; prior maps keep their original plan.
        bool MergedBathroom=false,SlidingRooms=false;
        for(TActorIterator<ACourtyardContentDoor> It(GetWorld());It;++It)
        {if(It->ActorHasTag(TEXT("MergedBathroom46")))MergedBathroom=true;if(It->SlidingDoor)SlidingRooms=true;}
        const float BedroomWallY=MergedBathroom?1530.f:1610.f;
        // Floor-focused plan at the same world orientation as the garden map.
        // New west-wing partitions are drawn with their real door gaps.
        Panel(116,134,720,565,FLinearColor(.022f,.041f,.051f,.95f));
        auto P=[&](float X,float Y){return FVector2D(476-X*.245f,177+(2160-Y)*.245f);};
        auto A=[&](float X,float Y,float W,float H,FLinearColor Col){auto V=P(X+W*.5f,Y+H*.5f);Rect(V.X,V.Y,W*.245f,H*.245f,Col);};
        auto L=[&](float X,float Y,float X2,float Y2){auto V=P(X,Y),W=P(X2,Y2);Line(V.X,V.Y,W.X,W.Y,Gold,2);};
        auto Label=[&](float X,float Y,const TCHAR* N,float Size=17.f){auto V=P(X,Y);Text(N,V.X-FCString::Strlen(N)*Size*.5f,V.Y-Size*.5f,Size,Ivory);};
        A(0,1240,2440,1840,FLinearColor(.21f,.25f,.27f));
        A(0,1240,2390,1790,FLinearColor(.09f,.13f,.15f));
        A(-905,980,610,1260,FLinearColor(.18f,.15f,.20f));
        A(880,1000,600,1300,FLinearColor(.13f,.20f,.19f));
        if(MergedBathroom)A(-910,1888,568,300,FLinearColor(.17f,.23f,.28f));
        else {A(-1064,1888,254,294,FLinearColor(.18f,.24f,.27f));A(-774,1888,292,294,FLinearColor(.17f,.23f,.28f));}
        for(int Side:{-1,1}){L(Side*565,340,Side*565,910);L(Side*565,1090,Side*565,(SlidingRooms&&Side==-1)?1552:1590);L(Side*565,(SlidingRooms&&Side==-1)?1708:1860,Side*565,2140);}
        L(-1240,BedroomWallY,-849,BedroomWallY);L(-733,BedroomWallY,-615,BedroomWallY);
        if(MergedBathroom)L(-1240,1730,-795,1730);
        else {L(-1240,1730,-1102,1730);L(-1002,1730,-795,1730);L(-925,1730,-925,2040);}
        L(-695,1730,-615,1730);
        L(-622,1730,-622,2040);L(-1240,2040,-622,2040);
        // Hinged-leaf marks identify doors without suggesting solid wall closures.
        if(SlidingRooms){L(-958,BedroomWallY-17,-848,BedroomWallY-17);L(-920,1747,-810,1747);}
        else {L(-733,BedroomWallY,-733,BedroomWallY-80);if(!MergedBathroom)L(-1002,1730,-1002,1810);L(-695,1730,-695,1810);}
        A(0,1760,385,510,FLinearColor(.055f,.08f,.10f));
        for(int K=0;K<12;++K){L(20,1550+K*38,175,1550+K*38);L(-175,1550+K*38,-20,1550+K*38);}
        Label(0,2070,TEXT("楼梯平台"),14);
        if(MergedBathroom)Label(-910,1900,TEXT("浴室"),23);
        else {Label(-1064,1900,TEXT("厕所"));Label(-774,1900,TEXT("浴室"));}
        Label(-905,1210,TEXT("卧室"),23);Label(-905,1680,TEXT("卫浴前廊"),12);
        Label(880,1180,TEXT("书房"),23);Label(850,1850,TEXT("资料架"),16);
        Label(0,1400,TEXT("画廊"),21);Label(0,1010,TEXT("起居厅"),21);Label(0,520,TEXT("音乐角"),21);
        auto Pos=C->GetPawn()->GetActorLocation();auto V=P(Pos.X,Pos.Y);float Yaw=FMath::DegreesToRadians(C->GetControlRotation().Yaw);
        FVector2D D(-FMath::Cos(Yaw),-FMath::Sin(Yaw)),Side(-D.Y,D.X);Ring(V.X,V.Y,12,Gold,2);Disk(V.X,V.Y,4,Ivory);
        Line(V.X+D.X*18,V.Y+D.Y*18,V.X-D.X*2+Side.X*5,V.Y-D.Y*2+Side.Y*5,Ivory,2);
        Line(V.X+D.X*18,V.Y+D.Y*18,V.X-D.X*2-Side.X*5,V.Y-D.Y*2-Side.Y*5,Ivory,2);
        Text(TEXT("庄园 · 二层"),872,170,27,Ivory);Text(TEXT("房间与通路"),872,211,17,Muted);Line(872,248,1155,248,Gold);
        Text(TEXT("沿画廊前往西翼卫浴"),872,285,18,Ivory);Text(TEXT("门洞留白，短线表示门扇"),872,325,16,Muted);
        Ring(883,404,9,Gold,2);Text(TEXT("你的位置与朝向"),906,393,18,Muted);
        Text(TEXT("上方为北侧"),872,461,17,Muted);Text(TEXT("下楼后显示完整庭院图"),872,499,17,Muted);
        Line(176,660,225,660,Gold,2);Text(TEXT("2 米"),182,671,13,Muted);
        Button(TEXT("返回庭院"),872,632,278,[this](float){C->OpenPage(ECourtyardPage::Game);});
        return;
    }
    const float S=Manor?.088f:.118f;
    // UE is left-handed: with +Y up the page, +X lies to the LEFT. Drawing +X to the right mirrored
    // the garden (turning right in game spun the arrow anticlockwise).
    auto P=[&](float X,float Y){return FVector2D(538-X*S,150+((Manor?2300:1300)-Y)*S);};
    auto Area=[&](float X,float Y,float W,float H,FLinearColor Col){auto A=P(X+W*.5f,Y+H*.5f);Rect(A.X,A.Y,W*S,H*S,Col);};
    auto Stroke=[&](FVector2D A,FVector2D B,FLinearColor Col,float W){auto A2=P(A.X,A.Y),B2=P(B.X,B.Y);Line(A2.X,A2.Y,B2.X,B2.Y,Col,W*S);};
    Panel(116,134,720,565,FLinearColor(.022f,.041f,.051f,.95f));
    // Subtle cartographic registration lines stay behind the grounds.
    for(int I=0;I<7;++I)Line(154+I*100,158,154+I*100,675,FLinearColor(.045f,.066f,.073f),.6f);
    for(int I=0;I<6;++I)Line(140,179+I*96,814,179+I*96,FLinearColor(.045f,.066f,.073f),.6f);
    Area(0,-1350,2800,3600,FLinearColor(.065f,.105f,.094f));
    for(int Side:{-1,1})Stroke(FVector2D(Side*1400,450),FVector2D(Side*1400,-3150),FLinearColor(.37f,.39f,.33f),20);
    Stroke(FVector2D(-1400,-3150),FVector2D(1400,-3150),Gold,18);
    Stroke(FVector2D(-1400,450),FVector2D(-550,450),Gold,18);Stroke(FVector2D(550,450),FVector2D(1400,450),Gold,18);
    // Planted masses, deliberately not a literal count of every plant instance.
    for(int I=0;I<50;++I)
    {
        float A=I*2*PI/50;auto V=P(-45+1190*FMath::Cos(A),-1410+1570*FMath::Sin(A));
        if(V.Y<270)continue;
        Disk(V.X,V.Y,13+(I%3)*2,FLinearColor(.09f,.16f,.14f));
        Disk(V.X+4,V.Y-3,6+(I%2)*2,FLinearColor(.25f,.22f,.34f));
    }
    for(int I=0;I<18;++I){float A=I*2*PI/18;auto V=P(600*FMath::Cos(A),-1410+740*FMath::Sin(A));Disk(V.X,V.Y,17,FLinearColor(.11f,.19f,.16f));Disk(V.X+5,V.Y,9,FLinearColor(.30f,.25f,.40f));}
    TArray<FVector2D> Loop;
    for(int I=0;I<=100;++I){float A=I*2*PI/100;Loop.Add(FVector2D(-45+1030*FMath::Cos(A),-1410+1430*FMath::Sin(A)));}
    for(int I=1;I<Loop.Num();++I)Stroke(Loop[I-1],Loop[I],FLinearColor(.29f,.31f,.29f),176);
    for(int I=-1200;I<1200;I+=40)Stroke(FVector2D(I,-1550+190*FMath::Sin(I/460.f)),FVector2D(I+40,-1550+190*FMath::Sin((I+40)/460.f)),FLinearColor(.29f,.31f,.29f),160);
    Stroke(FVector2D(-75,290),FVector2D(-75,-650),FLinearColor(.34f,.35f,.31f),170);
    Stroke(FVector2D(-265,-70),FVector2D(-265,-306),FLinearColor(.34f,.35f,.31f),110);
    Stroke(FVector2D(-325,-306),FVector2D(-75,-306),FLinearColor(.34f,.35f,.31f),170);
    Area(0,292,1440,116,FLinearColor(.45f,.43f,.36f));
    if(Manor){
        Area(0,1240,2440,1840,FLinearColor(.38f,.36f,.32f));Area(0,1240,2400,1800,FLinearColor(.18f,.20f,.22f));
        for(int Side:{-1,1}){
            Stroke(FVector2D(Side*565,340),FVector2D(Side*565,Upstairs?910:560),Gold,14);
            Stroke(FVector2D(Side*565,Upstairs?1090:1280),FVector2D(Side*565,1590),Gold,14);
            Stroke(FVector2D(Side*565,1860),FVector2D(Side*565,2140),Gold,14);
        }
        Area(0,1810,410,590,FLinearColor(.09f,.10f,.12f));
        for(int I=0;I<10;++I)Stroke(FVector2D(-185,1580+I*30),FVector2D(185,1580+I*30),FLinearColor(.45f,.40f,.29f),7);
        auto ST=P(0,2080);Text(TEXT("楼梯"),ST.X-14,ST.Y,11,Gold);
        if(Upstairs){Area(-885,830,212,254,FLinearColor(.27f,.34f,.39f));Area(880,890,176,84,FLinearColor(.42f,.35f,.24f));}
    }else{
        Area(0,815,1100,960,FLinearColor(.38f,.36f,.32f));Area(0,815,1036,896,FLinearColor(.18f,.20f,.22f));
        Area(-499,955,50,166,FLinearColor(.42f,.35f,.24f));Area(398,1160,67,202,FLinearColor(.42f,.35f,.24f));Area(200,755,145,110,FLinearColor(.35f,.29f,.24f));
    }
    Area(-740,-1920,330,430,FLinearColor(.41f,.35f,.29f));
    for(int I=0;I<5;++I)Stroke(FVector2D(-880+I*70,-1730),FVector2D(-880+I*70,-2110),FLinearColor(.21f,.26f,.24f),20);
    auto Pool=P(0,-1410);Disk(Pool.X,Pool.Y,445*S,FLinearColor(.32f,.34f,.31f));Disk(Pool.X,Pool.Y,304*S,FLinearColor(.48f,.50f,.45f));Disk(Pool.X,Pool.Y,279*S,FLinearColor(.12f,.25f,.31f));Ring(Pool.X,Pool.Y,207*S,FLinearColor(.33f,.48f,.51f));Disk(Pool.X,Pool.Y,69*S,FLinearColor(.59f,.59f,.48f));
    if(Manor){
        // Garden trees as placed in the manor maps; the old entrance blossom tree is hidden there.
        const FVector2D Trees[]={{-1190,-370},{1170,-900},{-720,-90},{720,-90},{-1190,-1380},{1210,-2320},{-1110,-2490},{-640,-2960},{820,-2860},{1240,-1700},{-740,-800},{-30,-370}};
        for(const FVector2D& T:Trees){auto V=P(T.X,T.Y);Disk(V.X,V.Y,9,FLinearColor(.10f,.20f,.15f));Disk(V.X-2,V.Y-2,5,FLinearColor(.17f,.29f,.21f));}
        // Ancient cherry: about 12 m of blossom crown around a single trunk.
        auto Cherry=P(-800,-2530);
        Disk(Cherry.X,Cherry.Y,600*S,FLinearColor(.36f,.25f,.38f,.92f));
        for(int I=0;I<7;++I){const float A=I*2*PI/7;Disk(Cherry.X+FMath::Cos(A)*30,Cherry.Y+FMath::Sin(A)*30,22,FLinearColor(.50f,.36f,.48f,.85f));}
        Disk(Cherry.X,Cherry.Y,5,FLinearColor(.24f,.17f,.15f));
        // Porch chime under the eaves beside the entrance.
        auto Chime=P(59,170);Ring(Chime.X,Chime.Y,4,Gold,1.5f);Line(Chime.X,Chime.Y+4,Chime.X,Chime.Y+9,Gold,1.5f);
        Text(TEXT("风铃"),Chime.X+7,Chime.Y-4,10,Muted);
        // Room names for the floor the player is on.
        auto Label=[&](float X,float Y,const TCHAR* N){auto V=P(X,Y);Text(N,V.X-FCString::Strlen(N)*5.5f,V.Y-6,11,Muted);};
        if(Upstairs){Label(-880,820,TEXT("卧室"));Label(-870,1950,TEXT("更衣角"));Label(880,900,TEXT("书房"));Label(0,520,TEXT("音乐角"));Label(330,1150,TEXT("起居厅"));}
        else{Label(-880,900,TEXT("阅读室"));Label(880,900,TEXT("会客厅"));Label(330,1250,TEXT("壁炉"));Label(850,1750,TEXT("餐厅"));Label(-850,1700,TEXT("资料桌"));}
    }else{
        auto Tree=P(-315,135);Disk(Tree.X,Tree.Y,27,FLinearColor(.42f,.31f,.43f));Disk(Tree.X-14,Tree.Y-5,17,FLinearColor(.50f,.37f,.47f));Disk(Tree.X+12,Tree.Y-9,18,FLinearColor(.46f,.35f,.47f));
    }
    Area(Manor?-210:-265,110,Manor?220:160,50,FLinearColor(.68f,.56f,.35f));
    auto Pin=[&](float X,float Y,const FString& N){auto V=P(X,Y);Disk(V.X,V.Y,11,Ink);Ring(V.X,V.Y,11,Gold);Text(N,V.X-6,V.Y-8,13,Ivory);};
    Pin(0,815,TEXT("1"));Pin(-210,80,TEXT("2"));Pin(0,-1410,TEXT("3"));Pin(-740,-1920,TEXT("4"));Pin(-45,-2840,TEXT("5"));
    if(Manor)Pin(-800,-2530,TEXT("6"));
    if(APawn* Pawn=C->GetPawn())
    {
        auto L=Pawn->GetActorLocation();auto V=P(L.X,L.Y);float Yaw=FMath::DegreesToRadians(C->GetControlRotation().Yaw);FVector2D D(-FMath::Cos(Yaw),-FMath::Sin(Yaw)),Side(-D.Y,D.X);
        Ring(V.X,V.Y,14,FLinearColor(.95f,.78f,.42f,.8f),2);Disk(V.X,V.Y,4,Ivory);
        Line(V.X+D.X*17,V.Y+D.Y*17,V.X-D.X*3+Side.X*5,V.Y-D.Y*3+Side.Y*5,Ivory,2);
        Line(V.X+D.X*17,V.Y+D.Y*17,V.X-D.X*3-Side.X*5,V.Y-D.Y*3-Side.Y*5,Ivory,2);
    }
    Text(TEXT("北"),191,163,18,Gold);Line(200,194,200,229,Gold);Line(200,194,194,205,Gold);Line(200,194,206,205,Gold);
    Line(156,649,156+500*S,649,Gold,2);Line(156,644,156,654,Gold);Line(156+500*S,644,156+500*S,654,Gold);Text(TEXT("5 米"),172,661,13,Muted);
    Text(TEXT("循着花径，慢慢走"),872,163,22,Ivory);Text(TEXT("庭院导览"),872,198,14,Muted);Line(872,237,1155,237,Gold);
    const TCHAR* Names[]={Manor?(Upstairs?TEXT("庄园 · 二层"):TEXT("庄园 · 一层")):TEXT("暖窗起居室"),TEXT("花树座椅"),TEXT("中央喷泉"),TEXT("静谧凉架"),TEXT("远端观景"),TEXT("古樱花")};
    const int32 Entries=Manor?6:5;const float Step=Manor?42.f:48.f;
    for(int I=0;I<Entries;++I){Disk(886,282+I*Step,10,Ink);Ring(886,282+I*Step,10,Gold);Text(FString::FromInt(I+1),880,274+I*Step,13,Ivory);Text(Names[I],912,272+I*Step,19,Ivory);}
    Ring(883,544,8,Gold,2);Text(TEXT("你的位置与朝向"),907,534,17,Muted);
    Disk(883,582,7,FLinearColor(.30f,.25f,.40f));Text(TEXT("花境与种植区"),907,572,17,Muted);
    Button(TEXT("返回庭院"),872,632,278,[this](float){C->OpenPage(ECourtyardPage::Game);});
}
void ACourtyardHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;C=Cast<ACourtyardController>(PlayerOwner);if(!C)return;
    Scale=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/800.f);OffsetX=(Canvas->SizeX-1280*Scale)*.5f;OffsetY=(Canvas->SizeY-800*Scale)*.5f;Buttons.Empty();
    if(C->Page==ECourtyardPage::Game)
    {
        if(C->CanHighFiveReply())Text(C->KeyLabel(ECourtyardAction::Respond)+TEXT("  轻轻击掌"),45,675,22);
        else if(C->CanRespond())Text(C->KeyLabel(ECourtyardAction::Respond)+TEXT("  轻轻摸头"),45,675,22);
        if(C->CanSeat())Text(C->KeyLabel(ECourtyardAction::Seat)+(C->Pose==ECourtyardPose::Seated?TEXT("  起身"):TEXT("  坐下")),45,713,22);
        Text(TEXT("Esc 设置   ·   ")+C->KeyLabel(ECourtyardAction::Map)+TEXT(" 地图"),990,755,15);
    }
    else
    {
        DrawRect(FLinearColor(.015f,.02f,.05f,.57f),0,0,Canvas->SizeX,Canvas->SizeY);
        Panel(80,56,1120,684,Ink);Line(111,119,1169,119,FLinearColor(.28f,.28f,.28f));
        Text(C->Page==ECourtyardPage::Map?TEXT("月夜庭院  /  花园手绘图"):TEXT("月夜庭院"),118,78,27,Ivory);
        Text(TEXT("一处可以安心停留的月夜"),851,88,15,Muted);
        Line(111,708,1169,708,FLinearColor(.18f,.21f,.25f));Text(TEXT("Esc  返回"),119,719,13,Muted);
        Text(TEXT("风与花香，仍在庭院里继续"),920,719,13,Muted);
        if(C->Page==ECourtyardPage::Home)
        {
            Text(TEXT("把时间，留给月色。"),148,176,32,Ivory);
            Text(TEXT("在星光与花影之间，稍作停留。"),151,228,18,Muted);
            Ring(396,443,139,FLinearColor(.21f,.26f,.32f));Ring(396,443,130,FLinearColor(.13f,.19f,.26f));
            Disk(397,401,40,FLinearColor(.65f,.62f,.50f));Disk(415,388,36,Ink);
            for(int I=0;I<13;++I){float X=301+(I*73%193),Y=332+(I*47%162);Disk(X,Y,I%4==0?1.8f:1.f,FLinearColor(.45f,.51f,.62f));}
            for(int I=0;I<9;++I){float X=300+I*24,Y=520+FMath::Sin(I*.8f)*16;Line(X,Y,X+5,Y-43,FLinearColor(.26f,.34f,.32f));Disk(X+5,Y-46,5,FLinearColor(.43f,.35f,.48f));Disk(X,Y-42,4,FLinearColor(.35f,.29f,.40f));}
            Line(296,574,496,574,Gold);Text(TEXT("月夜  ·  花园  ·  相伴"),285,602,17,Muted);
            MenuCard(TEXT("继续漫步"),TEXT("回到风与花影之间"),205,[this](float){C->OpenPage(ECourtyardPage::Game);});
            MenuCard(TEXT("偏好设置"),TEXT("声音、画面与操作"),300,[this](float){C->OpenPage(ECourtyardPage::Settings);});
            MenuCard(TEXT("操作指引"),TEXT("查看行走、姿态与互动按键"),395,[this](float){C->OpenPage(ECourtyardPage::Controls);});
            MenuCard(TEXT("离开庭院"),TEXT("为下一次相见，留一盏灯"),490,[this](float){C->OpenPage(ECourtyardPage::Exit);});
            Text(TEXT("樱羽艾玛（MMD）模型：POWER @MintLove2020"),148,647,15,Ivory);
            Text(TEXT("已获得制作者许可，以游戏形式使用与分发。"),148,675,15,Muted);
        }
        else if(C->Page==ECourtyardPage::Settings)DrawSettings();
        else if(C->Page==ECourtyardPage::Map)DrawMap();
        else if(C->Page==ECourtyardPage::Controls)
        {
            for(int32 I=0;I<12;++I)Text(C->Applied.Keys[I].GetDisplayName().ToString()+TEXT("  ·  ")+FCourtyardPreferences::ActionName(I),300+(I/6)*375,230+(I%6)*48,19);
            Text(TEXT("低姿态起身需有空间；坐姿使用座椅键退出。"),300,555,17);
            Button(TEXT("返回"),800,625,200,[this](float){C->Escape();});
        }
        else if(C->Page==ECourtyardPage::Exit)
        {
            Text(TEXT("要离开月夜庭院吗？"),460,300,25);
            Button(TEXT("留在庭院"),400,420,220,[this](float){C->Escape();});
            Button(TEXT("退出游戏"),660,420,220,[this](float){UKismetSystemLibrary::QuitGame(C,C,EQuitPreference::Quit,false);});
        }
        else if(C->Page==ECourtyardPage::DisplayConfirm)
        {
            Text(TEXT("保留新的显示设置？"),410,290,25);
            Text(FString::Printf(TEXT("%.0f 秒后自动恢复上一次设置"),FMath::Max(0.,C->ConfirmUntil-FPlatformTime::Seconds())),410,345,20);
            Button(TEXT("恢复"),400,440,220,[this](float){C->ConfirmDisplay(false);});
            Button(TEXT("保留"),660,440,220,[this](float){C->ConfirmDisplay(true);});
        }
    }
    if(FPlatformTime::Seconds()<C->NoticeUntil) {Rect(280,715,720,42,FLinearColor(.03f,.04f,.09f,.96f));Text(C->Notice,295,725,18);}
}
