#include "pch.h"
#include "Scene/Public/Actor/DecalSpotLightActor.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/DecalSpotLightComponent.h"
#include "Manager/Public/AssetManager.h"

IMPLEMENT_CLASS(ADecalSpotLightActor, AActor)

ADecalSpotLightActor::ADecalSpotLightActor()
{
}

UClass* ADecalSpotLightActor::GetDefaultRootComponent()
{
	return UDecalSpotLightComponent::StaticClass();
}

void ADecalSpotLightActor::InitializeComponents()
{
	Super::InitializeComponents();
	
	UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
	Billboard->AttachToComponent(GetRootComponent());
	Billboard->SetIsVisualizationComponent(true);
	Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/SpotLight_64x.png"));
	Billboard->SetScreenSizeScaled(true);
}
