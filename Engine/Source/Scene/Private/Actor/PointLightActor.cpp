#include "pch.h"
#include "Scene/Public/Actor/PointLightActor.h"
#include "Scene/Public/Component/PointLightComponent.h"

IMPLEMENT_CLASS(APointLightActor, ALightActor)

APointLightActor::APointLightActor()
{
}

UClass* APointLightActor::GetLightClass()
{
	return UPointLightComponent::StaticClass();
}
