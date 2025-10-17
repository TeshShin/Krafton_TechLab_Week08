#include "pch.h"
#include "Scene/Public/Actor/DirectionalLightActor.h"
#include "Manager/Public/AssetManager.h"
#include "Scene/Public/Component/DirectionalLightComponent.h"

IMPLEMENT_CLASS(ADirectionalLightActor, ALightActor)

UClass* ADirectionalLightActor::GetLightClass()
{
    return UDirectionalLightComponent::StaticClass();
}

class UTexture* ADirectionalLightActor::GetLightBillboardTexture()
{
    return UAssetManager::GetInstance().LoadTexture("Data/Icons/DirectionalLight_64x.png");
}
