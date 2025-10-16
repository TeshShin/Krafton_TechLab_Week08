#pragma once
#include "Scene/Public/Actor/Actor.h"
#include "Scene/Public/Component/TextComponent.h"

UCLASS()
class ATextActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(ATextActor, AActor)

public:
	ATextActor();
	
	virtual UClass* GetDefaultRootComponent() override;
private:
	UTextComponent* TextComponent = nullptr;
};
