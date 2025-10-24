#include "pch.h"
#include "Scene/Public/Actor/SpotLightActor.h"
#include "Manager/Public/AssetManager.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

ASpotLightActor::ASpotLightActor()
{
}

UClass* ASpotLightActor::GetDefaultRootComponent()
{
    return USpotLightComponent::StaticClass();
}

void ASpotLightActor::InitializeComponents()
{
    Super::InitializeComponents();
}
