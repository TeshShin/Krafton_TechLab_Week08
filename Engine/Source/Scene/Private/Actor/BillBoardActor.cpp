#include "pch.h"
#include "Scene/Public/Actor/BillBoardActor.h"
#include "Scene/Public/Component/BillBoardComponent.h"

IMPLEMENT_CLASS(ABillBoardActor, AActor)

ABillBoardActor::ABillBoardActor()
{
}

UClass* ABillBoardActor::GetDefaultRootComponent()
{
    return UBillBoardComponent::StaticClass();
}
