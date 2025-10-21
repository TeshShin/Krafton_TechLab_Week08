#pragma once
#include "Core/Public/Container/Name.h"
#include "Core/Public/Types.h"
#include "Core/Public/Math/Vector.h"
#include <typeinfo>
#include <limits>

class UClass;

/**
 * @brief 프로퍼티 타입을 나타내는 열거형
 */
enum class EPropertyType : uint8
{
	Unknown,
	Int8,
	Int16,
	Int32,
	Int64,
	UInt8,
	UInt16,
	UInt32,
	UInt64,
	Float,
	Double,
	Bool,
	String,
	Name,
	Object,
	Struct,
	Enum,
	Array,
	Vector2,
	Vector,
	Vector4,
	LinearColor3,
	LinearColor,
	Custom
};

/**
 * @brief 프로퍼티 플래그 (Unreal Engine 스타일)
 * 비트마스크 형식으로 프로퍼티의 특성을 정의합니다.
 */
enum class EPropertyFlags : uint64
{
	None = 0,

	// 에디터 편집 권한
	EditAnywhere = 1ULL << 0,           // 에디터에서 어디서나 편집 가능
	VisibleAnywhere = 1ULL << 1,        // 에디터에서 어디서나 표시 (읽기 전용)

	// 직렬화 및 저장
	SaveGame = 1ULL << 2,				// 저장 파일에 포함
	DuplicateTransient = 1ULL << 3		// 모든 유형의 복제에 대해 default value로 리셋
};

/**
 * @brief 프로퍼티 메타데이터 구조체
 * Designated initializer를 사용하여 다양한 메타정보를 저장합니다.
 *
 * 주의: C++20 designated initializer 사용을 위해 생성자가 없는 aggregate type입니다.
 */
struct FPropertyMetadata
{
	EPropertyFlags Flags = EPropertyFlags::None;

	// 숫자형 제약
	double Min = -std::numeric_limits<double>::infinity();
	double Max = std::numeric_limits<double>::infinity();
	double Step = 1.0;

	// UI 정보
	const char* Category = nullptr;      // 카테고리 이름
	const char* Tooltip = nullptr;       // 툴팁 텍스트
	const char* DisplayName = nullptr;   // 표시 이름 (기본값: 프로퍼티 이름)

	// 추가 옵션
	bool bSlider = false;                // 슬라이더 UI 사용 여부
	bool bReadOnly = false;              // 강제 읽기 전용
};

// 비트마스크 연산자 오버로딩
inline EPropertyFlags operator|(EPropertyFlags a, EPropertyFlags b)
{
	return static_cast<EPropertyFlags>(static_cast<uint64>(a) | static_cast<uint64>(b));
}

inline EPropertyFlags operator&(EPropertyFlags a, EPropertyFlags b)
{
	return static_cast<EPropertyFlags>(static_cast<uint64>(a) & static_cast<uint64>(b));
}

inline EPropertyFlags& operator|=(EPropertyFlags& a, EPropertyFlags b)
{
	a = a | b;
	return a;
}

inline bool HasFlag(EPropertyFlags flags, EPropertyFlags flag)
{
	return (static_cast<uint64>(flags) & static_cast<uint64>(flag)) != 0;
}

/**
 * @brief 타입에 독립적인 프로퍼티 메타데이터를 저장하는 기본 클래스
 *
 * UPropertyBase는 프로퍼티의 이름, 타입, 오프셋 등의 메타데이터를 저장합니다.
 * 실제 값 접근은 템플릿 파생 클래스인 UProperty<OwnerClass, T>를 통해 제공됩니다.
 */
class UPropertyBase
{
public:
	UPropertyBase(UClass* InOwnerClass, const char* InName, EPropertyType InType, size_t InOffset, size_t InSize, const FPropertyMetadata& InMetadata = FPropertyMetadata())
		: OwnerClassInfo(InOwnerClass)
		, PropertyName(InName)
		, PropertyType(InType)
		, Offset(InOffset)
		, Size(InSize)
		, Metadata(InMetadata)
	{
	}

