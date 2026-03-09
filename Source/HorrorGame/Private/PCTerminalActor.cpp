// PCTerminalActor.cpp
#include "PCTerminalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"

APCTerminalActor::APCTerminalActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh"));
    MonitorMesh->SetupAttachment(Root);

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(Root);
    InteractionBox->SetBoxExtent(FVector(40.f, 40.f, 40.f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractionBox->SetGenerateOverlapEvents(false);

    ArrowWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArrowWidget"));
    ArrowWidget->SetupAttachment(InteractionBox);
    ArrowWidget->SetWidgetSpace(EWidgetSpace::World);
    ArrowWidget->SetDrawAtDesiredSize(true);
    ArrowWidget->SetVisibility(false);
    ArrowWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArrowWidget->SetBlendMode(EWidgetBlendMode::Transparent);

    InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
    InteractionWidget->SetupAttachment(InteractionBox);
    InteractionWidget->SetWidgetSpace(EWidgetSpace::World);
    InteractionWidget->SetDrawAtDesiredSize(true);
    InteractionWidget->SetVisibility(false);
    InteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent);

    InteractionCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera"));
    InteractionCamera->SetupAttachment(Root);
    InteractionCamera->bAutoActivate = false;

    // default monitor render target sizes (tweakable in BP)
    RenderTargetWidth = 1024;
    RenderTargetHeight = 768;
}

void APCTerminalActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("PCTerminal BeginPlay running"));
    // Setup material/render target for the monitor (if ScreenBaseMaterial assigned)
    SetupRenderTarget();
}

void APCTerminalActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* Player = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Player)
        return;

    // -------- Arrow visibility --------
    const bool bArrowShouldShow = CanShowInteraction(Player);

    if (ArrowWidget)
        ArrowWidget->SetVisibility(bArrowShouldShow);

    // -------- Rotate widgets toward camera --------

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager)
        return;

    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

    auto FaceCamera = [&](UWidgetComponent* Widget)
    {
        if (!Widget || !Widget->IsVisible())
            return;

        FVector ToCamera = (CameraLocation - Widget->GetComponentLocation()).GetSafeNormal();
        FRotator FaceRot = ToCamera.Rotation();
        FaceRot.Pitch = 0.f;
        Widget->SetWorldRotation(FaceRot);
    };

    FaceCamera(ArrowWidget);
    FaceCamera(InteractionWidget);
}

bool APCTerminalActor::CanShowInteraction(APawn* Player) const
{
    if (!Player) return false;

    // Long-range check; no on-screen test here (character can do that if needed)
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    return Dist <= InteractionMaxDistance;
}

bool APCTerminalActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;

    // optional: check if on screen
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return true;

    FVector2D ScreenPos;
    const bool bProjected = PC->ProjectWorldLocationToScreen(InteractionBox->GetComponentLocation(), ScreenPos, true);
    if (!bProjected) return false;

    int32 SizeX = 0, SizeY = 0;
    PC->GetViewportSize(SizeX, SizeY);
    return ScreenPos.X >= 0 && ScreenPos.X <= SizeX && ScreenPos.Y >= 0 && ScreenPos.Y <= SizeY;
}

void APCTerminalActor::SetFullWidgetVisible(bool bVisible, const APawn* Player)
{
    if (InteractionWidget)
    {
        InteractionWidget->SetVisibility(bVisible);
    }
}

FVector APCTerminalActor::GetInteractionLocation() const
{
    return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation();
}

void APCTerminalActor::DeactivateInteractionCamera()
{
    if (InteractionCamera)
        InteractionCamera->Deactivate();
}

/* ----------------- RenderTarget / Canvas code (reuse your working approach) ----------------- */

void APCTerminalActor::SetupRenderTarget()
{
    if (!ScreenBaseMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: ScreenBaseMaterial not assigned in %s"), *GetName());
        return;
    }

    // Create dynamic material instance on the mesh (so we can set the texture parameter)
    DynMaterial = MonitorMesh->CreateDynamicMaterialInstance(ScreenMaterialIndex, ScreenBaseMaterial);
    if (!DynMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: Failed to create dynamic material instance."));
        return;
    }

    // Create CanvasRenderTarget2D
    CanvasRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
        this->GetWorld(),
        UCanvasRenderTarget2D::StaticClass(),
        RenderTargetWidth,
        RenderTargetHeight
    );

    if (!CanvasRenderTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: Failed to create CanvasRenderTarget2D."));
        return;
    }

    // Bind update event
    CanvasRenderTarget->OnCanvasRenderTargetUpdate.AddDynamic(
        this,
        &APCTerminalActor::OnCanvasUpdate
    );

    // Assign the canvas render target as the value of the "ScreenTexture" parameter
    FName ParamName = FName(TEXT("ScreenTexture"));
    DynMaterial->SetTextureParameterValue(ParamName, CanvasRenderTarget);

    // Force an initial update
    CanvasRenderTarget->UpdateResource();
}

void APCTerminalActor::OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height)
{
    if (!Canvas)
        return;

    // Clear background
    Canvas->K2_DrawBox(
        FVector2D(0,0),
        FVector2D(Width,Height),
        1.0f,
        FLinearColor::Black
    );

    // Choose a font
    UFont* Font = GEngine->GetSmallFont();
    if (!Font)
    {
        UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: No font found."));
    }

    // Terminal style
    FLinearColor TextColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
    float Scale = 1.0f;

    FVector2D DrawPos(Width * 0.075f, Height * 0.2f);

    // Draw multiple lines
    TArray<FString> Lines;
    TerminalText.ParseIntoArrayLines(Lines);

    float LineHeight = Font ? Font->GetMaxCharHeight() * Scale + 6.0f : 24.0f;

    for (int32 i = 0; i < Lines.Num(); ++i)
    {
        const FString& Line = Lines[i];

        Canvas->K2_DrawText(
            Font,
            Line,
            DrawPos + FVector2D(0.0f, i * LineHeight),
            FVector2D(1.5f, 1.5f),
            TextColor,
            1.0f,
            FLinearColor::Black,
            FVector2D::ZeroVector,
            false,
            false,
            false,
            FLinearColor::Black
        );
    }
}