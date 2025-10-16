#pragma once
#include "Scene/Public/Actor/Actor.h"

UCLASS()
class APointLightActor : public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(APointLightActor, AActor)
	
public:
    APointLightActor();

    virtual UClass* GetDefaultRootComponent() override;
    virtual void InitializeComponents() override;
};
