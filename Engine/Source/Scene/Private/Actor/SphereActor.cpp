#include "pch.h"
#include "Scene/Public/Actor/SphereActor.h"
#include "Scene/Public/Component/StaticMeshComponent.h"

IMPLEMENT_CLASS(ASphereActor, AActor)

ASphereActor::ASphereActor()
{
}

UClass* ASphereActor::GetDefaultRootComponent()
{
	return UStaticMeshComponent::StaticClass();
}

void ASphereActor::InitializeComponents()
{
	Super::InitializeComponents();
	Cast<UStaticMeshComponent>(GetRootComponent())->SetStaticMesh("Data/Shapes/Sphere.obj");
}