	virtual ~UPropertyBase() = default;

	// Getter 함수들
	UClass* GetOwnerClass() const { return OwnerClassInfo; }
	const char* GetName() const { return PropertyName; }
	const char* GetDisplayName() const { return Metadata.DisplayName ? Metadata.DisplayName : PropertyName; }
	const char* GetTooltip() const { return Metadata.Tooltip; }
	const char* GetCategory() const { return Metadata.Category; }
	EPropertyType GetPropertyType() const { return PropertyType; }
	size_t GetOffset() const { return Offset; }
	size_t GetSize() const { return Size; }
	EPropertyFlags GetFlags() const { return Metadata.Flags; }
	const FPropertyMetadata& GetMetadata() const { return Metadata; }

	// 숫자형 제약
	double GetMin() const { return Metadata.Min; }
	double GetMax() const { return Metadata.Max; }
	double GetStep() const { return Metadata.Step; }
	bool UseSlider() const { return Metadata.bSlider; }

	// 플래그 확인 함수
	bool HasAnyFlags(EPropertyFlags FlagsToCheck) const
	{
		return (static_cast<uint64>(Metadata.Flags) & static_cast<uint64>(FlagsToCheck)) != 0;
	}

	bool IsReadOnly() const
	{
		return Metadata.bReadOnly || HasAnyFlags(EPropertyFlags::VisibleAnywhere);
	}

	bool IsEditable() const
	{
		return !IsReadOnly() && HasAnyFlags(EPropertyFlags::EditAnywhere);
	}

	// 타입 정보 문자열 반환 (디버깅용)
	virtual const char* GetTypeName() const = 0;

	// 프로퍼티 값을 문자열로 변환 (디버깅/직렬화용)
	virtual FString ToString(const void* ContainerPtr) const = 0;

	// 프로퍼티 값을 문자열로부터 설정 (역직렬화용)
	virtual bool FromString(void* ContainerPtr, const FString& Value) const = 0;

protected:
	UClass* OwnerClassInfo;      // 이 프로퍼티를 소유한 클래스
	const char* PropertyName;    // 프로퍼티 이름
	EPropertyType PropertyType;  // 프로퍼티 타입
	size_t Offset;               // 클래스 내에서의 오프셋 (offsetof)
	size_t Size;                 // 프로퍼티 크기 (sizeof)
	FPropertyMetadata Metadata;  // 프로퍼티 메타데이터
};

/**
 * @brief 타입 안전한 프로퍼티 접근을 제공하는 템플릿 클래스
 *
 * @tparam OwnerClass 이 프로퍼티를 소유한 클래스 타입
 * @tparam T 프로퍼티의 실제 타입
 *
 * 사용 예:
 * class MyClass : public UObject
 * {
 *     DECLARE_CLASS(MyClass, UObject)
 * public:
 *     UPROPERTY(int32, Health);
 *     UPROPERTY(float, Speed);
 * };
 */
template<typename OwnerClass, typename T>
class UProperty : public UPropertyBase
{
public:
	UProperty(UClass* InOwnerClass, const char* InName, size_t InOffset, const FPropertyMetadata& InMetadata = FPropertyMetadata())
		: UPropertyBase(InOwnerClass, InName, DeducePropertyType(), InOffset, sizeof(T), InMetadata)
	{
		// 자동 등록 로직 (리플렉션 테이블에 등록)
		if (InOwnerClass)
		{
			RegisterProperty(InOwnerClass, this);
		}
	}

	// 타입 이름 반환
	virtual const char* GetTypeName() const override
	{
		return typeid(T).name();
	}

	// 컨테이너 객체로부터 프로퍼티 값 가져오기
	T* GetValuePtr(void* ContainerPtr) const
	{
		if (!ContainerPtr)
			return nullptr;

		return reinterpret_cast<T*>(static_cast<uint8*>(ContainerPtr) + Offset);
	}

