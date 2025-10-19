#pragma once

class ILineSource
{
public:
    virtual ~ILineSource() = default;

    virtual const TArray<FVector>& GetVertices() const = 0;
    virtual const TArray<uint32>& GetIndices() const = 0;
    virtual bool IsDirty() const = 0;
    virtual void ClearDirtyFlag() const = 0;
};
