#pragma once
#include "Editor/Public/EditorPrimitive.h"

class FAxis : public IEditorPrimitive
{
public:
	FAxis();
	~FAxis() override;
	void Render();
	TArray<const FEditorPrimitive*> GetEditorPrimitive() const override;

private:
	FEditorPrimitive Primitive;
	TArray<FNormalVertex> AxisVertices;
};