	const T* GetValuePtr(const void* ContainerPtr) const
	{
		if (!ContainerPtr)
			return nullptr;

		return reinterpret_cast<const T*>(static_cast<const uint8*>(ContainerPtr) + Offset);
	}

	// 프로퍼티 값을 문자열로 변환
	virtual FString ToString(const void* ContainerPtr) const override
	{
		const T* ValuePtr = GetValuePtr(ContainerPtr);
		if (!ValuePtr)
			return FString("nullptr");

		return PropertyToString(*ValuePtr);
	}

	// 문자열로부터 프로퍼티 값 설정
	virtual bool FromString(void* ContainerPtr, const FString& Value) const override
	{
		T* ValuePtr = GetValuePtr(ContainerPtr);
		if (!ValuePtr)
			return false;

		return PropertyFromString(*ValuePtr, Value);
	}

private:
	// 타입으로부터 EPropertyType 추론
	static constexpr EPropertyType DeducePropertyType()
	{
		if constexpr (std::is_same_v<T, int8>)
			return EPropertyType::Int8;
		else if constexpr (std::is_same_v<T, int16>)
			return EPropertyType::Int16;
		else if constexpr (std::is_same_v<T, int32>)
			return EPropertyType::Int32;
		else if constexpr (std::is_same_v<T, int64>)
			return EPropertyType::Int64;
		else if constexpr (std::is_same_v<T, uint8>)
			return EPropertyType::UInt8;
		else if constexpr (std::is_same_v<T, uint16>)
			return EPropertyType::UInt16;
		else if constexpr (std::is_same_v<T, uint32>)
			return EPropertyType::UInt32;
		else if constexpr (std::is_same_v<T, uint64>)
			return EPropertyType::UInt64;
		else if constexpr (std::is_same_v<T, float>)
			return EPropertyType::Float;
		else if constexpr (std::is_same_v<T, double>)
			return EPropertyType::Double;
		else if constexpr (std::is_same_v<T, bool>)
			return EPropertyType::Bool;
		else if constexpr (std::is_same_v<T, FString>)
			return EPropertyType::String;
		else if constexpr (std::is_same_v<T, FName>)
			return EPropertyType::Name;
		else if constexpr (std::is_same_v<T, FVector2>)
			return EPropertyType::Vector2;
		else if constexpr (std::is_same_v<T, FVector>)
			return EPropertyType::Vector;
		else if constexpr (std::is_same_v<T, FVector4>)
			return EPropertyType::Vector4;
		else if constexpr (std::is_same_v<T, FLinearColor3>)
			return EPropertyType::LinearColor3;
		else if constexpr (std::is_same_v<T, FLinearColor>)
			return EPropertyType::LinearColor;
		else if constexpr (std::is_pointer_v<T> && std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
			return EPropertyType::Object;
		else
			return EPropertyType::Custom;
	}

	// 프로퍼티를 UClass에 등록
	static void RegisterProperty(UClass* InClass, UPropertyBase* Property);

	// 값을 문자열로 변환 (타입별 특수화)
	template<typename U = T>
	static FString PropertyToString(const U& Value)
	{
		if constexpr (std::is_arithmetic_v<U>)
		{
			return FString(std::to_string(Value).c_str());
		}
		else if constexpr (std::is_same_v<U, FString>)
		{
			return Value;
		}
		else if constexpr (std::is_same_v<U, FName>)
		{
			return Value.ToString();
		}
		else if constexpr (std::is_same_v<U, bool>)
		{
			return Value ? FString("true") : FString("false");
		}
		else if constexpr (std::is_same_v<U, FVector2>)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "[%.6f, %.6f]", Value.X, Value.Y);
			return FString(buffer);
		}
		else if constexpr (std::is_same_v<U, FVector>)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f]", Value.X, Value.Y, Value.Z);
			return FString(buffer);
		}
		else if constexpr (std::is_same_v<U, FVector4>)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f, %.6f]", Value.X, Value.Y, Value.Z, Value.W);
			return FString(buffer);
		}
		else if constexpr (std::is_same_v<U, FLinearColor3>)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f]", Value.R, Value.G, Value.B);
			return FString(buffer);
		}
		else if constexpr (std::is_same_v<U, FLinearColor>)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f, %.6f]", Value.R, Value.G, Value.B, Value.A);
			return FString(buffer);
		}
		else
		{
			return FString("[Custom Type]");
		}
	}

	// 문자열로부터 값 설정 (타입별 특수화)
	template<typename U = T>
	static bool PropertyFromString(U& OutValue, const FString& Value)
	{
		if constexpr (std::is_same_v<U, int32>)
		{
			OutValue = std::stoi(Value);
			return true;
		}
		else if constexpr (std::is_same_v<U, float>)
		{
			OutValue = std::stof(Value);
			return true;
		}
		else if constexpr (std::is_same_v<U, double>)
		{
			OutValue = std::stod(Value);
			return true;
		}
		else if constexpr (std::is_same_v<U, bool>)
		{
			OutValue = (Value == "true" || Value == "1");
			return true;
		}
		else if constexpr (std::is_same_v<U, FString>)
		{
			OutValue = Value;
			return true;
		}
		else if constexpr (std::is_same_v<U, FName>)
		{
			OutValue = FName(Value);
			return true;
		}
		else if constexpr (std::is_same_v<U, FVector2>)
		{
			float x, y;
			if (sscanf_s(Value.c_str(), "[%f, %f]", &x, &y) == 2)
			{
				OutValue = FVector2(x, y);
				return true;
			}
			return false;
		}
		else if constexpr (std::is_same_v<U, FVector>)
		{
			float x, y, z;
			if (sscanf_s(Value.c_str(), "[%f, %f, %f]", &x, &y, &z) == 3)
			{
				OutValue = FVector(x, y, z);
				return true;
			}
			return false;
		}
		else if constexpr (std::is_same_v<U, FVector4>)
		{
			float x, y, z, w;
			if (sscanf_s(Value.c_str(), "[%f, %f, %f, %f]", &x, &y, &z, &w) == 4)
			{
				OutValue = FVector4(x, y, z, w);
				return true;
			}
			return false;
		}
		else if constexpr (std::is_same_v<U, FLinearColor3>)
		{
			float r, g, b;
			if (sscanf_s(Value.c_str(), "[%f, %f, %f]", &r, &g, &b) == 3)
			{
				OutValue = FLinearColor3(r, g, b);
				return true;
			}
			return false;
		}
		else if constexpr (std::is_same_v<U, FLinearColor>)
		{
			float r, g, b, a;
			if (sscanf_s(Value.c_str(), "[%f, %f, %f, %f]", &r, &g, &b, &a) == 4)
			{
				OutValue = FLinearColor(r, g, b, a);
				return true;
			}
			return false;
		}
		else
		{
			return false;
		}
	}
};

