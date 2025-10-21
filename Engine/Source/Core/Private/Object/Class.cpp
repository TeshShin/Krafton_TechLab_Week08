#include "pch.h"
#include "Core/Public/Object/Class.h"
#include "Core/Public/Object/Object.h"
#include "Core/Public/Object/Property.h"

using std::stringstream;

void UClass::SignUpClass(UClass* InClass)
{
	if (InClass)
	{
		for (UClass* Class : GetAllClasses())
		{
			if (Class == InClass) return;
		}
		GetAllClasses().emplace_back(InClass);
		UE_LOG("UClass: Class registered: %s (Total: %llu)", InClass->GetName().ToString().data(), GetAllClasses().size());
	}
}

UClass* UClass::FindClass(const FName& InClassName)
{
	for (UClass* Class : GetAllClasses())
	{
		if (Class && Class->GetName() == InClassName)
		{
			return Class;
		}
	}

	return nullptr;
}

TArray<UClass*> UClass::FindClasses(UClass* SuperClass)
{
	TArray<UClass*> Classes;

	for (UClass* Class: GetAllClasses())
	{
		if (Class->IsChildOf(SuperClass))
		{
			Classes.push_back(Class);
		}
	}

	return Classes;
}

TArray<UClass*>& UClass::GetAllClasses()
{
	static TArray<UClass*> AllClasses;
	return AllClasses;
}

/**
 * @brief UClass Constructor
 * @param InName Class 이름
 * @param InSuperClass Parent Class
 * @param InClassSize Class Size
 * @param InConstructor 생성자 함수 포인터
 */
UClass::UClass(const FName& InName, UClass* InSuperClass, size_t InClassSize, ClassConstructorType InConstructor, bool InIsAbstract)
	: ClassName(InName), SuperClass(InSuperClass), ClassSize(InClassSize), Constructor(InConstructor), bIsAbstract(InIsAbstract)
{
	UE_LOG("UClass: 클래스 등록: %s", ClassName.ToString().data());
}

/**
 * @brief 이 클래스가 지정된 클래스의 하위 클래스인지 확인
 * @param InClass 확인할 클래스
 * @return 하위 클래스이거나 같은 클래스면 true
 */
bool UClass::IsChildOf(UClass* InClass) const
{
	if (!InClass)
	{
		return false;
	}

	// 자기 자신과 같은 클래스인 경우
	if (this == InClass)
	{
		return true;
	}

	// 부모 클래스들을 거슬러 올라가면서 확인
	const UClass* CurrentClass = this;
	while (CurrentClass)
	{
		if (CurrentClass->ClassName == InClass->ClassName)
		{
			return true;
		}

		CurrentClass = CurrentClass->SuperClass;
	}

	return false;
}

/**
 * @brief 새로운 인스턴스 생성
 * @return 생성된 객체 포인터
 */
UObject* UClass::CreateDefaultObject() const
{
	if (Constructor)
	{
		return Constructor();
	}

	return nullptr;
}

/**
 * @brief 프로퍼티를 클래스에 등록
 * @param Property 등록할 프로퍼티 메타데이터
 */
void UClass::AddProperty(UPropertyBase* Property)
{
	if (!Property)
		return;

	// 중복 등록 방지
	for (UPropertyBase* ExistingProp : Properties)
	{
		if (ExistingProp == Property)
			return;
	}

	Properties.emplace_back(Property);
	UE_LOG("UClass: Property registered: %s.%s (Type: %s)",
		ClassName.ToString().data(),
		Property->GetName(),
		Property->GetTypeName());
}

/**
 * @brief 이름으로 프로퍼티 찾기
 * @param PropertyName 프로퍼티 이름 (FName)
 * @return 찾은 프로퍼티, 없으면 nullptr
 */
UPropertyBase* UClass::FindProperty(const FName& PropertyName) const
{
	// 현재 클래스의 프로퍼티 검색
	for (UPropertyBase* Prop : Properties)
	{
		if (Prop && FName(Prop->GetName()) == PropertyName)
		{
			return Prop;
		}
	}

	// 부모 클래스의 프로퍼티 검색 (상속된 프로퍼티)
	if (SuperClass)
	{
		return SuperClass->FindProperty(PropertyName);
	}

	return nullptr;
}

/**
 * @brief 이름으로 프로퍼티 찾기 (const char* 버전)
 * @param PropertyName 프로퍼티 이름 (C 문자열)
 * @return 찾은 프로퍼티, 없으면 nullptr
 */
UPropertyBase* UClass::FindProperty(const char* PropertyName) const
{
	return FindProperty(FName(PropertyName));
}

/**
 * @brief 이 클래스와 모든 부모 클래스의 프로퍼티를 재귀적으로 수집
 * @param OutProperties 수집된 프로퍼티 목록 (부모 → 자식 순서)
 */
void UClass::GetAllProperties(TArray<UPropertyBase*>& OutProperties) const
{
	// 부모 클래스의 프로퍼티를 먼저 수집 (재귀)
	if (SuperClass)
	{
		SuperClass->GetAllProperties(OutProperties);
	}

	// 현재 클래스의 프로퍼티 추가
	for (UPropertyBase* Prop : Properties)
	{
		if (Prop)
		{
			OutProperties.emplace_back(Prop);
		}
	}
}