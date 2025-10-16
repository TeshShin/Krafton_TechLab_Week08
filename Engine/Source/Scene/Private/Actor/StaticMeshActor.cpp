#include "pch.h"
#include "Scene/Public/Actor/StaticMeshActor.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor)

AStaticMeshActor::AStaticMeshActor()
{
}

UClass* AStaticMeshActor::GetDefaultRootComponent()
{
	return UStaticMeshComponent::StaticClass();
}
