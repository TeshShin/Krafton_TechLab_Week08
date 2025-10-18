#pragma once
#include "LightActor.h"

UCLASS()
class APointLightActor : public ALightActor
{
    GENERATED_BODY()
    DECLARE_CLASS(APointLightActor, AActor)

public:
    APointLightActor();

protected:
	virtual UClass* GetLightClass() override;
};
