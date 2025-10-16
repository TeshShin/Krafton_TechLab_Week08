#include "pch.h"
#include "Scene/Public/Actor/TriangleActor.h"
#include "Scene/Public/Component/StaticMeshComponent.h"

IMPLEMENT_CLASS(ATriangleActor, AActor)

ATriangleActor::ATriangleActor()
{
}

UClass* ATriangleActor::GetDefaultRootComponent()
{
    return UStaticMeshComponent::StaticClass();
}

void ATriangleActor::InitializeComponents()
{
    Super::InitializeComponents();
    Cast<UStaticMeshComponent>(GetRootComponent())->SetStaticMesh("Data/Shapes/Triangle.obj");
}
