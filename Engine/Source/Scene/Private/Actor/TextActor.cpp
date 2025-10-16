#include "pch.h"
#include "Scene/Public/Actor/TextActor.h"

IMPLEMENT_CLASS(ATextActor, AActor)

ATextActor::ATextActor()
{
}

UClass* ATextActor::GetDefaultRootComponent()
{
    return UTextComponent::StaticClass();
}
