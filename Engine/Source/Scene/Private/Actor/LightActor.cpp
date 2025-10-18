#include "pch.h"
#include "Scene/Public/Actor/LightActor.h"
#include "Scene/Public/Component/BillBoardComponent.h"

IMPLEMENT_ABSTRACT_CLASS(ALightActor, AActor)

ALightActor::ALightActor()
{
}

UClass* ALightActor::GetDefaultRootComponent()
{
    return GetLightClass();
}

void ALightActor::InitializeComponents()
{
    Super::InitializeComponents();
}