/**
 * @brief UPROPERTY 매크로 시스템
 *
 * 클래스 내에서 프로퍼티를 선언할 때 사용합니다.
 * 자동으로 리플렉션 시스템에 등록되며, 런타임에 프로퍼티 정보를 조회할 수 있습니다.
 *
 * 4가지 변형:
 * 1. UPROPERTY(Type, Name)
 *    - 기본값 없음, 메타데이터 없음
 *    - 예: UPROPERTY(int32, Health);
 *
 * 2. UPROPERTY_INIT(Type, Name, DefaultValue)
 *    - 기본값 있음, 메타데이터 없음
 *    - 예: UPROPERTY_INIT(float, Speed, 5.5f);
 *
 * 3. UPROPERTY_WITHMETA(Type, Name, Meta)
 *    - 기본값 없음, 메타데이터 있음
 *    - 예: UPROPERTY_WITHMETA(int32, Health, FLAGS(EPropertyFlags::EditAnywhere))
 *    - 예: UPROPERTY_WITHMETA(float, Speed, FPropertyMetadata({.Flags = EPropertyFlags::EditAnywhere, .Min = 0.0, .Max = 100.0}))
 *
 * 4. UPROPERTY_INIT_WITHMETA(Type, Name, DefaultValue, Meta)
 *    - 기본값 있음, 메타데이터 있음
 *    - 예: UPROPERTY_INIT_WITHMETA(float, Speed, 5.5f, FLAGS(EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame))
 *    - 예: UPROPERTY_INIT_WITHMETA(float, Speed, 5.5f, FPropertyMetadata({.Flags = EPropertyFlags::EditAnywhere, .Min = 0.0, .Max = 100.0, .Tooltip = "Movement speed"}))
 *
 * 헬퍼 매크로:
 * - FLAGS(x): 플래그만 지정하는 간편 매크로
 *   예: FLAGS(EPropertyFlags::EditAnywhere)
 */

