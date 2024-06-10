#ifndef __DXRPHOTONMAPPER_H__
#define __DXRPHOTONMAPPER_H__

#include "AppBase.h"
#include "utility/Utility.h"
#include "utility/OBJ.h"
#include <DirectXMath.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <d3d12.h>
#include <dxgi1_6.h>

#include "d3dx12.h"
#include <pix3.h>
#include "Camera.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define STAGE_DIVISION 4
#define STAGE_DIVISION_FOR_LIGHT_POSITION 20

#define BITONIC_BLOCK_SIZE 1024
#define TRANSPOSE_BLOCK_SIZE 16

#define BITONIC2_THREAD_NUM 64

#define GRID_SORT_THREAD_NUM 1024
#define GRID_DIMENSION 256//if increase this param, grid lines are occured in caustics. (Cause : Photon Accumuration)
#define PLANE_SIZE 200

#define PHOTON_NUM_1D 1024
#define DENOISE_ITE 1
#define MAX_RECURSION_DEPTH 10//0---31
#define REAL_MAX_RECURSION_DEPTH 8//0---31

#define SPHERE_LIGHTS_SIZE_RATIO 0.7f

#define MAX_SPATIAL_REUSE_TAP 8

namespace HitGroups {
    static const wchar_t* ReflectReflactMaterialSphere = L"hgReflectReflactSpheres";
    static const wchar_t* ReflectReflactMaterialBox = L"hgReflectReflactBoxes";
    static const wchar_t* DefaultMaterialSphere = L"hgMaterialSpheres";
    static const wchar_t* DefaultMaterialBox = L"hgMaterlalBoxes";
    static const wchar_t* Floor = L"hgFloor";
    static const wchar_t* Obj0 = L"hgObj0";
    static const wchar_t* Obj1 = L"hgObj1";
    static const wchar_t* Light = L"hgLight";
}

namespace RayTracingDxlibs {
    static const wchar_t* RayGen = L"raygen.dxlib";
    static const wchar_t* Miss = L"miss.dxlib";
    static const wchar_t* DefaultMaterialClosestHit = L"closestHitMaterial.dxlib";
    static const wchar_t* DefaultMaterialWithTexClosestHit = L"closestHitMaterialWithTex.dxlib";
    static const wchar_t* LightClosestHit = L"closestHitLight.dxlib";
}

namespace RayTracingEntryPoints {
    static const wchar_t* RayGen = L"rayGen";
    static const wchar_t* Miss = L"miss";
    static const wchar_t* ClosestHitMaterial = L"materialClosestHit";
    static const wchar_t* ClosestHitMaterialWithTex = L"materialWithTexClosestHit";
    static const wchar_t* ClosestHitLight = L"lightClosestHit";    
    static const wchar_t* RayGenPhoton = L"photonEmitting";
    static const wchar_t* MissPhoton = L"photonMiss";
    static const wchar_t* ClosestHitMaterialPhoton = L"materialStorePhotonClosestHit";
    static const wchar_t* ClosestHitMaterialWithTexPhoton = L"materialWithTexStorePhotonClosestHit";
    static const wchar_t* RayGenSpatialReuse = L"spatialReuse";
}

namespace ComputeShaders {
    const LPCWSTR BitonicSort = L"BitonicSort_BitonicSort.cso";
    const LPCWSTR Transpose = L"BitonicSort_Transpose.cso";

    const LPCWSTR BitonicSort2 = L"BitonicSort.cso";

    const LPCWSTR BuildGrid = L"Grid3D_BuildGrid.cso";
    const LPCWSTR BuildGridIndices = L"Grid3D_BuildGridIndices.cso";
    const LPCWSTR ClearGridIndices = L"Grid3D_ClearGridIndices.cso";
    const LPCWSTR Copy = L"Grid3D_Copy.cso";
    const LPCWSTR RearrangePhoton = L"Grid3D_RearrangePhoton.cso";

    const LPCWSTR Denoise = L"Denoise.cso";

    const LPCWSTR ComputeVariance = L"computeVariance.cso";
    const LPCWSTR A_Trous = L"A-trous.cso";

    const LPCWSTR DebugView = L"debugView.cso";

    const LPCWSTR TemporalAccumulation = L"temporalAccumulation.cso";
    const LPCWSTR TemporalReuse = L"temporalReuse.cso";
}

