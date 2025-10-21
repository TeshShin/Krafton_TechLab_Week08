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
    uint2   ViewportOrigin; // pixels (top-left x,y) [unused in CS, keeps CB layout in sync]
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

    // Detect orthographic vs perspective projection.
    // D3D-style: perspective has Proj[2][3] ~= 1 and Proj[3][3] ~= 0; ortho has Proj[2][3] ~= 0 and Proj[3][3] ~= 1
    bool isOrtho = (abs(Proj[2][3]) < 1e-6f) && (abs(Proj[3][3] - 1.0f) < 1e-6f);

    // Tile bounds in NDC [-1,1]
    // X: 0..NumTilesX-1 maps left(-1) -> right(+1)
    float xFracMin = (tileX)     / (float)max(NumTilesX, 1);
    float xFracMax = (tileX + 1) / (float)max(NumTilesX, 1);
    float ndcMinX = xFracMin * 2.0f - 1.0f;
    float ndcMaxX = xFracMax * 2.0f - 1.0f;

    // Y: define tileY=0 at TOP to match screen coords (y down in pixels)
    // Map to NDC with top=+1, bottom=-1
    float yFracMin = (tileY)     / (float)max(NumTilesY, 1);
    float yFracMax = (tileY + 1) / (float)max(NumTilesY, 1);
    float ndcMaxY = 1.0f - 2.0f * yFracMin; // top edge of tile
    float ndcMinY = 1.0f - 2.0f * yFracMax; // bottom edge of tile

    float2 ndcMin = float2(ndcMinX, ndcMinY);
    float2 ndcMax = float2(ndcMaxX, ndcMaxY);

    // Depth partitioning: use logarithmic for perspective, linear for orthographic
    float zNear, zFar;
    if (!isOrtho)
    {
        zNear = NearZ * pow(FarZ / NearZ, (float)zSlice / (float)max(NumZSlices, 1));
        zFar  = NearZ * pow(FarZ / NearZ, (float)(zSlice + 1) / (float)max(NumZSlices, 1));

    	// Perspective: side planes pass through origin; build from corner rays
    	float3 r00 = UnprojectToViewRay(float2(ndcMin.x, ndcMin.y)); // bottom-left
    	float3 r10 = UnprojectToViewRay(float2(ndcMax.x, ndcMin.y)); // bottom-right
    	float3 r01 = UnprojectToViewRay(float2(ndcMin.x, ndcMax.y)); // top-left
    	float3 r11 = UnprojectToViewRay(float2(ndcMax.x, ndcMax.y)); // top-right
    	float3 rCenter = normalize(r00 + r10 + r01 + r11);

    	// Build side planes; flip normals so they face inward (dot with rCenter > 0)
    	f.Sides[0].n = normalize(cross(r00, r01)); if (dot(f.Sides[0].n, rCenter) < 0) f.Sides[0].n = -f.Sides[0].n; f.Sides[0].d = 0.0f; // Left
    	f.Sides[1].n = normalize(cross(r11, r10)); if (dot(f.Sides[1].n, rCenter) < 0) f.Sides[1].n = -f.Sides[1].n; f.Sides[1].d = 0.0f; // Right
    	f.Sides[2].n = normalize(cross(r00, r10)); if (dot(f.Sides[2].n, rCenter) < 0) f.Sides[2].n = -f.Sides[2].n; f.Sides[2].d = 0.0f; // Bottom
    	f.Sides[3].n = normalize(cross(r11, r01)); if (dot(f.Sides[3].n, rCenter) < 0) f.Sides[3].n = -f.Sides[3].n; f.Sides[3].d = 0.0f; // Top
    }
    else
    {
        float t0 = (float)zSlice / (float)max(NumZSlices, 1);
        float t1 = (float)(zSlice + 1) / (float)max(NumZSlices, 1);
        zNear = lerp(NearZ, FarZ, t0);
        zFar  = lerp(NearZ, FarZ, t1);

    	// Orthographic: side planes are axis-aligned slabs in view space with non-zero offsets.
    	// Unproject NDC corners to view space (z doesn't affect x/y in ortho; pick zNDC=0)
    	float zNDC = 0.0f;
    	float4 v00 = mul(float4(ndcMin.x, ndcMin.y, zNDC, 1.0f), InvProj);
    	float4 v10 = mul(float4(ndcMax.x, ndcMin.y, zNDC, 1.0f), InvProj);
    	float4 v01 = mul(float4(ndcMin.x, ndcMax.y, zNDC, 1.0f), InvProj);
    	float4 v11 = mul(float4(ndcMax.x, ndcMax.y, zNDC, 1.0f), InvProj);
    	v00.xyz /= max(v00.w, 1e-6f);
    	v10.xyz /= max(v10.w, 1e-6f);
    	v01.xyz /= max(v01.w, 1e-6f);
    	v11.xyz /= max(v11.w, 1e-6f);

    	float leftX   = min(v00.x, v01.x);
    	float rightX  = max(v10.x, v11.x);
    	float bottomY = min(v00.y, v10.y);
    	float topY    = max(v01.y, v11.y);

    	// Planes: inside region satisfies x in [leftX,rightX], y in [bottomY,topY]
    	f.Sides[0].n = float3(+1, 0, 0); f.Sides[0].d = -leftX;   // Left:  x - leftX >= 0
    	f.Sides[1].n = float3(-1, 0, 0); f.Sides[1].d = +rightX;  // Right: -x + rightX >= 0
    	f.Sides[2].n = float3(0, +1, 0); f.Sides[2].d = -bottomY; // Bottom: y - bottomY >= 0
    	f.Sides[3].n = float3(0, -1, 0); f.Sides[3].d = +topY;    // Top:   -y + topY >= 0
    }

    // Z range (view space, camera looking +Z for D3D LH)
    f.zNear = zNear;
    f.zFar  = zFar;
    return f;
}

