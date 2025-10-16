#pragma once
#include "Core/Public/Object/Object.h"
#include "Editor/Public/EditorPrimitive.h"

class UAxis : public UObject
{
public:
	UAxis();
	~UAxis() override;
	void Render();

private:
	FEditorPrimitive Primitive;
	TArray<FNormalVertex> AxisVertices;
};
