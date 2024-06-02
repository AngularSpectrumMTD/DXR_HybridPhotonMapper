#include "sceneCBDefinition.hlsli"
ConstantBuffer<SceneCB> gSceneParam : register(b0);
#include "sceneParamInterface.hlsli"

#include "spectralRenderingHelper.hlsli"
#include "reservoir.hlsli"

#define THREAD_NUM 16
#define REINHARD_L 1000
#define MAX_ACCUMULATION_RANGE 1000

StructuredBuffer<DIReservoir> DIReservoirBufferSrc : register(t0);
Texture2D<float4> NormalDepthBuffer : register(t1);
Texture2D<float4> PrevNormalDepthBuffer : register(t2);
Texture2D<float2> VelocityBuffer : register(t3);
Texture2D<float4> IDRoughnessBuffer : register(t4);
Texture2D<float4> PrevIDRoughnessBuffer : register(t5);
Texture2D<float4> PositionBuffer : register(t6);
Texture2D<float4> PrevPositionBuffer : register(t7);
RWStructuredBuffer<DIReservoir> DIReservoirBufferDst : register(u0);

static uint rseed;

//restrict
bool isWithinBounds(int2 id, int2 size)
{
    return ((0 <= id.x) && (id.x <= (size.x - 1))) && ((0 <= id.y) && (id.y <= (size.y - 1)));
}

float rand(in int2 indexXY)//0-1
{
    rseed += 1.0;
    return frac(sin(dot(indexXY.xy, float2(12.9898, 78.233)) * (getLightRandomSeed() + 1) * 0.001 + rseed) * 43758.5453);
}

float computeLuminance(const float3 linearRGB)
{
    return dot(float3(0.2126, 0.7152, 0.0722), linearRGB);
}

float reinhard(float x, float L)
{
    return (x / (1 + x)) * (1 + x / L / L);
}

float3 reinhard3f(float3 v, float L)
{
    return float3(reinhard(v.x, L), reinhard(v.y, L), reinhard(v.z, L));
}

float ACESFilmicTonemapping(float x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate(x * (a * x + b) / (x * (c * x + d) + c));
}

float3 ACESFilmicTonemapping3f(float3 v)
{
    return float3(ACESFilmicTonemapping(v.x), ACESFilmicTonemapping(v.y), ACESFilmicTonemapping(v.z));
}

void DIReservoirTemporalReuse(inout DIReservoir currDIReservoir, in DIReservoir prevDIReservoir, in uint2 currID)
{
    //Limitting
    if(prevDIReservoir.M > MAX_TEMPORAL_REUSE_M)
    {
        float r = max(0, ((float)MAX_TEMPORAL_REUSE_M / prevDIReservoir.M));
        prevDIReservoir.W_sum *= r;
        prevDIReservoir.M = MAX_TEMPORAL_REUSE_M;
    }

    DIReservoir tempDIReservoir;
    tempDIReservoir.initialize();
    //combine reservoirs
    {
        const float currUpdateW = currDIReservoir.W_sum;
        combineDIReservoirs(tempDIReservoir, currDIReservoir, currUpdateW, rand(currID));
        const float prevUpdateW = prevDIReservoir.W_sum;// * (prevDIReservoir.targetPDF / currDIReservoir.targetPDF);
        combineDIReservoirs(tempDIReservoir, prevDIReservoir, prevUpdateW, rand(currID));
    }
    currDIReservoir = tempDIReservoir;
}

[numthreads(THREAD_NUM, THREAD_NUM, 1)]
void temporalReuse(uint3 dtid : SV_DispatchThreadID)
{
    rseed = getLightRandomSeed();
    float2 dims;
    NormalDepthBuffer.GetDimensions(dims.x, dims.y);

    uint2 currID = dtid.xy;

    float currDepth = NormalDepthBuffer[currID].w;
    float3 currNormal = NormalDepthBuffer[currID].xyz;
    uint currInstanceIndex = IDRoughnessBuffer[currID].y;
    float currRoughness = IDRoughnessBuffer[currID].z;
    float currAlbedoLuminance = IDRoughnessBuffer[currID].w;

    float2 velocity = VelocityBuffer[currID];//-1 to 1
    float2 currUV = currID / dims;
    float2 prevUV = currUV + velocity;
    int2 prevID = prevUV * dims;

    float velocityL = sqrt(dot(velocity, velocity));
    const bool isSmallVelocity = (velocityL < 0.5);

    float3 currPos = PositionBuffer[currID].xyz;

    float3 currDI = 0.xxx;
    if(isUseNEE() && isUseWRS_RIS())
    {
        const uint serialCurrID = currID.y * dims.x + currID.x;
        const uint serialPrevID = prevID.y * dims.x + prevID.x;
        DIReservoir currDIReservoir = DIReservoirBufferDst[serialCurrID];

        if (isUseReservoirTemporalReuse() && isWithinBounds(prevID, dims) && isSmallVelocity && !isHistoryResetRequested())
        {
            float prevDepth = PrevNormalDepthBuffer[prevID].w;
            float3 prevNormal = PrevNormalDepthBuffer[prevID].xyz;
            uint prevInstanceIndex = PrevIDRoughnessBuffer[prevID].y;
            float prevRoughness = PrevIDRoughnessBuffer[prevID].z;
            float prevAlbedoLuminance = PrevIDRoughnessBuffer[prevID].w;
            float3 prevPos = PrevPositionBuffer[prevID].xyz;

            const bool isNearDepth = ((currDepth * 0.7 < prevDepth) && (prevDepth < currDepth * 1.3)) && (currDepth > 0) && (prevDepth > 0);
            const bool isNearNormal = dot(currNormal, prevNormal) > 0.9;
            const bool isSameInstance = (currInstanceIndex == prevInstanceIndex);
            const bool isNearRoughness = (abs(currRoughness - prevRoughness) < 0.05);
            const bool isNearAlbedoLuminance = (abs(currAlbedoLuminance - prevAlbedoLuminance) < 0.05);
            const bool isNearPosition = (sqrt(dot(currPos - prevPos, currPos - prevPos)) < 0.3f);//30cm

            //const bool isNearDepth = (abs(currDepth - prevDepth) < 0.01f) && (currDepth > 0) && (prevDepth > 0);
            const bool isTemporalReuseEnable = isNearPosition && isNearNormal && isSameInstance && isNearRoughness && isNearAlbedoLuminance;// && (length(velocity) < 1.0);
            if(isTemporalReuseEnable)
            {
                DIReservoir prevDIReservoir = DIReservoirBufferSrc[serialPrevID];
                DIReservoirTemporalReuse(currDIReservoir, prevDIReservoir, currID);
            }
        }

        DIReservoirBufferDst[serialCurrID] = currDIReservoir;
    }
}