// Sphere vs frustum test in view space
bool IntersectsSphereFrustum(float3 centerVS, float radius, Frustum f)
{
    // Side planes (support both origin-passing and offset planes)
    for (int i = 0; i < 4; ++i)
    {
        float dist = dot(f.Sides[i].n, centerVS) + f.Sides[i].d;
        if (dist < -radius) { return false; }
    }
    // Z slabs
    if (centerVS.z + radius < f.zNear) return false;
    if (centerVS.z - radius > f.zFar)  return false;
    return true;
}

// Cone vs frustum test in view space (spotlight)
// apexVS  : cone apex in view space
// axisVS  : cone axis (normalized, pointing from apex toward cone)
// angleRad: outer cone half-angle in radians
// length  : cone length (spot range)
bool IntersectsConeFrustum(float3 apexVS, float3 axisVS, float angleRad, float length, Frustum f)
{
    axisVS = normalize(axisVS);
    length = max(length, 0.0f);

    // Degenerate: zero length -> reduce to point test against slabs and side planes
    if (length <= 1e-5f)
    {
        // Side planes
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float dist = dot(f.Sides[i].n, apexVS) + f.Sides[i].d;
            if (dist < 0.0f) return false;
        }
        // Z slabs
        if (apexVS.z < f.zNear) return false;
        if (apexVS.z > f.zFar)  return false;
        return true;
    }

    // Precompute cone parameters
    float tanTheta = tan(max(angleRad, 0.0f));
    float R = length * tanTheta; // base radius

    // Reject against the 4 side planes (planes pass through origin => d = 0)
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float3 n = f.Sides[i].n;
        float d  = f.Sides[i].d;

        float k = dot(n, axisVS);
        float s = sqrt(saturate(1.0f - k * k));

        float da = dot(n, apexVS) + d;          // apex distance
        float db = da + length * k;             // base center distance
        float maxPlane = max(da, db + R * s);   // furthest point on cone along n

        // If furthest point is still outside (negative), cone is completely outside this plane
        if (maxPlane < 0.0f)
            return false;
    }

    // Z slab rejection (axis-aligned planes zNear <= z <= zFar)
    float3 baseCenter = apexVS + axisVS * length;
    float az = axisVS.z;
    float sZ = sqrt(saturate(1.0f - az * az));

    float zApex = apexVS.z;
    float zBase = baseCenter.z;
    float zMax = max(zApex, zBase + R * sZ);
    float zMin = min(zApex, zBase - R * sZ);

    if (zMax < f.zNear) return false;
    if (zMin > f.zFar)  return false;

    return true;
}

// Unified light vs cluster test
bool IntersectsUnifiedLightFrustum(FUnifiedDynamicLight Light, Frustum Frustum)
{
    // Transform center/direction to view space
    float3 CenterVS = mul(float4(Light.Position, 1.0f), View).xyz;

    if (Light.LightType == LIGHT_TYPE_DIRECTIONAL || Light.LightType == LIGHT_TYPE_AMBIENT)
    {
        return true;
    }
	if (Light.LightType == LIGHT_TYPE_SPOT)
	{
		// Transform direction to view space (w = 0 for direction vectors)
		float3 AxisVS = normalize(mul(float4(Light.Direction, 0.0f), View).xyz);
		float  Range  = max(Light.SourceRadius, 0.0f);
		float  Angle  = radians(Light.Param1); // use outer cone for culling
		return IntersectsConeFrustum(CenterVS, AxisVS, Angle, Range, Frustum);
	}

    // Use sphere approximation for Point/Rect
    float Radius = max(Light.SourceRadius, 0.0f);
    return IntersectsSphereFrustum(CenterVS, Radius, Frustum);
}

// 1 thread per cluster (tileX,tileY,zSlice)
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint3 gid = dispatchThreadID;
	uint cid = (gid.z * NumTilesY + gid.y) * NumTilesX + gid.x;
	ClusterCount[cid] = 0;

    uint tileX = dispatchThreadID.x;
    uint tileY = dispatchThreadID.y;
    uint zSlice = dispatchThreadID.z;

    if (tileX >= NumTilesX || tileY >= NumTilesY || zSlice >= NumZSlices)
        return;

    uint clusterID = (zSlice * NumTilesY + tileY) * NumTilesX + tileX;
    if (clusterID >= TotalClusters) return;

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
