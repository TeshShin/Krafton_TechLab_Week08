#pragma once
#include "Scene/Public/Actor/Actor.h"

class UTriangleComponent;

UCLASS()
class ATriangleActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(ATriangleActor, AActor)

public:
	ATriangleActor();
	virtual ~ATriangleActor() override {}
	
	virtual UClass* GetDefaultRootComponent() override;
	virtual void InitializeComponents() override;

private:
	UTriangleComponent* TriangleComponent = nullptr;
};
