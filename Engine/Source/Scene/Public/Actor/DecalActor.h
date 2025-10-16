#pragma once

#include "Scene/Public/Actor/Actor.h"

UCLASS()
class ADecalActor : public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(ADecalActor, AActor)

public:
    ADecalActor();

    virtual UClass* GetDefaultRootComponent() override;
    virtual void InitializeComponents() override;
};