template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class DxrPhotonMapper : public AppBase {
public:
    DxrPhotonMapper(u32 width, u32 height);

    void Initialize() override;
    void Terminate() override;

    void Update() override;
    void Draw() override;

    void OnMouseDown(MouseButton button, s32 x, s32 y) override;
    void OnMouseUp(MouseButton button, s32 x, s32 y) override;
    void OnMouseMove(s32 dx, s32 dy) override;
    void OnKeyDown(UINT8 wparam) override;
    void OnMouseWheel(s32 rotate) override;

private:

    struct float2
    {
        f32 x;
        f32 y;
    };

    struct float3
    {
        f32 x;
        f32 y;
        f32 z;
    };

    struct uint2
    {
        u32 x;
        u32 y;
    };

    struct Payload
    {
        float3 throughput;
        float3 caustics;
        uint2 storeIndexXY;
        float3 eyeDir;
        int recursive;
        unsigned int flags;
        float3 DI;
        float3 GI;
    };

    struct PhotonPayload
    {
        float3 throughput;
        s32 recursive;
        s32 storeIndex;
        s32 stored;
        f32 lambdaNM;
    };

    struct TriangleIntersectionAttributes
    {
        float2 barys;
    };

    struct DIReservoir
    {
        u32 lightID; //light ID of most important light
        float3 preSampledLightInfo;//light surface position / directionallight direction to light
        f32 targetPDF; //weight of light
        float3 targetPDF_3f; //weight of light(float 3)
        f32 W_sum; //sum of all weight
        f32 M; //number of ligts processed for this reservoir
    };

    enum SphereTypeCount {
        NormalSpheres = 9
    };

    enum BoxTypeCount {
        NormalBoxes = 6
    };

    enum OBJ0TypeCount {
        NormalOBJ0s = 1
    };

    enum OBJ1TypeCount {
        NormalOBJ1s = 1
    };

    enum StageType {
        StageType_Plane,
        StageType_Box
    };

    enum LightTypeCount {
        NormalLight = 1
    };



    struct SceneParam
    {
        XMMATRIX mtxView;
        XMMATRIX mtxProj;
        XMMATRIX mtxViewInv;
        XMMATRIX mtxProjInv;
        XMMATRIX mtxViewPrev;
        XMMATRIX mtxProjPrev;
        XMMATRIX mtxViewInvPrev;
        XMMATRIX mtxProjInvPrev;
        XMUINT4 flags;
        XMFLOAT4 photonParams;//x unused
        XMVECTOR cameraParams;//near far reserved reserved
        XMVECTOR gatherParams;
        XMVECTOR gatherParams2;
        XMVECTOR spotLightParams;
        XMVECTOR viewVec;
        XMUINT4 additional;
        XMUINT4 additional1;
        XMUINT4 additional2;
    };

    struct PhotonInfo
    {
        XMFLOAT3 throughput;
        XMFLOAT3 position;
        //XMFLOAT3 inDir;
    };

    struct BitonicSortCB
    {
        u32 mLevel;
        u32 mLevelMask;
        u32 mWidth;
        u32 mHeight;

        void set(u32 level, u32 levelMask, u32 width, u32 height)
        {
            mLevel = level;
            mLevelMask = levelMask;
            mWidth = width;
            mHeight = height;
        }
    };

    struct BitonicSortCB2
    {
        s32 inc;
        s32 dir;
    };

    struct GridCB
    {
        s32 numPhotons;
        XMFLOAT3 gridDimensions;
        f32 gridH;
        f32 photonExistRange;
    };

    struct DenoiseCB
    {
        u32 stepScale;
    };

    enum LightType
    {
        LightType_Sphere = 0,
        LightType_Rect = 1,
        LightType_Spot = 2,
        LightType_Directional = 3
    };

    struct ReSTIRParam
    {
        XMUINT4 data;
    };

    struct LightGenerateParam
    {
        XMFLOAT3 position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 emission = XMFLOAT3(0, 0, 0);
        XMFLOAT3 U = XMFLOAT3(0, 0, 0); //u vector for rectangle or spot light
        XMFLOAT3 V = XMFLOAT3(0, 0, 0); //v vector for rectangle or spot light
        f32 sphereRadius = 0; //radius for sphere light
        u32 type = 0; //Sphere Light 0 / Rect Light 1 / Spot Light 2 / Directional Light 3

        void setParamAsSphereLight(const XMFLOAT3 pos, const XMFLOAT3 emi, const f32 radius)
        {
            type = LightType_Sphere;
            position = pos;
            emission = emi;
            sphereRadius = radius;
        }

        void setParamAsRectLight(const XMFLOAT3 pos, const XMFLOAT3 emi, const XMFLOAT3 u, const XMFLOAT3 v)
        {
            type = LightType_Rect;
            position = pos;
            emission = emi;
            U = u;
            V = v;
        }

        void setParamAsSpotLight(const XMFLOAT3 pos, const XMFLOAT3 emi, const XMFLOAT3 u, const XMFLOAT3 v)
        {
            type = LightType_Spot;
            position = pos;
            emission = emi;
            U = u;
            V = v;
        }

        void setParamAsDirectionalLight(const XMFLOAT3 pos, const XMFLOAT3 emi)
        {
            type = LightType_Directional;
            position = pos;
            emission = emi;
        }
    };

    enum LightCount
    {
        LightCount_Sphere = 800,
        LightCount_Rect = 1,
        LightCount_Spot = 1,
        LightCount_Directional = 1,
        LightCount_ALL = LightCount_Sphere + LightCount_Rect + LightCount_Spot + LightCount_Directional
    };

    enum ModelType
    {
        ModelType_Crab,
        ModelType_TwistCube,
        ModelType_SimpleCube,
        ModelType_Teapot,
        ModelType_LikeWater,
        ModelType_Ocean,
        ModelType_Ocean2,
        ModelType_Diamond,
        ModelType_Skull,
        ModelType_HorseStatue,
        ModelType_Dragon,
        ModelType_Afrodyta,
        ModelType_Rock,
        ModelType_CurvedMesh,
        ModelType_DebugMesh
    };

    enum SceneType
    {
        SceneType_Simple,
        SceneType_Sponza,
        SceneType_BistroExterior,
        SceneType_BistroInterior,
        SceneType_SanMiguel,
    };

    enum Spectrum
    {
        Spectrum_D65,
        Spectrum_Stabdard,
        Spectrum_Fluorescent,
        Spectrum_IncandescentBulb,
        Spectrum_Count,
    };

    enum CausticsQuality
    {
        CausticsQuality_LOW = 256,
        CausticsQuality_MIDDLE = 512,
        CausticsQuality_HIGH = 1024
    };

    void InitializeCamera();

    void Setup();
    void SetupMeshMaterialAndPos();
    void CreateSceneBLAS();
    void CreateSceneTLAS();
    void CreateStateObject();
    void CreateLightGenerationBuffer();
    void CreateRootSignatureGlobal();
    void CreateRootSignatureLocal();
    void CreateShaderTable();
    void CreateComputeRootSignatureAndPSO();
    void CreateComputeShaderStateObject(const LPCWSTR& compiledComputeShaderName, ComPtr<ID3D12PipelineState>& computePipelineState, ComPtr<ID3D12RootSignature> rootSig);
    //�� combine
    void CreateAccelerationStructure();
    void CreateRaytracingRootSignatureAndPSO();
    void CreateRegularBuffer();
    void CreateConstantBuffer();

    void UpdateSceneTLAS();
    void UpdateSceneParams();
    void UpdateMaterialParams();
    void UpdateLightGenerateParams();
    void InitializeLightGenerateParams();
    void UpdateWindowText();

    void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, u32 pathSize);
    std::wstring GetAssetFullPath(LPCWSTR assetName);
    f32 Clamp(f32 min, f32 max, f32 src);
    f32 getFrameRate();
    //utility::TextureResource LoadTextureFromFile(const std::wstring& fileName);
    
    void SetupMeshInfo(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs);

    void Grid3DSort();
    void BitonicSortLDS();
    void BitonicSortSimple();

    void SpatiotemporalVarianceGuidedFiltering();

    ComPtr<ID3D12GraphicsCommandList4> mCommandList;
    static const u32 BackBufferCount = dx12::RenderDeviceDX12::BackBufferCount;
    
    //Meshes
    utility::PolygonMesh mMeshStage;
    utility::PolygonMesh mMeshSphere;
    utility::PolygonMesh mMeshBox;
    utility::PolygonMesh mMeshOBJ0;
    utility::PolygonMesh mMeshOBJ1;
    //utility::PolygonMesh mMeshLightSphere;
    std::wstring mOBJ0FileName;
    std::wstring mOBJ1FileName;
    std::wstring mStageTextureFileName;
    std::wstring mCubeMapTextureFileName;

    std::string mOBJFileName;
    std::string mOBJFolderName;

    utility::OBJ_MODEL mOBJModel;

    //ObjectAttributes of TLAS
    std::array<XMMATRIX, NormalSpheres> mSpheresNormalTbl;
    std::array<XMMATRIX, NormalBoxes> mBoxesNormalTbl;
    std::array<XMMATRIX, NormalOBJ0s> mOBJ0sNormalTbl;
    std::array<XMMATRIX, NormalOBJ1s> mOBJ1sNormalTbl;
    std::array<XMMATRIX, NormalLight> mLightTbl;

    XMMATRIX mOBJModelTRS;

    //Materials
    std::array<utility::MaterialParam, NormalSpheres> mNormalSphereMaterialTbl;
    std::array<utility::MaterialParam, NormalBoxes> mNormalBoxMaterialTbl;
    std::array<utility::MaterialParam, NormalOBJ0s> mOBJ0MaterialTbl;
    std::array<utility::MaterialParam, NormalOBJ1s> mOBJ1MaterialTbl;
    utility::MaterialParam mStageMaterial;
    ComPtr<ID3D12Resource> mNormalSphereMaterialCB;
    ComPtr<ID3D12Resource> mNormalBoxMaterialCB;
    ComPtr<ID3D12Resource> mReflectSphereMaterialCB;
    ComPtr<ID3D12Resource> mRefractSphereMaterialCB;
    ComPtr<ID3D12Resource> mReflectBoxMaterialCB;
    ComPtr<ID3D12Resource> mRefractBoxMaterialCB;
    ComPtr<ID3D12Resource> mOBJ0MaterialCB;
    ComPtr<ID3D12Resource> mOBJ1MaterialCB;
    ComPtr<ID3D12Resource> mStageMaterialCB;

    //Lights
    std::vector<LightGenerateParam> mLightGenerationParamTbl;
    ComPtr <ID3D12Resource> mLightGenerationParamBuffer;
    dx12::Descriptor mLightGenerationParamSRV;

    SceneParam mSceneParam;
    utility::TextureResource mGroundTex;
    utility::TextureResource mCubeMapTex;
    utility::TextureResource mDummyAlphaMask;

    Camera mCamera;

    //Acceleration Structures
    ComPtr<ID3D12Resource> mTLAS;
    ComPtr<ID3D12Resource> mTLASUpdate;
    dx12::Descriptor mTLASDescriptor;
    ComPtr<ID3D12Resource> mInstanceDescsBuffer;

    //Buffers
    ComPtr <ID3D12Resource> mFinalRenderResult;
    dx12::Descriptor mFinalRenderResultDescriptorUAV;
    dx12::Descriptor mFinalRenderResultDescriptorSRV;
    ComPtr<ID3D12Resource> mAccumulationBuffer;
    dx12::Descriptor mAccumulationBufferDescriptorUAV;
    ComPtr<ID3D12Resource> mPhotonMap;
    dx12::Descriptor mPhotonMapDescriptorSRV;
    dx12::Descriptor mPhotonMapDescriptorUAV;
    ComPtr<ID3D12Resource> mPhotonMapSorted;
    dx12::Descriptor mPhotonMapSortedDescriptorSRV;
    dx12::Descriptor mPhotonMapSortedDescriptorUAV;
    ComPtr<ID3D12Resource> mPhotonGrid;
    dx12::Descriptor mPhotonGridDescriptorSRV;
    dx12::Descriptor mPhotonGridDescriptorUAV;
    ComPtr<ID3D12Resource> mPhotonGridTmp;
    dx12::Descriptor mPhotonGridTmpDescriptorSRV;
    dx12::Descriptor mPhotonGridTmpDescriptorUAV;
    ComPtr<ID3D12Resource> mPhotonGridId;
    dx12::Descriptor mPhotonGridIdDescriptorSRV;
    dx12::Descriptor mPhotonGridIdDescriptorUAV;

    std::vector < ComPtr<ID3D12Resource>> mNormalDepthBufferTbl;
    std::vector < dx12::Descriptor> mNormalDepthBufferDescriptorSRVTbl;
    std::vector < dx12::Descriptor> mNormalDepthBufferDescriptorUAVTbl;
    std::vector < ComPtr<ID3D12Resource>> mIDRoughnessBufferTbl;
    std::vector < dx12::Descriptor> mIDRoughnessBufferDescriptorSRVTbl;
    std::vector < dx12::Descriptor> mIDRoughnessBufferDescriptorUAVTbl;
    std::vector < ComPtr<ID3D12Resource>> mPositionBufferTbl;
    std::vector < dx12::Descriptor> mPositionBufferDescriptorSRVTbl;
    std::vector < dx12::Descriptor> mPositionBufferDescriptorUAVTbl;
    std::vector < ComPtr<ID3D12Resource>> mAccumulationCountBufferTbl;
    std::vector < dx12::Descriptor> mAccumulationCountBufferDescriptorSRVTbl;
    std::vector < dx12::Descriptor> mAccumulationCountBufferDescriptorUAVTbl;
    ComPtr<ID3D12Resource> mDenoisedColorBuffer;
    dx12::Descriptor mDenoisedColorBufferDescriptorSRV;
    dx12::Descriptor mDenoisedColorBufferDescriptorUAV;

    ComPtr<ID3D12Resource> mDiffuseAlbedoBuffer;
    dx12::Descriptor mDiffuseAlbedoBufferDescriptorSRV;
    dx12::Descriptor mDiffuseAlbedoBufferDescriptorUAV;
    ComPtr<ID3D12Resource> mPrevIDBuffer;
    dx12::Descriptor mPrevIDBufferDescriptorSRV;
    dx12::Descriptor mPrevIDBufferDescriptorUAV;

    std::vector < ComPtr<ID3D12Resource>> mDIBufferPingPongTbl;
    std::vector < dx12::Descriptor> mDIBufferDescriptorSRVPingPongTbl;
    std::vector < dx12::Descriptor> mDIBufferDescriptorUAVPingPongTbl;

    std::vector < ComPtr<ID3D12Resource>> mGIBufferPingPongTbl;
    std::vector < dx12::Descriptor> mGIBufferDescriptorSRVPingPongTbl;
    std::vector < dx12::Descriptor> mGIBufferDescriptorUAVPingPongTbl;

    std::vector < ComPtr<ID3D12Resource>> mCausticsBufferPingPongTbl;
    std::vector < dx12::Descriptor> mCausticsBufferDescriptorSRVPingPongTbl;
    std::vector < dx12::Descriptor> mCausticsBufferDescriptorUAVPingPongTbl;

    std::vector < ComPtr<ID3D12Resource>> mLuminanceMomentBufferTbl;
    std::vector < dx12::Descriptor> mLuminanceMomentBufferDescriptorSRVTbl;
    std::vector < dx12::Descriptor> mLuminanceMomentBufferDescriptorUAVTbl;

    std::vector < ComPtr<ID3D12Resource>> mLuminanceVarianceBufferTbl;
    std::vector < dx12::Descriptor> mLuminanceVarianceBufferDescriptorSRVTbl;
    std::vector < dx12::Descriptor> mLuminanceVarianceBufferDescriptorUAVTbl;

    ComPtr<ID3D12Resource> mDebugTexture;
    dx12::Descriptor mDebugTextureDescriptorSRV;
    dx12::Descriptor mDebugTextureDescriptorUAV;

    ComPtr<ID3D12Resource> mDebugTexture0;
    dx12::Descriptor mDebugTexture0DescriptorSRV;
    dx12::Descriptor mDebugTexture0DescriptorUAV;

    ComPtr<ID3D12Resource> mDebugTexture1;
    dx12::Descriptor mDebugTexture1DescriptorSRV;
    dx12::Descriptor mDebugTexture1DescriptorUAV;

    std::vector < ComPtr<ID3D12Resource>> mDIReservoirPingPongTbl;
    std::vector < dx12::Descriptor> mDIReservoirDescriptorSRVPingPongTbl;
    std::vector < dx12::Descriptor> mDIReservoirDescriptorUAVPingPongTbl;

    std::vector < ComPtr<ID3D12Resource>> mDISpatialReservoirPingPongTbl;
    std::vector < dx12::Descriptor> mDISpatialReservoirDescriptorSRVPingPongTbl;
    std::vector < dx12::Descriptor> mDISpatialReservoirDescriptorUAVPingPongTbl;

    //ConstantBuffers
    std::vector<ComPtr<ID3D12Resource>> mBitonicLDSCB0Tbl;
    std::vector<ComPtr<ID3D12Resource>> mBitonicLDSCB1Tbl;
    std::vector<ComPtr<ID3D12Resource>> mBitonicLDSCB2Tbl;
    std::vector<ComPtr<ID3D12Resource>> mBitonicSimpleCBTbl;
    ComPtr<ID3D12Resource> mGridSortCB;
    std::vector<ComPtr<ID3D12Resource>> mDenoiseCBTbl;
    ComPtr<ID3D12Resource> mSceneCB;
    std::vector<ComPtr<ID3D12Resource>> mReSTIRParamCBTbl;

    //Pipeline State
    //--Ordinal Raytrace
    ComPtr<ID3D12StateObject> mRTPSO;
    ComPtr<ID3D12Resource> mShaderTable;
    D3D12_DISPATCH_RAYS_DESC mDispatchRayDesc;
    ComPtr<ID3D12RootSignature> mGlobalRootSig;
    std::unordered_map < std::string, u32> mRegisterMapGlobalRootSig;

    //--PhotonMapping
    ComPtr<ID3D12StateObject> mRTPSOPhoton;
    ComPtr<ID3D12Resource> mShaderTablePhoton;
    D3D12_DISPATCH_RAYS_DESC mDispatchPhotonRayDesc;
    ComPtr<ID3D12RootSignature> mGlobalRootSigPhoton;
    std::unordered_map < std::string, u32> mRegisterMapGlobalRootSigPhoton;

    //--Reservoir Spatial Reuse
    ComPtr<ID3D12StateObject> mRTPSOReservoirSpatialReuse;
    ComPtr<ID3D12Resource> mShaderTableReservoirSpatialReuse;
    D3D12_DISPATCH_RAYS_DESC mDispatchReservoirSpatialReuseRayDesc;
    ComPtr<ID3D12RootSignature> mGlobalRootSigReservoirSpatialReuse;
    std::unordered_map < std::string, u32> mRegisterMapGlobalRootSigReservoirSpatialReuse;

    //material binding
    ComPtr<ID3D12RootSignature> mLocalRootSigMaterial;
    std::unordered_map < std::string, u32> mRegisterMapGlobalLocalRootSigMaterial;
    ComPtr<ID3D12RootSignature> mLocalRootSigMaterialWithTex;
    std::unordered_map < std::string, u32> mRegisterMapGlobalLocalRootSigMaterialWithTex;

    ComPtr<ID3D12RootSignature> mRsBitonicSortLDS;
    std::unordered_map < std::string, u32> mRegisterMapBitonicSortLDS;
    ComPtr<ID3D12RootSignature> mRsTranspose;
    std::unordered_map < std::string, u32> mRegisterMapTranspose;
    ComPtr<ID3D12PipelineState> mBitonicSortLDSPSO;
    ComPtr<ID3D12PipelineState> mTransposePSO;

    ComPtr<ID3D12RootSignature> mRsBitonicSortSimple;
    std::unordered_map < std::string, u32> mRegisterMapBitonicSortSimple;
    ComPtr<ID3D12PipelineState> mBitonicSortSimplePSO;

    ComPtr<ID3D12RootSignature> mRsBuildGrid;
    std::unordered_map < std::string, u32> mRegisterMapBuildGrid;
    ComPtr<ID3D12RootSignature> mRsBuildGridIndices;
    std::unordered_map < std::string, u32> mRegisterMapBuildGridIndices;
    ComPtr<ID3D12RootSignature> mRsCopy;
    std::unordered_map < std::string, u32> mRegisterMapCopy;
    ComPtr<ID3D12RootSignature> mRsClearGridIndices;
    std::unordered_map < std::string, u32> mRegisterMapClearGridIndices;
    ComPtr<ID3D12RootSignature> mRsRearrangePhoton;
    std::unordered_map < std::string, u32> mRegisterMapRearrangePhoton;
    ComPtr<ID3D12PipelineState> mBuildGridPSO;
    ComPtr<ID3D12PipelineState> mBuildGridIndicesPSO;
    ComPtr<ID3D12PipelineState> mCopyPSO;
    ComPtr<ID3D12PipelineState> mClearGridIndicesPSO;
    ComPtr<ID3D12PipelineState> mRearrangePhotonPSO;

    ComPtr<ID3D12RootSignature> mRsComputeVariance;
    std::unordered_map < std::string, u32> mRegisterMapComputeVariance;
    ComPtr<ID3D12PipelineState> mComputeVariancePSO;

    ComPtr<ID3D12RootSignature> mRsA_TrousWaveletFilter;
    std::unordered_map < std::string, u32> mRegisterMapA_TrousWaveletFilter;
    ComPtr<ID3D12PipelineState> mA_TrousWaveletFilterPSO;

    ComPtr<ID3D12RootSignature> mRsDebugView;
    std::unordered_map < std::string, u32> mRegisterMapDebugView;
    ComPtr<ID3D12PipelineState> mDebugViewPSO;

    ComPtr<ID3D12RootSignature> mRsTemporalAccumulation;
    std::unordered_map < std::string, u32> mRegisterMapTemporalAccumulation;
    ComPtr<ID3D12PipelineState> mTemporalAccumulationPSO;

    ComPtr<ID3D12RootSignature> mRsTemporalReuse;
    std::unordered_map < std::string, u32> mRegisterMapTemporalReuse;
    ComPtr<ID3D12PipelineState> mTemporalReusePSO;

    u32 mRenderFrame = 0;
    u32 mSeedFrame = 0;

    std::wstring mAssetPath;

    f32 mGlassObjYOfsset;
    XMFLOAT3 mGlassObjScale;
    f32 mMetalObjYOfsset;
    XMFLOAT3 mMetalObjScale;
    f32 mLightRange;
    f32 mIntenceBoost;
    f32 mGatherRadius;
    u32 mGatherBlockRange;
    f32 mStandardPhotonNum;
    bool mIsDebug;

    f32 mLightPosX;
    f32 mLightPosY;
    f32 mLightPosZ;

    f32 mPhi;
    f32 mTheta;
    f32 mPhiDirectional;
    f32 mThetaDirectional;

    u32 mLightCount;
    bool mIsSpotLightPhotonMapper;

    f32 mCausticsBoost;

    f32 mGlassRotateRange;

    ModelType mGlassModelType;
    ModelType mMetalModelType;

    bool mVisualizeLightRange;

    bool mInverseMove;
    bool mIsApplyCaustics;
    bool mIsUseDenoise;
    s32 mSpectrumMode;
    bool mIsMoveModel;
    bool mIsUseTexture;
    bool mIsIndirectOnly;
    bool mIsUseDebugView;

    LARGE_INTEGER mCpuFreq;
    LARGE_INTEGER mStartTime;
    LARGE_INTEGER mEndTime;

    u32 mLightLambdaNum;

    u32 mPhotonMapSize1D;

    StageType mStageType;
    SceneType mSceneType;

    utility::MaterialParam mMaterialParam0;
    utility::MaterialParam mMaterialParam1;

    bool mIsTargetGlass;

    bool mIsUseAccumulation;
    bool mIsUseNEE;
    //bool mIsUseDirectionalLight;

    XMFLOAT3 mInitEyePos;
    XMFLOAT3 mInitTargetPos;

    u32 mRecursionDepth = 0;
    u32 mSceneLightNum = 0;

    f32 mStageOffsetY = 0;
    f32 mStageOffsetX = 0;
    f32 mStageOffsetZ = 0;

    bool mIsUseManySphereLightLighting;
    bool isPrimalLightSRVUpdate = true;
    bool mIsUseWRS_RIS = false;
    bool mIsTemporalAccumulationForceDisable = false;

    bool mIsUseReservoirTemporalReuse = false;
    bool mIsUseReservoirSpatialReuse = false;

    u32 mSpatialReuseTap = 4;

    bool mIsUseMetallicTest = false;

    bool mIsHistoryResetRequested = false;

    bool mIsAlbedoOne = false;
};

#endif
