#include "pch.h"
#include "Core/Public/Object/Object.h"
#include "Core/Public/Object/Property.h"
#include "Core/Public/EngineStatics.h"
#include "Core/Public/Container/Name.h"
#include "json.hpp"


uint32 UEngineStatics::NextUUID = 0;

TArray<UObject*>& GetUObjectArray()
{
	static TArray<UObject*> GUObjectArray;
	return GUObjectArray;
}

IMPLEMENT_CLASS_BASE(UObject)

UObject::UObject()
	: Name(FName::GetNone()), Outer(nullptr)
{
	UUID = UEngineStatics::GenUUID();

	GetUObjectArray().emplace_back(this);
	InternalIndex = static_cast<uint32>(GetUObjectArray().size()) - 1;
}

UObject::~UObject()
{
	/** @todo: 이후에 리뷰 필요 */

	TArray<UObject*>& ObjArray = GetUObjectArray();
	const size_t IndexToRemove = this->InternalIndex;
	const size_t LastIndex = ObjArray.size() - 1;

	if (IndexToRemove > LastIndex || ObjArray[IndexToRemove] != this)
	{
		return;
	}

	if (IndexToRemove == LastIndex)
	{
		ObjArray.pop_back();
	}
	else
	{
		UObject* LastObject = ObjArray[LastIndex];
		ObjArray[IndexToRemove] = LastObject;

		LastObject->InternalIndex = IndexToRemove;
		ObjArray.pop_back();
	}
}

void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// UPROPERTY 시스템을 사용한 자동 직렬화
	// GetClass()는 virtual이므로 실제 인스턴스 타입의 모든 프로퍼티를 가져옴
	UClass* Class = GetClass();
	if (!Class)
		return;

	// 이 클래스와 모든 부모 클래스의 프로퍼티 수집
	TArray<UPropertyBase*> AllProperties;
	Class->GetAllProperties(AllProperties);

	for (UPropertyBase* Prop : AllProperties)
	{
		if (!Prop)
			continue;

		// SaveGame 플래그가 있는 프로퍼티만 직렬화
		if (!Prop->HasAnyFlags(EPropertyFlags::SaveGame))
			continue;

		const char* PropName = Prop->GetName();

		if (bInIsLoading)
		{
			// 로딩: JSON에서 값을 읽어 프로퍼티에 설정
			try
			{
				// JSON 키가 존재하는지 확인 (hasKey 또는 예외 처리)
				// 키가 없으면 기본값 유지
				if (InOutHandle.hasKey(PropName))
				{
					// JSON 값을 문자열로 변환
					std::string jsonValue = InOutHandle[PropName].ToString();
					FString valueStr(jsonValue.c_str());

					// FromString으로 프로퍼티에 설정
					if (!Prop->FromString(this, valueStr))
					{
						UE_LOG("UObject::Serialize: Failed to deserialize property '%s' of class '%s'",
							PropName, Class->GetName().ToString().data());
					}
				}
			}
			catch (...)
			{
				UE_LOG("UObject::Serialize: Exception while deserializing property '%s' of class '%s'",
					PropName, Class->GetName().ToString().data());
			}
		}
		else
		{
			// 저장: 프로퍼티 값을 JSON에 쓰기
			try
			{
				FString valueStr = Prop->ToString(this);
				InOutHandle[PropName] = valueStr.c_str();
			}
			catch (...)
			{
				UE_LOG("UObject::Serialize: Exception while serializing property '%s' of class '%s'",
					PropName, Class->GetName().ToString().data());
			}
		}
	}
}

UObject* UObject::Duplicate()
{
	// 새 인스턴스 생성 (GetClass()는 virtual이므로 실제 타입으로 생성)
	UObject* NewObj = NewObject(GetClass());
	if (!NewObj)
		return nullptr;

	// UPROPERTY 시스템을 사용한 자동 복제
	UClass* Class = GetClass();
	if (!Class)
		return NewObj;

	// 이 클래스와 모든 부모 클래스의 프로퍼티 수집
	TArray<UPropertyBase*> AllProperties;
	Class->GetAllProperties(AllProperties);

	for (UPropertyBase* Prop : AllProperties)
	{
		if (!Prop)
			continue;

		// DuplicateTransient 플래그가 있으면 복제하지 않음 (기본값 유지)
		if (Prop->HasAnyFlags(EPropertyFlags::DuplicateTransient))
			continue;

		try
		{
			// ToString/FromString으로 안전하게 복제
			FString valueStr = Prop->ToString(this);
			if (!Prop->FromString(NewObj, valueStr))
			{
				UE_LOG("UObject::Duplicate: Failed to copy property '%s' of class '%s'",
					Prop->GetName(), Class->GetName().ToString().data());
			}
		}
		catch (...)
		{
			UE_LOG("UObject::Duplicate: Exception while copying property '%s' of class '%s'",
				Prop->GetName(), Class->GetName().ToString().data());
		}
	}

	// 하위 객체 복제 (하위 클래스가 오버라이드 가능)
	DuplicateSubObjects(NewObj);

	return NewObj;
}

void UObject::DuplicateSubObjects(UObject* DuplicatedObject)
{

}

void UObject::SetOuter(UObject* InObject)
{
	if (Outer == InObject)
	{
		return;
	}

	// 기존 Outer가 있었다면, 나의 전체 메모리 사용량을 빼달라고 전파
	// 새로운 Outer가 있다면, 나의 전체 메모리 사용량을 더해달라고 전파
	if (Outer)
	{
		Outer->PropagateMemoryChange(-static_cast<int64>(AllocatedBytes), -static_cast<int32>(AllocatedCounts));
	}

	Outer = InObject;

	if (Outer)
	{
		Outer->PropagateMemoryChange(AllocatedBytes, AllocatedCounts);
	}
}

void UObject::AddMemoryUsage(uint64 InBytes, uint32 InCount)
{
	uint64 BytesToAdd = InBytes;

	if (!BytesToAdd)
	{
		BytesToAdd = GetClass()->GetClassSize();
	}

	// 메모리 변경 전파
	PropagateMemoryChange(BytesToAdd, InCount);
}

void UObject::RemoveMemoryUsage(uint64 InBytes, uint32 InCount)
{
	PropagateMemoryChange(-static_cast<int64>(InBytes), -static_cast<int32>(InCount));
}

void UObject::PropagateMemoryChange(uint64 InBytesDelta, uint32 InCountDelta)
{
	// 자신의 값에 변화량을 더함
	AllocatedBytes += InBytesDelta;
	AllocatedCounts += InCountDelta;

	// Outer가 있다면, 동일한 변화량을 그대로 전파
	if (Outer)
	{
		Outer->PropagateMemoryChange(InBytesDelta, InCountDelta);
	}
}

/**
 * @brief 해당 클래스가 현재 내 클래스의 조상 클래스인지 판단하는 함수
 * 내부적으로 재귀를 활용해서 부모를 계속 탐색한 뒤 결과를 반환한다
 * @param InClass 판정할 Class
 * @return 판정 결과
 */
bool UObject::IsA(UClass* InClass) const
{
	if (!InClass)
	{
		return false;
	}

	return GetClass()->IsChildOf(InClass);
}

/**
 * @brief 해당 클래스가 현재 내 클래스와 동일한지 판단하는 함수
 * @return 판정 결과
 */
bool UObject::IsExactly(UClass* InClass) const
{
	if (!InClass)
	{
		return false;
	}

	return GetClass() == InClass;
}
