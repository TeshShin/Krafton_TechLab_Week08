// ================================================================
// LightTilesComputeShader.hlsl
// - Forward+ (tiled/clustered forward) light culling compute shader
// - Inputs : camera params + unified dynamic lights buffer (world space)
// - Outputs: per-cluster light counts and indices
// ================================================================

// Camera and tiling parameters
cbuffer CameraCB : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Proj;
    row_major float4x4 InvProj;
    uint2   ScreenSize;     // pixels (width, height)
    uint    NumTilesX;      // dispatch dim X
    uint    NumTilesY;      // dispatch dim Y
    uint    NumZSlices;     // dispatch dim Z
    float   NearZ;          // view-space near (>= 0)
    float   FarZ;           // view-space far  (>  NearZ)
};

// Forward+ control parameters
cbuffer ForwardPlusCB : register(b1)
{
    uint NumLights;                // number of entries in DynamicLights
    uint MaxLightsPerCluster;      // capacity per cluster
    uint TotalClusters;            // NumTilesX*NumTilesY*NumZSlices
    uint FP_Pad0;
};

// Light type enumeration (must match pixel shader and LightingFunctions.hlsl)
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1
#define LIGHT_TYPE_SPOT        2
#define LIGHT_TYPE_AMBIENT     3

// Must match TexturePS.hlsl FUnifiedDynamicLight exactly
struct FUnifiedDynamicLight
{
    float3 Position;            // world space
    float  Intensity;
    float3 Color;
    float  SourceRadius;        // influence radius / size
    float3 Direction;           // world space
    float  FalloffExponent;
    float  Param0;              // spot inner (rad) / rect width
    float  Param1;              // spot outer (rad) / rect height
    float  Param2;              // reserved
    uint   LightType;           // enum above
    float4 Padding;             // alignment
};

// Inputs/Outputs
StructuredBuffer<FUnifiedDynamicLight> DynamicLights : register(t0);
RWStructuredBuffer<uint> ClusterCount  : register(u0); // size = TotalClusters
RWStructuredBuffer<uint> ClusterIndex  : register(u1); // size = TotalClusters * MaxLightsPerCluster

// Geometry helpers -------------------------------------------------
struct Plane { float3 n; float d; };
struct Frustum
{
    Plane Sides[4];
    float zNear;
    float zFar;
};

// Unproject an NDC corner (x,y in [-1,1]) at the far plane to a view-space ray
float3 UnprojectToViewRay(float2 ndc)
{
    float4 pClip = float4(ndc, 1.0f, 1.0f);
    // Row-major math (vector on left)
    float4 pView = mul(pClip, InvProj);
    float3 ray = pView.xyz / max(pView.w, 1e-6f);
    return normalize(ray);
}

// Build cluster frustum planes from tile extents and z slice
Frustum BuildClusterFrustum(uint tileX, uint tileY, uint zSlice)
{
    Frustum f;

    // Tile bounds in NDC [-1,1]
    float2 ndcMin = float2((tileX    / (float)NumTilesX) * 2.0f - 1.0f,
                           (tileY    / (float)NumTilesY) * 2.0f - 1.0f);
    float2 ndcMax = float2(((tileX+1)/ (float)NumTilesX) * 2.0f - 1.0f,
                           ((tileY+1)/ (float)NumTilesY) * 2.0f - 1.0f);

    // Logarithmic partitioning in view-space depth (optional but common)
    float zNear = NearZ * pow(FarZ / NearZ, (float)zSlice / (float)max(NumZSlices, 1));
    float zFar  = NearZ * pow(FarZ / NearZ, (float)(zSlice + 1) / (float)max(NumZSlices, 1));

    // Corner rays in view space
    float3 r00 = UnprojectToViewRay(float2(ndcMin.x, ndcMin.y)); // bottom-left
    float3 r10 = UnprojectToViewRay(float2(ndcMax.x, ndcMin.y)); // bottom-right
    float3 r01 = UnprojectToViewRay(float2(ndcMin.x, ndcMax.y)); // top-left
    float3 r11 = UnprojectToViewRay(float2(ndcMax.x, ndcMax.y)); // top-right

    // Side planes passing through the origin; choose winding so normals point inward
    // For left edge (between r01 and r00), inward normal = normalize(cross(r00, r01))
    f.Sides[0].n = normalize(cross(r00, r01)); f.Sides[0].d = 0.0f; // Left
    // Right edge (between r10 and r11)
    f.Sides[1].n = normalize(cross(r11, r10)); f.Sides[1].d = 0.0f; // Right
    // Bottom edge (between r10 and r00)
    f.Sides[2].n = normalize(cross(r00, r10)); f.Sides[2].d = 0.0f; // Bottom
    // Top edge (between r01 and r11)
    f.Sides[3].n = normalize(cross(r11, r01)); f.Sides[3].d = 0.0f; // Top

    // Z range (view space, camera at origin looking +Z for D3D LH)
    f.zNear = zNear;
    f.zFar  = zFar;
    return f;
}

// Sphere vs frustum test in view space
bool IntersectsSphereFrustum(float3 centerVS, float radius, Frustum f)
{
    // Side planes (planes pass through origin => d = 0)
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float dist = dot(f.Sides[i].n, centerVS) + f.Sides[i].d; // d==0
        if (dist < -radius) { return false; }
    }
    // Z slabs
    if (centerVS.z + radius < f.zNear) return false;
    if (centerVS.z - radius > f.zFar)  return false;
    return true;
}

// Unified light vs cluster test
bool IntersectsUnifiedLightFrustum(FUnifiedDynamicLight L, Frustum f)
{
    // Transform center/direction to view space
    float3 centerVS = mul(float4(L.Position, 1.0f), View).xyz;
    float3 dirVS    = normalize(mul(float4(L.Direction, 0.0f), View).xyz);

    if (L.LightType == LIGHT_TYPE_DIRECTIONAL || L.LightType == LIGHT_TYPE_AMBIENT)
    {
        // Directional lights affect all clusters (optionally handle separately)
        return true;
    }

    // Use sphere approximation for Point/Spot/Rect
    float radius = max(L.SourceRadius, 0.0f);
    return IntersectsSphereFrustum(centerVS, radius, f);
}

// 1 thread per cluster (tileX,tileY,zSlice)
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint3 gid = dispatchThreadID;
	uint cid = (gid.z * NumTilesY + gid.y) * NumTilesX + gid.x;
	ClusterCount[cid] = 0;
	GroupMemoryBarrierWithGroupSync();

    uint tileX = dispatchThreadID.x;
    uint tileY = dispatchThreadID.y;
    uint zSlice = dispatchThreadID.z;

    if (tileX >= NumTilesX || tileY >= NumTilesY || zSlice >= NumZSlices)
        return;

    uint clusterID = (zSlice * NumTilesY + tileY) * NumTilesX + tileX;
    if (clusterID >= TotalClusters) return;

    // Reset count (safe: exactly one thread writes per cluster)
    ClusterCount[clusterID] = 0u;

    Frustum clusterFrustum = BuildClusterFrustum(tileX, tileY, zSlice);
    uint base = clusterID * MaxLightsPerCluster;

    for (uint i = 0; i < NumLights; ++i)
    {
        if (IntersectsUnifiedLightFrustum(DynamicLights[i], clusterFrustum))
        {
        	uint oldIndex;
        	InterlockedAdd(ClusterCount[clusterID], 1u, oldIndex);
        	uint idx = oldIndex;
            if (idx < MaxLightsPerCluster)
            {
                ClusterIndex[base + idx] = i;
            }
        }
    }
}
