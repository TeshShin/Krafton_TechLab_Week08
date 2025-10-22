#pragma once
#include "Scene/Public/Component/ActorComponent.h"
#include "Core/Public/Object/Property.h"

namespace json { class JSON; }
using JSON = json::JSON;

UCLASS()
class USceneComponent : public UActorComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USceneComponent, UActorComponent)

public:
	USceneComponent();

	void BeginPlay() override;
	    void TickComponent(float DeltaTime) override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	virtual void MarkAsDirty();

	void SetRelativeLocation(const FVector& Location);
	void SetRelativeRotation(const FQuaternion& Rotation);
	void SetRelativeScale3D(const FVector& Scale);
	void SetUniformScale(bool bIsUniform);

	bool IsUniformScale() const;

	const FVector& GetRelativeLocation() const { return RelativeLocation; }
	const FQuaternion& GetRelativeRotation() const { return RelativeRotation; }
	const FVector& GetRelativeScale3D() const { return RelativeScale3D; }

	// Visibility
	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

	/**
	 * @brief 계층적 visibility를 체크합니다.
	 * 컴포넌트가 렌더링되려면: 소유 Actor + 모든 상위 컴포넌트 + 자신이 모두 visible이어야 합니다.
	 * @return 계층 구조에서 이 컴포넌트가 visible인지 여부
	 */
	bool IsVisibleInHierarchy() const;

	const FMatrix& GetWorldTransformMatrix() const;
	const FMatrix& GetWorldTransformMatrixInverse() const;

	FVector GetWorldLocation() const;
    FVector GetWorldRotation() const;
    FQuaternion GetWorldRotationAsQuaternion() const;
    FVector GetWorldScale3D() const;

    void SetWorldLocation(const FVector& NewLocation);
    void SetWorldRotation(const FVector& NewRotation);
    void SetWorldRotation(const FQuaternion& NewRotation);
    void SetWorldScale3D(const FVector& NewScale);

	FVector GetWorldForwardVector() const;
	FVector GetWorldRightVector() const;
	FVector GetWorldUpVector() const;
protected:
	UPROPERTY_INIT_WITHMETA(bool, bVisible, true, FPropertyMetadata({
		.Flags = EPropertyFlags::SaveGame,
		.DisplayName = "Visible"
	}))

private:
	mutable bool bIsTransformDirty = true;
	mutable bool bIsTransformDirtyInverse = true;
	mutable FMatrix WorldTransformMatrix;
	mutable FMatrix WorldTransformMatrixInverse;

	FVector RelativeLocation = FVector{ 0,0,0.f };
	FQuaternion RelativeRotation = FQuaternion::Identity();
	FVector RelativeScale3D = FVector{ 0.3f,0.3f,0.3f };
	bool bIsUniformScale = false;

	// SceneComponent Hierarchy Section
public:
	USceneComponent* GetAttachParent() const { return AttachParent; }
	void AttachToComponent(USceneComponent* Parent, bool bRemainTransform = false);
	void DetachFromComponent();
	bool IsAttachedTo(const USceneComponent* Parent) const { return AttachParent == Parent; }
	const TArray<USceneComponent*>& GetChildren() const { return AttachChildren; }

protected:
	void DetachChild(USceneComponent* ChildToDetach);

private:
	USceneComponent* AttachParent = nullptr;
	TArray<USceneComponent*> AttachChildren;

public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;
};
