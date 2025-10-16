#pragma once
#include "Scene/Public/Actor/Actor.h"
#include "Scene/Public/Component/BillBoardComponent.h"

UCLASS()
class ABillBoardActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(ABillBoardActor, AActor)

public:
	ABillBoardActor();

	virtual UClass* GetDefaultRootComponent() override;
private:
	UBillBoardComponent* BillBoardComponent = nullptr;
};