// 플래그만 지정하는 간편 매크로
#define UPROPERTY_FLAGS(x) FPropertyMetadata({.Flags = x})

// 1. 기본값 없음, 메타데이터 없음
#define UPROPERTY(Type, Name) \
	Type Name; \
	static UProperty<ThisClass, Type>* __GetPropertyMeta_##Name() \
	{ \
		static UProperty<ThisClass, Type> PropertyMeta( \
			ThisClass::StaticClass(), \
			#Name, \
			offsetof(ThisClass, Name), \
			FPropertyMetadata() \
		); \
		return &PropertyMeta; \
	} \
	static inline bool __PropertyRegistered_##Name = []() { \
		__GetPropertyMeta_##Name(); \
		return true; \
	}();

// 2. 기본값 있음, 메타데이터 없음
#define UPROPERTY_INIT(Type, Name, DefaultValue) \
	Type Name = DefaultValue; \
	static UProperty<ThisClass, Type>* __GetPropertyMeta_##Name() \
	{ \
		static UProperty<ThisClass, Type> PropertyMeta( \
			ThisClass::StaticClass(), \
			#Name, \
			offsetof(ThisClass, Name), \
			FPropertyMetadata() \
		); \
		return &PropertyMeta; \
	} \
	static inline bool __PropertyRegistered_##Name = []() { \
		__GetPropertyMeta_##Name(); \
		return true; \
	}();

// 3. 기본값 없음, 메타데이터 있음
#define UPROPERTY_WITHMETA(Type, Name, Meta) \
	Type Name; \
	static UProperty<ThisClass, Type>* __GetPropertyMeta_##Name() \
	{ \
		static UProperty<ThisClass, Type> PropertyMeta( \
			ThisClass::StaticClass(), \
			#Name, \
			offsetof(ThisClass, Name), \
			Meta \
		); \
		return &PropertyMeta; \
	} \
	static inline bool __PropertyRegistered_##Name = []() { \
		__GetPropertyMeta_##Name(); \
		return true; \
	}();

// 4. 기본값 있음, 메타데이터 있음
#define UPROPERTY_INIT_WITHMETA(Type, Name, DefaultValue, Meta) \
	Type Name = DefaultValue; \
	static UProperty<ThisClass, Type>* __GetPropertyMeta_##Name() \
	{ \
		static UProperty<ThisClass, Type> PropertyMeta( \
			ThisClass::StaticClass(), \
			#Name, \
			offsetof(ThisClass, Name), \
			Meta \
		); \
		return &PropertyMeta; \
	} \
	static inline bool __PropertyRegistered_##Name = []() { \
		__GetPropertyMeta_##Name(); \
		return true; \
	}();

// 템플릿 구현부를 여기에 포함 (헤더 파일이므로)
#include "Class.h"

template<typename OwnerClass, typename T>
void UProperty<OwnerClass, T>::RegisterProperty(UClass* InClass, UPropertyBase* Property)
{
	// UClass::AddProperty 호출 (Class.h에서 선언됨)
	InClass->AddProperty(Property);
}
