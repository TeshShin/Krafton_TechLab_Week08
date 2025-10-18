#include "pch.h"
#include "Scene/Public/Actor/DirectionalLightActor.h"
#include "Scene/Public/Component/DirectionalLightComponent.h"

IMPLEMENT_CLASS(ADirectionalLightActor, ALightActor)

UClass* ADirectionalLightActor::GetLightClass()
{
    return UDirectionalLightComponent::StaticClass();
}
