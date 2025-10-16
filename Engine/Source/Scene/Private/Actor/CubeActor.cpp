#include "pch.h"
#include "Scene/Public/Actor/CubeActor.h"
#include "Scene/Public/Component/StaticMeshComponent.h"

IMPLEMENT_CLASS(ACubeActor, AActor)

ACubeActor::ACubeActor()
{
}

UClass* ACubeActor::GetDefaultRootComponent()
{
    return UStaticMeshComponent::StaticClass();
}

void ACubeActor::InitializeComponents()
{
    Super::InitializeComponents();
    Cast<UStaticMeshComponent>(GetRootComponent())->SetStaticMesh("Data/Shapes/Cube.obj");
}
