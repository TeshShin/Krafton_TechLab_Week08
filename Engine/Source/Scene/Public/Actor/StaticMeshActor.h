#pragma once
#include "Scene/Public/Actor/Actor.h"
#include "Scene/Public/Component/StaticMeshComponent.h"

class UCubeComponent;

UCLASS()
class AStaticMeshActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(AStaticMeshActor, AActor)

public:
	AStaticMeshActor();
	
	virtual UClass* GetDefaultRootComponent() override;

private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
};
