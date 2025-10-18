#include "pch.h"
#include "Scene/Public/Actor/AmbientLightActor.h"
#include "Manager/Public/AssetManager.h"
#include "Scene/Public/Component/AmbientLightComponent.h"

IMPLEMENT_CLASS(AAmbientLightActor, ALightActor)
UClass* AAmbientLightActor::GetLightClass()
{
    return UAmbientLightComponent::StaticClass();
}
