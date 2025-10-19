#pragma once

#include "LineVertex.h"

class ILineSource
{
public:
    virtual ~ILineSource() = default;

    virtual const TArray<FLineVertex>& GetVertices() const = 0;
    virtual const TArray<uint32>& GetIndices() const = 0;
    virtual bool IsDirty() const = 0;
    virtual void ClearDirtyFlag() const = 0;
	virtual bool IsRenderInPIE() = 0;
};
