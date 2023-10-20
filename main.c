#if !__has_include(<Windows.h>)
#error you need to download the Windows SDK: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
#endif

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <Windows.h>
#include <Shlwapi.h>
#include <commdlg.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <D3Dcompiler.h>

#if !__has_include(<cglm/cglm.h>)
#error you need to download cglm: https://github.com/recp/cglm
#endif

#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <cglm/call.h>
#include <cglm/cam.h>
#include <cglm/clipspace/persp_lh_zo.h>
#include <cglm/clipspace/view_lh.h>

#if !__has_include(<zlib.h>)
#error you need to download zlib: http://www.zlib.net/
#endif

#include <zlib.h>

#include <stdint.h>

#pragma comment(linker, "/DEFAULTLIB:D3d12.lib")
#pragma comment(linker, "/DEFAULTLIB:DXGI.lib")
#pragma comment(linker, "/DEFAULTLIB:D3DCompiler.lib")
#pragma comment(linker, "/DEFAULTLIB:dxguid.lib")
#pragma comment(linker, "/DEFAULTLIB:Shlwapi.lib")
#pragma comment(linker, "/DEFAULTLIB:zlib.lib")

#include <stdbool.h>
#include <stdalign.h>
#define nullptr ((void*)0)

#define printf_dbg ERROR
HANDLE ConsoleHandle;

void printMatrix4x4(mat4 matrix)
{
	for (int i = 0; i < 4; i++)
	{
		printf("%f   %f   %f   %f\n", matrix[0][i], matrix[1][i], matrix[2][i], matrix[3][i]);
	}
}

ID3D12Device* Device;

inline void THROW_ON_FAIL_IMPL(HRESULT hr, int line)
{
	if (hr == 0x887A0005)
	{
		THROW_ON_FAIL_IMPL(ID3D12Device_GetDeviceRemovedReason(Device), line);
	}

	if (FAILED(hr))
	{
		LPWSTR messageBuffer;
		DWORD formattedErrorLength = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			hr,
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPWSTR)&messageBuffer,
			0,
			nullptr
		);

		if (formattedErrorLength == 0)
			WriteConsoleA(ConsoleHandle, "an error occured, unable to retrieve error message\n", 51, nullptr, nullptr);
		else
		{
			WriteConsoleA(ConsoleHandle, "an error occured: ", 18, nullptr, nullptr);
			WriteConsoleW(ConsoleHandle, messageBuffer, formattedErrorLength, nullptr, nullptr);
			WriteConsoleA(ConsoleHandle, "\n", 1, nullptr, nullptr);
		}

		char buffer[50];
		int stringlength = _snprintf_s(buffer, 50, _TRUNCATE, "error code: 0x%X\nlocation:line %i\n", hr, line);
		WriteConsoleA(ConsoleHandle, buffer, stringlength, nullptr, nullptr);

		

		RaiseException(0, EXCEPTION_NONCONTINUABLE, 0, nullptr);
	}
}

[[nodiscard]]
inline int FloatAsInt(float in)
{
	return *((int*)&in);
}

[[nodiscard]]
inline float IntAsFloat(int in)
{
	return *((float*)&in);
}

[[nodiscard]]
inline int pad(int input, int size)
{
	int result = input - input % size;
	if (result < input)
		result += size;
	return result;
}

[[nodiscard]]
inline char* padPointer_impl(char* input, int size)
{
	uintptr_t offset = (~(uintptr_t)input & ((uintptr_t)size - 1)) + 1;
	return (void*)((uintptr_t)input + offset);
}

#define PadPointer(ptr, size) ((typeof(ptr))(padPointer_impl((char*)ptr, size)))

#define pad256(x) pad(x, 256)

#define THROW_ON_FAIL(x) THROW_ON_FAIL_IMPL(x, __LINE__)

#define THROW_ON_FALSE(x) if((x) == FALSE) THROW_ON_FAIL(GetLastError())

#define VALIDATE_HANDLE(x) if((x) == nullptr || (x) == INVALID_HANDLE_VALUE) THROW_ON_FAIL(GetLastError())

#define DEBUG_ASSERT(x) if(((bool)(x)) != true && IsDebuggerPresent() == TRUE)DebugBreak();

#define SwapEndian(x) (typeof(x))_Generic((x), \
						short: _byteswap_ushort,\
						unsigned short: _byteswap_ushort,\
						int: _byteswap_ulong,  \
						unsigned int: _byteswap_ulong, \
						long: _byteswap_ulong, \
						unsigned long: _byteswap_ulong, \
						long long: _byteswap_uint64, \
						unsigned long long: _byteswap_uint64 \
						)(x)

#define SwapEndianFloat(x) IntAsFloat(_byteswap_ulong(FloatAsInt(x)))

#define OffsetPointer(x, offset) ((typeof(x))((char*)x + (offset)))

#define countof(x) (sizeof(x) / sizeof(x[0]))

#define INIT_MAT4(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
{\
	{ a, e, i, m },\
	{ b, f, j, n },\
	{ c, g, k, o },\
	{ d, h, l, p }\
}

struct VertexData
{
	float posx;
	float posy;
	float posz;
	float nrmx;
	float nrmy;
	float nrmz;
	float tex0u;
	float tex0v;
	float tex1u;
	float tex1v;
	float tex2u;
	float tex2v;
	float tex3u;
	float tex3v;
	float tex4u;
	float tex4v;
	float tex5u;
	float tex5v;
	float tex6u;
	float tex6v;
	float tex7u;
	float tex7v;
};

struct TextureOutputData
{
	int16_t Width;
	int16_t Height;
	BYTE ImageData;
};

vec3 cameraPosition = { 0.0, 0.0, 1.f };
float CameraPitch = 0.0;
float CameraYaw = 0.0;
POINT cursorPos;
LPCTSTR WindowName = L"MetroidPrimePC";

LPCTSTR WindowTitle = L"Metroid Prime Level Viewer";


int Width = 800;
int Height = 600;

int MSAACount = 1;
D3D_ROOT_SIGNATURE_VERSION RootSignatureVersion;

bool FullScreen = false;

bool Running = true;

bool bTearingSupport = false;
bool bVsync = true;
bool bWarp = false;

DWORD PageSize;

#define TotalTextureSlots 2048

#define TotalObjectSlots 4096

#define TotalMaterialSlots 4096

LARGE_INTEGER ProcessorFrequency;

LRESULT CALLBACK PreInitProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK IdleProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#define FrameBufferCount 3

IDXGISwapChain3* SwapChain;
ID3D12CommandQueue* CommandQueue;
ID3D12CommandQueue* CopyCommandQueue;

ID3D12DescriptorHeap* rtvDescriptorHeap;
ID3D12DescriptorHeap* dsDescriptorHeap;
ID3D12DescriptorHeap* TextureDescriptorHeap;

D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart;
D3D12_CPU_DESCRIPTOR_HANDLE dsvHeapStart;
D3D12_CPU_DESCRIPTOR_HANDLE textureHeapStart;

int rtvDescriptorSize;
int cbvDescriptorSize;

ID3D12Resource* RenderTargets[FrameBufferCount];
ID3D12Resource* RenderTargetsMSAA[FrameBufferCount];

ID3D12Resource* VertexBuffer[TotalObjectSlots];
ID3D12Resource* IndexBuffer[TotalObjectSlots];

int GlobalMaterialCount = 0;

ID3D12Heap* DepthBufferHeap;
ID3D12Resource* DepthStencilBuffer;

ID3D12Resource* OcclusionVertexBuffer;

ID3D12Resource* PredicationBuffer;
ID3D12QueryHeap* PredicationQueryHeap;

ID3D12Resource* PerObjectConstantBuffer[FrameBufferCount];
ID3D12Resource* VertexShaderConstantBuffer[FrameBufferCount];
ID3D12Resource* PixelShaderConstantBuffer[FrameBufferCount];

ID3D12Heap* TextureBufferHeap;
ID3D12Resource* TextureBuffers[TotalTextureSlots];
ID3D12Resource* TextureBufferUploadHeaps[TotalTextureSlots];

uint32_t TextureLoadQueueSize = 0;
uint32_t TextureLoadQueue[TotalTextureSlots];

uint32_t TextureStandbyListSize = 0;
uint32_t TextureStandbyList[TotalTextureSlots];

struct StandbyModel
{
	uint32_t modelIndex;
	uint32_t cmdlID;
};

uint32_t ModelLoadQueue[TotalObjectSlots];
uint32_t ModelLoadQueueSize = 0;


struct StandbyModel ModelStandbyList[TotalObjectSlots];
uint32_t ModelStandbyListSize = 0;

D3D12_CLEAR_VALUE OptimizedClearValue = { 0 };

ID3D12CommandAllocator* CommandAllocator[FrameBufferCount];
ID3D12GraphicsCommandList* GraphicsCommandList;
ID3D12Fence* fence[FrameBufferCount];
HANDLE FenceEvent;
UINT64 FenceValue[FrameBufferCount];

ID3D12CommandAllocator* CopyCommandAllocator;
ID3D12GraphicsCommandList* CopyCommandList;
ID3D12Fence* CopyCommandFence;
UINT64 CopyCommandFenceValue;


int FrameIndex;


void WaitForPreviousFrame();


ID3D12PipelineState* PipelineStateObject_FinalStage;

ID3D12RootSignature* RootSignature;
ID3D12RootSignature* ComputeRootSignature;

D3D12_VIEWPORT Viewport = { 0 };

D3D12_RECT ScissorRect = { 0 };

D3D12_VERTEX_BUFFER_VIEW VertexBufferView[TotalObjectSlots] = { 0 };
D3D12_INDEX_BUFFER_VIEW IndexBufferView[TotalObjectSlots] = { 0 };

D3D12_VERTEX_BUFFER_VIEW OcclusionMeshVertexBufferView = { 0 };
D3D12_INDEX_BUFFER_VIEW OcclusionMeshIndexBufferView = { 0 };

struct Script_Actor
{
	float pos[3];
	float rot[3];
	float scale[3];
	float scanOffset[3];
	float health;
	float knockbackResistance;
	uint32_t powerBeamVulnerability;
	uint32_t iceBeamVulnerability;
	uint32_t waveBeamVulnerability;
	uint32_t plasmaBeamVulnerability;
	uint32_t bombVulnerability;
	uint32_t powerBombVulnerability;
	uint32_t missileVulnerability;
	uint32_t boostBallVulnerability;
	uint32_t phazonVulnerability;
	uint32_t aiVulnerability;
	uint32_t poisonWaterVulnerability;
	uint32_t lavaVulnerability;
	uint32_t hotVulnerability;
	uint32_t chargedPowerBeamVulnerability;
	uint32_t chargedIceBeamVulnerability;
	uint32_t chargedWaveBeamVulnerability;
	uint32_t chargedPlasmaBeamVulnerability;
	uint32_t chargedPhazonBeamVulnerability;
	uint32_t superMissileVulnerability;
	uint32_t iceSpreaderVulnerability;
	uint32_t wavebusterVulnerability;
	uint32_t flamethrowerVulnerability;
	uint32_t phazonComboVulnerability;
	uint32_t CmdlID;
};

struct Script_Platform
{
	float pos[3];
	float rot[3];
	float scale[3];
	float scanOffset[3];
	uint32_t CmdlID;
};

struct Script_AreaAttributes
{
	uint32_t unknown;
	bool showSkybox;
	uint32_t environmentalEffect;
	float initialEnvironmentalEffectDensity;
	float initialThermalHeatLevel;
	float xrayFogDistance;
	float initialWorldLightingLevel;
	uint32_t skyboxModel;
	uint32_t phazonType;
};


struct Material
{
	int VertexAttributeCount;
	int UVChannelCount;
};

struct Texgen
{
	alignas(16) int32_t TexCoordGenType;
	int32_t TexCoordSource;
	int32_t PreTransformIndex;
	int32_t bNormalize;
	int32_t PostTransformIndex;
};

struct VertexConstantBuffer
{
	mat4 pretransformMatrices[5];
	mat4 posttransformMatrices[5];
	int TexgenCount;
	struct Texgen texgens[5];
};

const int VertexPerObjectConstantsAlignedSize = (sizeof(struct VertexConstantBuffer) + 255) & ~255;

struct PerObjectConstants
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 view_inv;
	float secondsmod900;
};

const int PerObjectDataAlignedSize = (sizeof(struct PerObjectConstants) + 255) & ~255;

struct PerObjectConstants cbPerObject = { 0 };

UINT8* perObjectCbvGPUAddress[FrameBufferCount];
UINT8* vertexCbvGPUAddress[FrameBufferCount];
UINT8* pixelCbvGPUAddress[FrameBufferCount];

struct TEVStage
{
	int registerInputs[4];
	int combineOperator;
	float bias;
	float scale;
	int clamp;
	int outputRegister;
	int texCoordIndex;
	uint8_t KonstColor[4];
};

#define TEV_NO_TEXTURE_PRESENT 65535

struct PixelConstantBuffer
{
	int TEVStageCount;
	struct TEVStage TEVStages[16];
	struct TEVStage TEVAlphaStages[16];
	float KonstColors[4][4];
	float AlphaThreshold;
};

struct TextureIndexConstantBuffer
{
	uint32_t indices[16];
};

const int PixelPerObjectConstantsAlignedSize = (sizeof(struct TextureIndexConstantBuffer) + 255) & ~255;

struct PerSurfaceData
{
	vec3 centroid;
	int materialIndex;
	int surfaceSize;
	int surfaceOffset;
};

struct uvanim
{
	int mode;
	float par1;
	float par2;
	float par3;
	float par4;
};

struct PipelineStateData
{
	uint64_t materialHash;
	int shaderIndex;
	bool enableDepth;
	short BlendSourceFactor;
	short BlendDestinationFactor;
};

struct PerMaterialData
{
	int pipelineStateIndex;
	bool occlusionMesh;
	bool transparent;
	int animationCount;
	struct uvanim animations[4];
};

struct MaterialConstantBuffers
{
	struct VertexConstantBuffer vertexCB;
	struct PixelConstantBuffer pixelCB;
	struct TextureIndexConstantBuffer textureIndices;
};

struct ModelOutputData
{
	int TextureCount;
	float AABB[6];
	_Field_size_full_(TextureCount) uint32_t* textureID;
	int materialCount;
	int firstMaterialIndex;
	int VertexCount;
	int IndexCount;
	_Field_size_full_opt_(VertexCount) struct VertexData* VertexBuffer;
	_Field_size_full_opt_(IndexCount) uint32_t* IndexBuffer;
	int surfaceCount;
	_Field_size_full_(surfaceCount) struct PerSurfaceData* pod;
	_Field_size_full_(materialCount) struct PerMaterialData* pmd;
	_Field_size_full_(materialCount) struct PipelineStateData* psd;
	_Field_size_full_(materialCount) struct MaterialConstantBuffers* materials;
};

struct ScriptObject
{
	uint32_t objectType;
	uint32_t instanceID;
	void* typeSpecificData;
};

struct ScriptLayer
{
	int scriptObjectCount;
	_Field_size_full_(scriptObjectCount) struct ScriptObject* scriptObjectBuffer;
};

#define MAX_WORLD_MODELS 544

struct worldModel
{
	uint16_t visorFlags;
	uint16_t surfaceCount;
	float AABB[6];
};

struct levelArea
{
	int modelIndex;
	uint32_t nameID;
	float AABB[6];
	int worldModelCount;
	_Field_size_full_(worldModelCount) uint16_t* worldModelSurfaceCounts;
	_Field_size_full_(worldModelCount) struct worldModel* perWorldModelInfo;
	int scriptLayerCount;
	_Field_size_full_(scriptLayerCount) struct ScriptLayer* scriptLayerBuffer;
};

int maxSurfacesPerWorldModel = 0;

#define MAX_LEVEL_AREAS 64

struct GameLevel
{
	uint32_t nameID;
	uint32_t skybox;
	uint32_t areaCount;
	uint32_t areaIDs[MAX_LEVEL_AREAS];
	struct levelArea areas[MAX_LEVEL_AREAS];
};

struct GameLevel LoadedLevel;

struct SettingsType
{
	float fov;
	float NormalGravityAccel;
	float FluidGravityAccel;
	float VerticalJumpAccel;
	float PlayerHeight;
};

struct SettingsType Settings;

struct PlayerType
{
	vec3 position;
	vec3 velocity;
	int currentAreaIndex;
};


struct PlayerType Player = { 0 };


int CurrentAreaIndex = 0;

int maxScriptLayers = 0;

struct ObjectPosition
{
	mat4 RotMat;
	vec3 Position;
	float Rotation;
	mat4 WorldMat;
};

struct ObjectPosition ObjectPositions[TotalObjectSlots];

struct PixelShaderByteCode
{
	uint64_t hash;
	ID3DBlob* bytecode;
};

int PixelShaderCount = 0;
struct PixelShaderByteCode PixelShaders[TotalMaterialSlots];

struct PipelineStateWithHash
{
	uint64_t hash;
	ID3D12PipelineState* pso;
};

int PipelineStateCount = 0;
struct PipelineStateWithHash PipelineStates[TotalMaterialSlots];

ID3D12PipelineState* predicationPipeline;
ID3D12PipelineState* debugPipeline;

mat4 cameraProjMat = { 0 };
mat4 cameraViewMat;

int ModelCount = 0;
struct ModelOutputData* Models[TotalObjectSlots];

mat4 AreaPositions[512];

struct ModelOutputData* Hangar;

typedef int32_t FourCC;

LONG WINAPI PageFaultDetector(PEXCEPTION_POINTERS ExceptionInfo)
{
	if (ExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	
	if (ExceptionInfo->ExceptionRecord->ExceptionInformation[0] != 1)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}


	MEMORY_BASIC_INFORMATION memoryInfo;

	DEBUG_ASSERT(VirtualQuery((LPCVOID)ExceptionInfo->ExceptionRecord->ExceptionInformation[1], &memoryInfo, sizeof(memoryInfo)));

	if (memoryInfo.State != MEM_RESERVE || memoryInfo.Protect != 0)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	void* memory = VirtualAlloc((LPVOID)ExceptionInfo->ExceptionRecord->ExceptionInformation[1], PageSize * 4, MEM_COMMIT, PAGE_READWRITE);

	DEBUG_ASSERT(memory != nullptr);


	return EXCEPTION_CONTINUE_EXECUTION;
}

void GeneratePixelShader(struct PixelConstantBuffer* _In_ pcb, ID3DBlob** _Out_ outputShader)
{
	char* hlsl = VirtualAlloc(
		nullptr,
		1024 * 1024,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);

	char* WritePointer = hlsl;


	WritePointer += _snprintf_s(WritePointer, 4096, _TRUNCATE,
		"#define	GX_TEVPREV 0\n"
		"\n"
		"#define	GX_TEVREG0 1\n"
		"\n"
		"#define	GX_TEVREG1 2\n"
		"\n"
		"#define	GX_TEVREG2 3\n"
		"\n"
		"struct VS_OUTPUT\n"
		"{\n"
		"	float4 pos : SV_POSITION;\n"
		"	float3 nrm : NORMAL;\n"
		"	float3 color : COLOR0;\n"
		"	float2 uv0 : TEXCOORD0;\n"
		"	float2 uv1 : TEXCOORD1;\n"
		"	float2 uv2 : TEXCOORD2;\n"
		"	float2 uv3 : TEXCOORD3;\n"
		"	float2 uv4 : TEXCOORD4;\n"
		"	float2 uv5 : TEXCOORD5;\n"
		"	float2 uv6 : TEXCOORD6;\n"
		"	float2 uv7 : TEXCOORD7;\n"
		"};\n"
		"\n"
		"struct TEVConstantBuffer\n"
		"{\n"
		"	int Tex0Index;\n"
		"	int Tex1Index;\n"
		"	int Tex2Index;\n"
		"	int Tex3Index;\n"
		"	int Tex4Index;\n"
		"	int Tex5Index;\n"
		"	int Tex6Index;\n"
		"	int Tex7Index;\n"
		"	int Tex8Index;\n"
		"	int Tex9Index;\n"
		"	int Tex10Index;\n"
		"	int Tex11Index;\n"
		"	int Tex12Index;\n"
		"	int Tex13Index;\n"
		"	int Tex14Index;\n"
		"	int Tex15Index;\n"
		"};\n"
		"\n"
		"\n"
		"float3 float3from1(float input)\n"
		"{\n"
		"	return float3(input, input, input);\n"
		"}\n"
		"\n"
		"float4 float4from3(float3 input)\n"
		"{\n"
		"	return float4(input.x, input.y, input.z, 1);\n"
		"}\n"
		"\n"
		"float3 float3from4(float4 input)\n"
		"{\n"
		"	return float3(input.x, input.y, input.z);\n"
		"}\n"
		"ConstantBuffer<TEVConstantBuffer> Materialcb : register(b0, space0);\n"
		"Texture2D textures[] : register(t0);\n"
		"SamplerState mySampler : register(s0);\n"
		"\n"
		"\n"
		"float4 main(VS_OUTPUT input) : SV_TARGET\n"
		"{\n"
		"\n"
		"	float3 registers[4];\n"
		"\n"
		"	float alphaRegisters[4];\n"
		"\n"
		"\n"
		"	registers[0] = float3(1, 1, 1);\n"
		"	registers[1] = float3(1, 1, 1);\n"
		"	registers[2] = float3(1, 1, 1);\n"
		"	registers[3] = float3(1, 1, 1);\n"
		"\n"
		"	alphaRegisters[0] = 1;\n"
		"	alphaRegisters[1] = 1;\n"
		"	alphaRegisters[2] = 1;\n"
		"	alphaRegisters[3] = 1;\n"
		"\n"
		"\n"
		"	float3 colorChannels[4];\n"
		"	float alphaChannels[4];\n"
		"\n\n");

	for (int TEVStageNum = 0; TEVStageNum < pcb->TEVStageCount; TEVStageNum++)
	{
		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");
		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	const int Stage%iTexCoordIndex = %i;\n", TEVStageNum, pcb->TEVStages[TEVStageNum].texCoordIndex);

		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n\n");

		for (int registerNum = 0; registerNum < 4; registerNum++)
		{
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	//color input flag %i: %X\n", registerNum, pcb->TEVStages[TEVStageNum].registerInputs[registerNum]);
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	colorChannels[%i] = ", registerNum);

			switch (pcb->TEVStages[TEVStageNum].registerInputs[registerNum])
			{
			case 0x0:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "registers[GX_TEVPREV];\n", TEVStageNum - 1, 0);
				break;
			case 0x2:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "registers[GX_TEVREG0];\n", TEVStageNum - 1, 0);
				break;
			case 0x4:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "registers[GX_TEVREG1];\n", TEVStageNum - 1, 0);
				break;
			case 0x6:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "registers[GX_TEVREG2];\n", TEVStageNum - 1, 0);
				break;
			case 0x8:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "float3from4(textures[Materialcb.Tex%iIndex].Sample(mySampler, input.uv%i));\n", TEVStageNum, TEVStageNum);
				break;
			case 0x9:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "float3from1(textures[Materialcb.Tex%iIndex].Sample(mySampler, input.uv%i).w);\n", TEVStageNum, TEVStageNum);
				break;
			case 0xA:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "input.color;\n");
				break;
			case 0xC:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "float3(1, 1, 1);\n");
				break;
			case 0xE:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "float3(%f, %f, %f);\n",
					pcb->TEVStages[TEVStageNum].KonstColor[0] / 255.f,
					pcb->TEVStages[TEVStageNum].KonstColor[1] / 255.f,
					pcb->TEVStages[TEVStageNum].KonstColor[2] / 255.f
				);
				break;
			case 0xF:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "float3(0, 0, 0);\n");
				break;
			default:
				DebugBreak();
			}
			
				
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");
		}
		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");

		for (int registerNum = 0; registerNum < 4; registerNum++)
		{
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	//alpha input flag %i: %X\n", registerNum, pcb->TEVAlphaStages[TEVStageNum].registerInputs[registerNum]);
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	alphaChannels[%i] = ", registerNum);

			switch (pcb->TEVAlphaStages[TEVStageNum].registerInputs[registerNum])
			{
			case 0x0:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "alphaRegisters[GX_TEVPREV];\n", TEVStageNum - 1);
				break;
			case 0x1:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "alphaRegisters[GX_TEVREG0];\n", TEVStageNum - 1);
				break;
			case 0x2:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "alphaRegisters[GX_TEVREG1];\n", TEVStageNum - 1);
				break;
			case 0x3:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "alphaRegisters[GX_TEVREG2];\n", TEVStageNum - 1);
				break;
			case 0x4:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "textures[Materialcb.Tex%iIndex].Sample(mySampler, input.uv%i).w;\n", TEVStageNum, TEVStageNum);
				break;
			case 0x5:
				DebugBreak();
				break;
			case 0x6:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "%f;\n", pcb->TEVStages[TEVStageNum].KonstColor[3] / 255.f);
				break;
			case 0x7:
				WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "0.0f;\n");
				break;
			default:
				DebugBreak();
			}

			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");
		}
		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");

		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	//combine:\n");

		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	//RGB:\n");
		
		switch (pcb->TEVStages[TEVStageNum].combineOperator)
		{
		case 0:
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	registers[%i] = (colorChannels[3] + ((1.0 - colorChannels[2]) * colorChannels[0] + colorChannels[2] * colorChannels[1]) + %f) * %f;\n",
				pcb->TEVStages[TEVStageNum].outputRegister,
				pcb->TEVStages[TEVStageNum].bias,
				pcb->TEVStages[TEVStageNum].scale
			);
			break;
		case 1:
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	registers[%i] = (colorChannels[3] - ((1.0 - colorChannels[2]) * colorChannels[0] + colorChannels[2] * colorChannels[1]) + %f) * %f;\n",
				pcb->TEVStages[TEVStageNum].outputRegister,
				TEVStageNum, TEVStageNum, TEVStageNum, TEVStageNum, TEVStageNum,
				pcb->TEVStages[TEVStageNum].bias,
				pcb->TEVStages[TEVStageNum].scale
			);
			break;
		default:
			DebugBreak();
			break;
		}
		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");

		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	//ALPHA:\n");
		switch (pcb->TEVAlphaStages[TEVStageNum].combineOperator)
		{
		case 0:
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	alphaRegisters[%i] = (alphaChannels[3] + ((1.0 - alphaChannels[2]) * alphaChannels[0] + alphaChannels[2] * alphaChannels[1]) + %f) * %f;\n",
				pcb->TEVAlphaStages[TEVStageNum].outputRegister,
				TEVStageNum, TEVStageNum, TEVStageNum, TEVStageNum, TEVStageNum,
				pcb->TEVAlphaStages[TEVStageNum].bias,
				pcb->TEVAlphaStages[TEVStageNum].scale
			);
			break;
		case 1:
			WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	alphaRegisters[%i] = (alphaChannels[3] - ((1.0 - alphaChannels[2]) * alphaChannels[0] + alphaChannels[2] * alphaChannels[1]) + %f) * %f;\n",
				pcb->TEVAlphaStages[TEVStageNum].outputRegister,
				TEVStageNum, TEVStageNum, TEVStageNum, TEVStageNum, TEVStageNum,
				pcb->TEVAlphaStages[TEVStageNum].bias,
				pcb->TEVAlphaStages[TEVStageNum].scale
			);
			break;
		default:
			DebugBreak();
			break;
		}
		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n");

		WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "\n\n\n");
	}

	WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	clip(alphaRegisters[GX_TEVPREV] - %f);\n", pcb->AlphaThreshold);

	WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "	return float4(registers[GX_TEVPREV], alphaRegisters[GX_TEVPREV]);\n");

	WritePointer += _snprintf_s(WritePointer, 1024, _TRUNCATE, "}\n");

	
	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3DCompile(
		hlsl,
		WritePointer - hlsl,
		nullptr,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_1",
		D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES,
		0,
		outputShader,
		&errorBlob);
	THROW_ON_FAIL(hr);

	if (errorBlob)
	{
		ID3D10Blob_Release(errorBlob);
		DebugBreak();
	}

	DEBUG_ASSERT(VirtualFree(hlsl, 0, MEM_RELEASE) != 0);
}

int Unpack565(const uint8_t* const _In_ packed, uint8_t* _Out_ color)
{
	const int value = (int)packed[1] | ((int)packed[0] << 8);

	const uint8_t red = (uint8_t)((value >> 11) & 0x1f);
	const uint8_t green = (uint8_t)((value >> 5) & 0x3f);
	const uint8_t blue = (uint8_t)(value & 0x1f);

	color[0] = (red << 3) | (red >> 2);
	color[1] = (green << 2) | (green >> 4);
	color[2] = (blue << 3) | (blue >> 2);
	color[3] = 255;


	return value;
}

void DecompressColorGCN(const uint32_t _In_ texWidth, uint8_t* _Out_writes_bytes_all_(texWidth * 32) rgba, const void* const _In_ block)
{
	const uint8_t* bytes = block;

	uint8_t codes[16];
	const int a = Unpack565(bytes, codes);
	const int b = Unpack565(bytes + 2, codes + 4);

	for (int i = 0; i < 3; ++i)
	{
		const int c = codes[i];
		const int d = codes[4 + i];

		if (a <= b)
		{
			codes[8 + i] = (uint8_t)((c + d) / 2);
			codes[12 + i] = codes[8 + i];
		}
		else
		{
			codes[8 + i] = (uint8_t)((c * 5 + d * 3) >> 3);
			codes[12 + i] = (uint8_t)((c * 3 + d * 5) >> 3);
		}
	}

	codes[8 + 3] = 255;
	codes[12 + 3] = (a <= b) ? 0 : 255;

	uint8_t indices[16];
	for (int i = 0; i < 4; ++i)
	{
		uint8_t* ind = indices + 4 * i;
		uint8_t packed = bytes[4 + i];

		ind[3] = packed & 0x3;
		ind[2] = (packed >> 2) & 0x3;
		ind[1] = (packed >> 4) & 0x3;
		ind[0] = (packed >> 6) & 0x3;
	}

	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++)
		{
			uint8_t offset = 4 * indices[y * 4 + x];
			for (int j = 0; j < 4; j++)
			{
				rgba[4 * ((y * texWidth + x)) + (j)] = codes[offset + j];
			}
		}
}


void ReadMaterial(
	const void* AssetPtr,
	struct ModelOutputData* mod,
	struct Material* materials,
	int MaterialNum)
{
	const long MaterialFlags = SwapEndian(*(long*)AssetPtr);

	DEBUG_ASSERT((MaterialFlags & 0x1) != 0);
	DEBUG_ASSERT((MaterialFlags & 0x2) != 0);
	DEBUG_ASSERT((MaterialFlags & 0x4) == 0);

	if (MaterialFlags & 0x80)
		mod->psd[MaterialNum].enableDepth = true;
	else
		mod->psd[MaterialNum].enableDepth = false;

	if (MaterialFlags & 0x10)
	{
		mod->psd[MaterialNum].enableDepth = false;
		mod->pmd[MaterialNum].transparent = true;
	}
	else
		mod->pmd[MaterialNum].transparent = false;
	
	
	if (MaterialFlags & 0x20)
	{
		mod->materials[MaterialNum].pixelCB.AlphaThreshold = .25f;
	}
	else
	{
		mod->materials[MaterialNum].pixelCB.AlphaThreshold = .0f;
	}

	if (MaterialFlags & 0x200)
	{
		mod->pmd[MaterialNum].occlusionMesh = true;
	}
	else
	{
		mod->pmd[MaterialNum].occlusionMesh = false;
	}

	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	long TextureIndexCount = SwapEndian(*(long*)AssetPtr);


	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	const uint32_t* materialTextureIndices = AssetPtr;

	AssetPtr = OffsetPointer(AssetPtr, sizeof(long) * TextureIndexCount);

	long VertexAttributeFlags = SwapEndian(*(long*)AssetPtr);

	int VertexAttributeCount = 0;

	if (VertexAttributeFlags & 0x3) VertexAttributeCount++;
	if (VertexAttributeFlags & 0xC) VertexAttributeCount++;
	if (VertexAttributeFlags & 0x30) VertexAttributeCount++;
	if (VertexAttributeFlags & 0xC0) VertexAttributeCount++;
	if (VertexAttributeFlags & 0x300) VertexAttributeCount++;
	if (VertexAttributeFlags & 0xC00) VertexAttributeCount++;
	if (VertexAttributeFlags & 0x3000) VertexAttributeCount++;
	if (VertexAttributeFlags & 0xC000) VertexAttributeCount++;
	if (VertexAttributeFlags & 0x30000) VertexAttributeCount++;
	if (VertexAttributeFlags & 0xC0000) VertexAttributeCount++;

	if ((VertexAttributeFlags & 0xC) >> 2 == 0)
	{
		DebugBreak();
	}

	if (VertexAttributeFlags & 0x30)
	{
		DebugBreak();
	}

	if (VertexAttributeFlags & 0xC0)
	{
		DebugBreak();
	}

	if (VertexAttributeFlags & 0xC0000)
	{
		DebugBreak();
	}

	if (VertexAttributeFlags & 0x300000)
	{
		DebugBreak();
	}

	if (VertexAttributeCount > 7)
	{
		DebugBreak();
	}

	switch (VertexAttributeCount)
	{
	case 8:
		DEBUG_ASSERT(VertexAttributeFlags == 0b11111111111100001111);
		break;
	case 7:
		DEBUG_ASSERT(VertexAttributeFlags == 0b00111111111100001111);
		break;
	case 6:
		DEBUG_ASSERT(VertexAttributeFlags == 0b00001111111100001111);
		break;
	case 5:
		DEBUG_ASSERT(VertexAttributeFlags == 0b00000011111100001111);
		break;
	case 4:
		DEBUG_ASSERT(VertexAttributeFlags == 0b00000000111100001111);
		break;
	case 3:
		DEBUG_ASSERT(VertexAttributeFlags == 0b00000000001100001111);
		break;
	case 2:
		DEBUG_ASSERT(VertexAttributeFlags == 0b00000000000000001111);
		break;
	}

	materials[MaterialNum].VertexAttributeCount = VertexAttributeCount;
	materials[MaterialNum].UVChannelCount = VertexAttributeCount - 2;

	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	long GroupIndex = SwapEndian(*(long*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));


	mod->materials[MaterialNum].pixelCB.KonstColors[0][0] = .1;
	mod->materials[MaterialNum].pixelCB.KonstColors[0][1] = .1;
	mod->materials[MaterialNum].pixelCB.KonstColors[0][2] = .1;
	mod->materials[MaterialNum].pixelCB.KonstColors[0][3] = .257;

	uint8_t KonstColorsRGBA[4][4] = { 0 };

	if (MaterialFlags & 0x8)
	{
		long KonstCount = SwapEndian(*(long*)AssetPtr);
		AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

		for (int i = 0; i < KonstCount; i++)
		{
			long KonstColor = SwapEndian(*(long*)OffsetPointer(AssetPtr, i * sizeof(long)));

			KonstColorsRGBA[i][0] = (KonstColor & 0xFF000000) >> 24;
			KonstColorsRGBA[i][1] = (KonstColor & 0x00FF0000) >> 16;
			KonstColorsRGBA[i][2] = (KonstColor & 0x0000FF00) >> 8;
			KonstColorsRGBA[i][3] = (KonstColor & 0x000000FF) >> 0;

			mod->materials[MaterialNum].pixelCB.KonstColors[i][0] = KonstColorsRGBA[i][0] / 255.f;
			mod->materials[MaterialNum].pixelCB.KonstColors[i][1] = KonstColorsRGBA[i][1] / 255.f;
			mod->materials[MaterialNum].pixelCB.KonstColors[i][2] = KonstColorsRGBA[i][2] / 255.f;
			mod->materials[MaterialNum].pixelCB.KonstColors[i][3] = KonstColorsRGBA[i][3] / 255.f;
		}

		AssetPtr = OffsetPointer(AssetPtr, sizeof(long) * KonstCount);
	}

	short BlendDestinationFactor = SwapEndian(*(const short*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(short));

	short BlendSourceFactor = SwapEndian(*(const short*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(short));

	if (BlendDestinationFactor == 0 && BlendSourceFactor == 1 && mod->pmd[MaterialNum].transparent)
	{
		DebugBreak();
	}

	mod->psd[MaterialNum].BlendDestinationFactor = BlendDestinationFactor;
	mod->psd[MaterialNum].BlendSourceFactor = BlendSourceFactor;
	
	if (MaterialFlags & 0x400)
	{
		long ReflectionIndirectTextureSlotIndex = SwapEndian(*(long*)AssetPtr);
		AssetPtr = OffsetPointer(AssetPtr, sizeof(long));
	}

	long ColorChannelCount = SwapEndian(*(long*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	long ColorChannelFlags = SwapEndian(*(long*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(long) * ColorChannelCount);

	long TEVStageCount = SwapEndian(*(long*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	mod->materials[MaterialNum].pixelCB.TEVStageCount = TEVStageCount;

	struct TEVStage_gcn
	{
		uint32_t ColorInputFlags;
		uint32_t AlphaInputFlags;
		uint32_t ColorCombineFlags;
		uint32_t AlphaCombineFlags;
		uint8_t Padding;
		uint8_t KonstAlphaInput;
		uint8_t KonstColorInput;
		uint8_t RasterizedColorInput;
	};

	const struct TEVStage_gcn* stages = AssetPtr;

	for (int i = 0; i < TEVStageCount; i++)
	{
		uint32_t colorInputFlags = SwapEndian(stages[i].ColorInputFlags);
		for (int j = 0; j < 4; j++)
		{
			mod->materials[MaterialNum].pixelCB.TEVStages[i].registerInputs[j] = colorInputFlags & 0b11111;
			colorInputFlags = colorInputFlags >> 5;
		}

		uint32_t alphaInputFlags = SwapEndian(stages[i].AlphaInputFlags);

		for (int j = 0; j < 4; j++)
		{
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].registerInputs[j] = alphaInputFlags & 0b11111;
			alphaInputFlags = alphaInputFlags >> 5;
		}

		const uint32_t alphaCombineFlags = SwapEndian(stages[i].AlphaCombineFlags);
		mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].combineOperator = alphaCombineFlags & 0xF;
		
		switch ((alphaCombineFlags & 0x30) >> 4)
		{
		case 0b00:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].bias = 0;
			break;
		case 0b01:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].bias = .5;
			break;
		case 0b10:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].bias = -.5;
			break;
		case 0b11:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].bias = 0;
			break;
		}

		switch ((alphaCombineFlags & 0xC0) >> 6)
		{
		case 0b00:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].scale = 1;
			break;
		case 0b01:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].scale = 2;
			break;
		case 0b10:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].scale = 4;
			break;
		case 0b11:
			mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].scale = .5f;
			break;
		default:
			DebugBreak();
		}

		mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].clamp = (alphaCombineFlags & 0x100) >> 8;
		mod->materials[MaterialNum].pixelCB.TEVAlphaStages[i].outputRegister = (alphaCombineFlags & 0x600) >> 9;


		const uint32_t colorCombineFlags = SwapEndian(stages[i].ColorCombineFlags);
		mod->materials[MaterialNum].pixelCB.TEVStages[i].combineOperator = colorCombineFlags & 0xF;

		switch ((colorCombineFlags & 0x30) >> 4)
		{
		case 0b00:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].bias = 0;
			break;
		case 0b01:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].bias = .5;
			break;
		case 0b10:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].bias = -.5;
			break;
		case 0b11:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].bias = 0;
			break;
		}

		switch ((colorCombineFlags & 0xC0) >> 6)
		{
		case 0b00:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].scale = 1;
			break;
		case 0b01:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].scale = 2;
			break;
		case 0b10:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].scale = 4;
			break;
		case 0b11:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].scale = .5;
			break;
		default:
			DebugBreak();
		}

		mod->materials[MaterialNum].pixelCB.TEVStages[i].clamp = (colorCombineFlags & 0x100) >> 8;
		mod->materials[MaterialNum].pixelCB.TEVStages[i].outputRegister = (colorCombineFlags & 0x600) >> 9;
		mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 124;
		mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 124;
		mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 124;
		mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 23;

		switch (stages[i].KonstAlphaInput)
		{
		case 0x00:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255;
			break;
		case 0x04:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (1.f / 2.f);
			break;
		case 0x06:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (1.f / 4.f);
			break;
		case 0x07:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (1.f / 8.f);
			break;
		case 0x02:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (3.f / 4.f);
			break;
		case 0x05:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (3.f / 8.f);
			break;
		case 0x03:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (5.f / 8.f);
			break;
		case 0x01:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = 255 * (7.f / 8.f);
			break;
		case 0x1C:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[0][3];
			break;
		case 0x18:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[0][2];
			break;
		case 0x14:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[0][1];
			break;
		case 0x10:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[0][0];
			break;
		case 0x1D:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[1][3];
			break;
		case 0x19:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[1][2];
			break;
		case 0x15:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[1][1];
			break;
		case 0x11:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[1][0];
			break;
		case 0x1E:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[2][3];
			break;
		case 0x1A:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[2][2];
			break;
		case 0x16:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[2][1];
			break;
		case 0x12:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[2][0];
			break;
		case 0x1F:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[3][3];
			break;
		case 0x1B:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[3][2];
			break;
		case 0x17:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[3][1];
			break;
		case 0x13:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[3] = KonstColorsRGBA[3][0];
			break;
		default:
			DebugBreak();
		}

		switch (stages[i].KonstColorInput)
		{
		case 0x00:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255;
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255;
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255;
			break;
		case 0x04:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (1.f / 2.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (1.f / 2.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (1.f / 2.f);
			break;
		case 0x06:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (1.f / 4.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (1.f / 4.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (1.f / 4.f);
			break;
		case 0x07:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (1.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (1.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (1.f / 8.f);
			break;
		case 0x02:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (3.f / 4.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (3.f / 4.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (3.f / 4.f);
			break;
		case 0x05:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (3.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (3.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (3.f / 8.f);
			break;
		case 0x03:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (5.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (5.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (5.f / 8.f);
			break;
		case 0x01:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = 255 * (7.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = 255 * (7.f / 8.f);
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = 255 * (7.f / 8.f);
			break;
		case 0x0C:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[0][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[0][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[0][2];
			break;
		case 0x1C:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[0][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[0][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[0][3];
			break;
		case 0x18:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[0][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[0][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[0][2];
			break;
		case 0x14:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[0][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[0][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[0][1];
			break;
		case 0x10:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[0][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[0][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[0][0];
			break;
		case 0x0D:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[1][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[1][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[1][2];
			break;
		case 0x1D:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[1][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[1][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[1][3];
			break;
		case 0x19:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[1][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[1][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[1][2];
			break;
		case 0x15:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[1][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[1][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[1][1];
			break;
		case 0x11:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[1][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[1][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[1][0];
			break;
		case 0x0E:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[2][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[2][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[2][2];
			break;
		case 0x1E:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[2][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[2][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[2][3];
			break;
		case 0x1A:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[2][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[2][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[2][2];
			break;
		case 0x16:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[2][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[2][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[2][1];
			break;
		case 0x12:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[2][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[2][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[2][0];
			break;
		case 0x0F:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[3][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[3][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[3][2];
			break;
		case 0x1F:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[3][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[3][3];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[3][3];
			break;
		case 0x1B:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[3][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[3][2];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[3][2];
			break;
		case 0x17:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[3][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[3][1];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[3][1];
			break;
		case 0x13:
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[0] = KonstColorsRGBA[3][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[1] = KonstColorsRGBA[3][0];
			mod->materials[MaterialNum].pixelCB.TEVStages[i].KonstColor[2] = KonstColorsRGBA[3][0];
			break;
		default:
			DebugBreak();
		}
	}
	AssetPtr = OffsetPointer(AssetPtr, sizeof(struct TEVStage_gcn) * TEVStageCount);

	struct TEVInput_gcn
	{
		uint16_t padding;
		uint8_t TextureTEVInput;
		uint8_t TexCoordTEVInput;
	};

	const struct TEVInput_gcn* TEVinputs = AssetPtr;

	for (int i = 0; i < TEVStageCount; i++)
	{
		if (TEVinputs[i].TextureTEVInput == 255)
		{
			mod->materials[MaterialNum].textureIndices.indices[i] = TEV_NO_TEXTURE_PRESENT;
		}
		else
		{
			mod->materials[MaterialNum].textureIndices.indices[i] = SwapEndian(materialTextureIndices[TEVinputs[i].TextureTEVInput]);

			DEBUG_ASSERT(TEVinputs[i].TextureTEVInput <= TextureIndexCount);
			DEBUG_ASSERT(mod->materials[MaterialNum].textureIndices.indices[i] <= mod->TextureCount);
		}

		mod->materials[MaterialNum].pixelCB.TEVStages[i].texCoordIndex = TEVinputs[i].TexCoordTEVInput;
	}

	AssetPtr = OffsetPointer(AssetPtr, sizeof(struct TEVInput_gcn) * TEVStageCount);

	long TexGenCount = SwapEndian(*(long*)AssetPtr);

	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));
	DEBUG_ASSERT(TexGenCount <= 5);
	mod->materials[MaterialNum].vertexCB.TexgenCount = TexGenCount;

	for (int i = 0; i < TexGenCount; i++)
	{
		long TexGenFlags = SwapEndian(*(long*)AssetPtr);
		mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordGenType = TexGenFlags & 0xF;

		switch ((TexGenFlags & 0x1F0) >> 4)
		{
		case 0:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 0;
			break;
		case 1:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 1;
			break;
		case 4:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 2;
			break;
		case 5:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 3;
			break;
		case 6:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 4;
			break;
		case 7:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 5;
			break;
		case 8:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 6;
			break;
		case 9:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 7;
			break;
		case 10:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 8;
			break;
		case 11:
			mod->materials[MaterialNum].vertexCB.texgens[i].TexCoordSource = 9;
			break;
		default:
			DebugBreak();
		}

		switch (((TexGenFlags & 0x3E00) >> 9) + 30)
		{
		case 60:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 0;
			break;
		case 30:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 1;
			break;
		case 33:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 2;
			break;
		case 36:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 3;
			break;
		case 39:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 4;
			break;
		case 42:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 5;
			break;
		case 45:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 6;
			break;
		case 48:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 7;
			break;
		case 51:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 8;
			break;
		case 54:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 9;
			break;
		case 57:
			mod->materials[MaterialNum].vertexCB.texgens[i].PreTransformIndex = 10;
			break;
		default:
			DebugBreak();
		}

		if ((TexGenFlags & 0x4000) >> 14)
			mod->materials[MaterialNum].vertexCB.texgens[i].bNormalize = 1;
		else
			mod->materials[MaterialNum].vertexCB.texgens[i].bNormalize = 0;

		switch (((TexGenFlags & 0x1F8000) >> 15) + 64)
		{
		case 125:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 0;
			break;
		case 64:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 1;
			break;
		case 67:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 2;
			break;
		case 70:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 3;
			break;
		case 73:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 4;
			break;
		case 76:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 5;
			break;
		case 79:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 6;
			break;
		case 82:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 7;
			break;
		case 85:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 8;
			break;
		case 88:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 9;
			break;
		case 91:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 10;
			break;
		case 94:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 11;
			break;
		case 97:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 12;
			break;
		case 100:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 13;
			break;
		case 103:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 14;
			break;
		case 106:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 15;
			break;
		case 109:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 16;
			break;
		case 112:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 17;
			break;
		case 115:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 18;
			break;
		case 118:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 19;
			break;
		case 121:
			mod->materials[MaterialNum].vertexCB.texgens[i].PostTransformIndex = 20;
			break;
		default:
			DebugBreak();
		}
		AssetPtr = OffsetPointer(AssetPtr, sizeof(long));
	}

	mod->materials[MaterialNum].vertexCB.pretransformMatrices[0][0][0] = 1;
	mod->materials[MaterialNum].vertexCB.pretransformMatrices[0][1][1] = 1;
	mod->materials[MaterialNum].vertexCB.pretransformMatrices[0][2][2] = 1;
	mod->materials[MaterialNum].vertexCB.pretransformMatrices[0][3][3] = 1;

	mod->materials[MaterialNum].vertexCB.posttransformMatrices[0][0][0] = 1;
	mod->materials[MaterialNum].vertexCB.posttransformMatrices[0][1][1] = 1;
	mod->materials[MaterialNum].vertexCB.posttransformMatrices[0][2][2] = 1;
	mod->materials[MaterialNum].vertexCB.posttransformMatrices[0][3][3] = 1;
	
	long MaterialAnimationSectionSize = SwapEndian(*(long*)AssetPtr);
	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	long AnimationCount = SwapEndian(*(long*)AssetPtr);
	mod->pmd[MaterialNum].animationCount = AnimationCount;
	DEBUG_ASSERT(AnimationCount <= 4);

	AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

	for (int i = 0; i < AnimationCount; i++)
	{
		long AnimationType = SwapEndian(*(long*)AssetPtr);
		mod->pmd[MaterialNum].animations[i].mode = AnimationType;
		AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

		switch (AnimationType)
		{
		case 0:
			break;
		case 1:
			break;
		case 2:
		{
			float OffsetA = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float OffsetB = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float ScaleA = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float ScaleB = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			mod->pmd[MaterialNum].animations[i].par1 = OffsetA;
			mod->pmd[MaterialNum].animations[i].par2 = OffsetB;
			mod->pmd[MaterialNum].animations[i].par3 = ScaleA;
			mod->pmd[MaterialNum].animations[i].par4 = ScaleB;
			break;
		}
		case 3:
		{
			float Offset = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float Scale = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));


			mod->pmd[MaterialNum].animations[i].par1 = Offset;
			mod->pmd[MaterialNum].animations[i].par2 = Scale;
			break;

		}
		case 4:
		{
			float Scale = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float FrameCount = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float Step = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float Offset = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));


			mod->pmd[MaterialNum].animations[i].par1 = Scale;
			mod->pmd[MaterialNum].animations[i].par2 = FrameCount;
			mod->pmd[MaterialNum].animations[i].par3 = Step;
			mod->pmd[MaterialNum].animations[i].par4 = Offset;
			break;
		}
		case 5:
		{
			float Scale = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float FrameCount = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float Step = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float Offset = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			mod->pmd[MaterialNum].animations[i].par1 = Scale;
			mod->pmd[MaterialNum].animations[i].par2 = FrameCount;
			mod->pmd[MaterialNum].animations[i].par3 = Step;
			mod->pmd[MaterialNum].animations[i].par4 = Offset;
			
			break;
		}
		case 6:
			break;
		case 7:
		{
			float par1 = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			float par2 = SwapEndianFloat(*(float*)AssetPtr);
			AssetPtr = OffsetPointer(AssetPtr, sizeof(float));

			mod->pmd[MaterialNum].animations[i].par1 = par1;
			mod->pmd[MaterialNum].animations[i].par2 = par2;
			break;
		}
		default:
			DebugBreak();
		}
	}
}

const void* ReadGeometry(
	const void* FileHead,
	const void* DataSectionPtr,
	const uint32_t* SectionSizeTable,
	int* ResidentSection,
	struct ModelOutputData* mod,
	struct Material* materials,
	int* totalVertexCount,
	int* totalIndexCount,
	int* totalSurfaceCount,
	bool useShortUVs)
{
	const float* NativeVertexBuffer = DataSectionPtr;

	DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
	(*ResidentSection)++;

	const float* NativeNormalBuffer = DataSectionPtr;
	const short* NativeNormalBufferShorts = DataSectionPtr;
	DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
	(*ResidentSection)++;

	DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
	(*ResidentSection)++;

	const float* NativeVertexUVBuffer = DataSectionPtr;

	DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
	(*ResidentSection)++;

	const uint16_t* NativeVertexUVBufferShorts = DataSectionPtr;

	if (useShortUVs)
	{
		DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
		(*ResidentSection)++;
	}

	uint32_t SurfaceCount = SwapEndian(*(uint32_t*)DataSectionPtr);

	const uint32_t* SurfaceEndOffsets = OffsetPointer(DataSectionPtr, sizeof(SurfaceCount));

	DEBUG_ASSERT(SurfaceCount < 1000 && SurfaceCount != 0);

	DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
	const void* AssetPtr = DataSectionPtr;
	(*ResidentSection)++;

	const void* FirstSurface = DataSectionPtr;

	int vertexNumber = *totalVertexCount;
	int indexNumber = *totalIndexCount;

	mod->surfaceCount = SurfaceCount;
	int ultraTemp = 0;
	for (int SurfaceNum = 0; SurfaceNum < SurfaceCount; SurfaceNum++, (*totalSurfaceCount)++)
	{
		mod->pod[*totalSurfaceCount].centroid[0] = SwapEndianFloat(((float*)AssetPtr)[0]);
		mod->pod[*totalSurfaceCount].centroid[1] = SwapEndianFloat(((float*)AssetPtr)[1]);
		mod->pod[*totalSurfaceCount].centroid[2] = SwapEndianFloat(((float*)AssetPtr)[2]);

		AssetPtr = OffsetPointer(AssetPtr, sizeof(float) * 3);

		uint32_t MaterialIndex = SwapEndian(*(uint32_t*)AssetPtr);

		DEBUG_ASSERT(MaterialIndex <= mod->materialCount);

		mod->pod[*totalSurfaceCount].materialIndex = MaterialIndex;

		AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

		uint32_t DisplayListSize = 0x7FFFFFFF & SwapEndian(*(uint32_t*)AssetPtr);


		AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

		AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

		AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

		const uint32_t ExtraDataSize = SwapEndian(*(uint32_t*)AssetPtr);

		AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

		AssetPtr = OffsetPointer(AssetPtr, sizeof(float) * 3);

		AssetPtr = OffsetPointer(AssetPtr, ExtraDataSize);

		AssetPtr = OffsetPointer(FileHead, pad((((char*)AssetPtr) - ((char*)FileHead)), 32));

		const void* StartOfDisplayList = AssetPtr;

		while (true)
		{
			char FormatBitmap = *(char*)AssetPtr;

			AssetPtr = OffsetPointer(AssetPtr, sizeof(FormatBitmap));

			uint16_t VertexCount = SwapEndian(*(uint16_t*)AssetPtr);

			if (VertexCount == 0)
			{
				break;
			}

			AssetPtr = OffsetPointer(AssetPtr, sizeof(VertexCount));

			const uint16_t* VertexPointer = AssetPtr;

			bool UseShortUVs = false;
			bool useShortNormals = false;

			switch (FormatBitmap & 0b111)
			{
			case 0:
				UseShortUVs = false;
				useShortNormals = false;
				break;
			case 1:
				UseShortUVs = false;
				useShortNormals = true;
				break;
			case 2:
				UseShortUVs = true;
				useShortNormals = true;
				break;
			default:
				RaiseFailFastException(nullptr, nullptr, 0);
			}

			switch (FormatBitmap & 0b11111000)
			{
			case 0x90:
				for (int i = 0; i < VertexCount; i += 3)
				{
					for (int j = 0; j < 3; j++)
					{
						struct VertexData Vertex = { 0 };
						
						int positionIndex = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount]);
						int normalIndex = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount + 1]);
						int uv0Index = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount + 2]);
						int uv1Index = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount + 3]);
						int uv2Index = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount + 4]);
						int uv3Index = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount + 5]);
						int uv4Index = SwapEndian(VertexPointer[(i + j) * materials[MaterialIndex].VertexAttributeCount + 6]);

						Vertex.posx = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 0]);
						Vertex.posy = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 1]);
						Vertex.posz = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 2]);

						if (useShortNormals)
						{
							Vertex.nrmx = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 0]) / (float)(0x8000);
							Vertex.nrmy = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 1]) / (float)(0x8000);
							Vertex.nrmz = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 2]) / (float)(0x8000);
						}
						else
						{
							Vertex.nrmx = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 0]);
							Vertex.nrmy = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 1]);
							Vertex.nrmz = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 2]);
						}
						switch (materials[MaterialIndex].VertexAttributeCount)
						{
						case 8:
							DebugBreak();
						case 7:
							Vertex.tex4u = SwapEndianFloat(NativeVertexUVBuffer[uv4Index * 2 + 0]);
							Vertex.tex4v = SwapEndianFloat(NativeVertexUVBuffer[uv4Index * 2 + 1]);
						case 6:
							Vertex.tex3u = SwapEndianFloat(NativeVertexUVBuffer[uv3Index * 2 + 0]);
							Vertex.tex3v = SwapEndianFloat(NativeVertexUVBuffer[uv3Index * 2 + 1]);
						case 5:
							Vertex.tex2u = SwapEndianFloat(NativeVertexUVBuffer[uv2Index * 2 + 0]);
							Vertex.tex2v = SwapEndianFloat(NativeVertexUVBuffer[uv2Index * 2 + 1]);
						case 4:
							Vertex.tex1u = SwapEndianFloat(NativeVertexUVBuffer[uv1Index * 2 + 0]);
							Vertex.tex1v = SwapEndianFloat(NativeVertexUVBuffer[uv1Index * 2 + 1]);
						case 3:
							if (UseShortUVs)
							{
								Vertex.tex0u = (float)SwapEndian(NativeVertexUVBufferShorts[uv0Index * 2 + 0]) / 0x8000;
								Vertex.tex0v = (float)SwapEndian(NativeVertexUVBufferShorts[uv0Index * 2 + 1]) / 0x8000;
							}
							else
							{
								Vertex.tex0u = SwapEndianFloat(NativeVertexUVBuffer[uv0Index * 2 + 0]);
								Vertex.tex0v = SwapEndianFloat(NativeVertexUVBuffer[uv0Index * 2 + 1]);
							}
						default:
							;
						}

						bool alreadyInVertexList = false;

						for (int i = *totalVertexCount; i < (vertexNumber - 1); i++)
						{
							if (memcmp(&mod->VertexBuffer[i], &Vertex, (materials[MaterialIndex].VertexAttributeCount * 2 + 2) * sizeof(float)) == 0)
							{
								alreadyInVertexList = true;
								mod->IndexBuffer[indexNumber] = i;
								indexNumber++;
								break;
							}
						}

						if (!alreadyInVertexList)
						{
							mod->VertexBuffer[vertexNumber] = Vertex;
							mod->IndexBuffer[indexNumber] = vertexNumber;
							vertexNumber++;
							indexNumber++;
						}
					}

				}
				break;
			case 0x98:
				for (int i = 0; i < VertexCount - 2; i++)
				{
					int vertexPointerOffsets[3];

					if (i % 2 == 0)
					{
						vertexPointerOffsets[0] = i;
						vertexPointerOffsets[1] = i + 1;
						vertexPointerOffsets[2] = i + 2;
					}
					else
					{
						vertexPointerOffsets[0] = i;
						vertexPointerOffsets[1] = i + 2;
						vertexPointerOffsets[2] = i + 1;
					}

					for (int j = 0; j < 3; j++)
					{
						struct VertexData Vertex = { 0 };

						int positionIndex = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount]);
						int normalIndex = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount + 1]);
						int uv0Index = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount + 2]);
						int uv1Index = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount + 3]);
						int uv2Index = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount + 4]);
						int uv3Index = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount + 5]);
						int uv4Index = SwapEndian(VertexPointer[vertexPointerOffsets[j] * materials[MaterialIndex].VertexAttributeCount + 6]);

						Vertex.posx = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 0]);
						Vertex.posy = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 1]);
						Vertex.posz = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 2]);

						if (useShortNormals)
						{
							Vertex.nrmx = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 0]) / (float)(0x8000);
							Vertex.nrmy = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 1]) / (float)(0x8000);
							Vertex.nrmz = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 2]) / (float)(0x8000);
						}
						else
						{
							Vertex.nrmx = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 0]);
							Vertex.nrmy = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 1]);
							Vertex.nrmz = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 2]);
						}
						switch (materials[MaterialIndex].VertexAttributeCount)
						{
						case 8:
							DebugBreak();
						case 7:
							Vertex.tex4u = SwapEndianFloat(NativeVertexUVBuffer[uv4Index * 2 + 0]);
							Vertex.tex4v = SwapEndianFloat(NativeVertexUVBuffer[uv4Index * 2 + 1]);
						case 6:
							Vertex.tex3u = SwapEndianFloat(NativeVertexUVBuffer[uv3Index * 2 + 0]);
							Vertex.tex3v = SwapEndianFloat(NativeVertexUVBuffer[uv3Index * 2 + 1]);
						case 5:
							Vertex.tex2u = SwapEndianFloat(NativeVertexUVBuffer[uv2Index * 2 + 0]);
							Vertex.tex2v = SwapEndianFloat(NativeVertexUVBuffer[uv2Index * 2 + 1]);
						case 4:
							Vertex.tex1u = SwapEndianFloat(NativeVertexUVBuffer[uv1Index * 2 + 0]);
							Vertex.tex1v = SwapEndianFloat(NativeVertexUVBuffer[uv1Index * 2 + 1]);
						case 3:
							if (UseShortUVs)
							{
								Vertex.tex0u = (float)SwapEndian(NativeVertexUVBufferShorts[uv0Index * 2 + 0]) / 0x8000;
								Vertex.tex0v = (float)SwapEndian(NativeVertexUVBufferShorts[uv0Index * 2 + 1]) / 0x8000;
							}
							else
							{
								Vertex.tex0u = SwapEndianFloat(NativeVertexUVBuffer[uv0Index * 2 + 0]);
								Vertex.tex0v = SwapEndianFloat(NativeVertexUVBuffer[uv0Index * 2 + 1]);
							}
						default:
							;
						}

						bool alreadyInVertexList = false;

						for (int i = *totalVertexCount; i < vertexNumber; i++)
						{
							if (memcmp(&mod->VertexBuffer[i], &Vertex, (materials[MaterialIndex].VertexAttributeCount * 2 + 2) * sizeof(float)) == 0)
							{
								alreadyInVertexList = true;
								mod->IndexBuffer[indexNumber] = i;
								indexNumber++;
								break;
							}
						}

						if (!alreadyInVertexList)
						{
							mod->VertexBuffer[vertexNumber] = Vertex;
							mod->IndexBuffer[indexNumber] = vertexNumber;
							vertexNumber++;
							indexNumber++;
						}
					}
				}
				break;
			case 0xA0:
				for (int i = 2; i < VertexCount; i++)
				{
					int vertexPointerOffsets[3] = { 0, i - 1, i };

					for (int j = 0; j < 3; j++)
					{
						struct VertexData Vertex = { 0 };
						int positionIndex = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount]);
						int normalIndex = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount + 1]);
						int uv0Index = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount + 2]);
						int uv1Index = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount + 3]);
						int uv2Index = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount + 4]);
						int uv3Index = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount + 5]);
						int uv4Index = SwapEndian(VertexPointer[(vertexPointerOffsets[j]) * materials[MaterialIndex].VertexAttributeCount + 6]);

						Vertex.posx = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 0]);
						Vertex.posy = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 1]);
						Vertex.posz = SwapEndianFloat(NativeVertexBuffer[positionIndex * 3 + 2]);

						if (useShortNormals)
						{
							Vertex.nrmx = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 0]) / (float)(0x8000);
							Vertex.nrmy = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 1]) / (float)(0x8000);
							Vertex.nrmz = SwapEndian(NativeNormalBufferShorts[normalIndex * 3 + 2]) / (float)(0x8000);
						}
						else
						{
							Vertex.nrmx = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 0]);
							Vertex.nrmy = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 1]);
							Vertex.nrmz = SwapEndianFloat(NativeNormalBuffer[normalIndex * 3 + 2]);
						}
						switch (materials[MaterialIndex].VertexAttributeCount)
						{
						case 8:
							DebugBreak();
						case 7:
							Vertex.tex4u = SwapEndianFloat(NativeVertexUVBuffer[uv4Index * 2 + 0]);
							Vertex.tex4v = SwapEndianFloat(NativeVertexUVBuffer[uv4Index * 2 + 1]);
						case 6:
							Vertex.tex3u = SwapEndianFloat(NativeVertexUVBuffer[uv3Index * 2 + 0]);
							Vertex.tex3v = SwapEndianFloat(NativeVertexUVBuffer[uv3Index * 2 + 1]);
						case 5:
							Vertex.tex2u = SwapEndianFloat(NativeVertexUVBuffer[uv2Index * 2 + 0]);
							Vertex.tex2v = SwapEndianFloat(NativeVertexUVBuffer[uv2Index * 2 + 1]);
						case 4:
							Vertex.tex1u = SwapEndianFloat(NativeVertexUVBuffer[uv1Index * 2 + 0]);
							Vertex.tex1v = SwapEndianFloat(NativeVertexUVBuffer[uv1Index * 2 + 1]);
						case 3:
							if (UseShortUVs)
							{
								Vertex.tex0u = (float)SwapEndian(NativeVertexUVBufferShorts[uv0Index * 2 + 0]) / 0x8000;
								Vertex.tex0v = (float)SwapEndian(NativeVertexUVBufferShorts[uv0Index * 2 + 1]) / 0x8000;
							}
							else
							{
								Vertex.tex0u = SwapEndianFloat(NativeVertexUVBuffer[uv0Index * 2 + 0]);
								Vertex.tex0v = SwapEndianFloat(NativeVertexUVBuffer[uv0Index * 2 + 1]);
							}
						default:
							;
						}

						bool alreadyInVertexList = false;
						
						for (int i = *totalVertexCount; i < vertexNumber; i++)
						{
							if (memcmp(&mod->VertexBuffer[i], &Vertex, (materials[MaterialIndex].VertexAttributeCount * 2 + 2) * sizeof(float)) == 0)
							{
								alreadyInVertexList = true;
								mod->IndexBuffer[indexNumber] = i;
								indexNumber++;
								break;
							}
						}

						if (!alreadyInVertexList)
						{
							mod->VertexBuffer[vertexNumber] = Vertex;
							mod->IndexBuffer[indexNumber] = vertexNumber;
							vertexNumber++;
							indexNumber++;
						}
					}
				}
				break;
			default:
				DebugBreak();
				break;
			}

			AssetPtr = OffsetPointer(AssetPtr, sizeof(uint16_t) * VertexCount * materials[MaterialIndex].VertexAttributeCount);

			uint32_t DataProcessed = ((uintptr_t)AssetPtr - (uintptr_t)StartOfDisplayList);

			if (DisplayListSize - DataProcessed < 15)
			{
				break;
			}
		}

		mod->pod[*totalSurfaceCount].surfaceOffset = *totalIndexCount;
		mod->pod[*totalSurfaceCount].surfaceSize = indexNumber - *totalIndexCount;
		

		*totalVertexCount = vertexNumber;
		*totalIndexCount = indexNumber;

		AssetPtr = OffsetPointer(FirstSurface, SwapEndian(SurfaceEndOffsets[SurfaceNum]));
		ultraTemp++;
	}

	for (int i = 0; i < SurfaceCount; i++)
	{
		DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[*ResidentSection]));
		(*ResidentSection)++;
	}
	if (ultraTemp != SurfaceCount)
	{
		DebugBreak();
	}

	*totalVertexCount = vertexNumber;
	*totalIndexCount = indexNumber;

	return DataSectionPtr;
}

uint32_t GetNamedAssetID(const void* _In_ pPak, const char* _In_ name)
{
	struct Pak_Header
	{
		int16_t VersionMajor;
		int16_t VersionMinor;
		int32_t Unused;
	};

	struct Pak_NamedResource
	{
		FourCC AssetType;
		int32_t AssetID;
		int32_t NameLength;
		char Name;
	};

	const struct Pak_Header* Header = pPak;

	pPak = OffsetPointer(pPak, sizeof(struct Pak_Header));

	const int32_t NamedResourceCount = SwapEndian(*(int32_t*)pPak);

	pPak = OffsetPointer(pPak, sizeof(int32_t));

	for (int i = 0; i < NamedResourceCount; i++)
	{
		const struct Pak_NamedResource* NamedResource = pPak;

		if (memcmp(name, &NamedResource->Name, strlen(name)) == 0)
			return SwapEndian(NamedResource->AssetID);
		
		pPak = OffsetPointer(pPak,
			sizeof(NamedResource->AssetType) +
			sizeof(NamedResource->AssetID) +
			sizeof(NamedResource->NameLength) +
			SwapEndian(NamedResource->NameLength));
	}

	return 0;
}

void ReadPak(const void* _In_ pPak, int32_t AssetID, void** output)
{
	struct Pak_Header
	{
		int16_t VersionMajor;
		int16_t VersionMinor;
		int32_t Unused;
	};

	struct Pak_NamedResource
	{
		FourCC AssetType;
		int32_t AssetID;
		int32_t NameLength;
		char Name;
	};

	struct Pak_UnnamedResource
	{
		int32_t CompressionFlag;
		FourCC AssetType;
		int32_t AssetID;
		int32_t Size;
		int32_t Offset;
	};

	const struct Pak_Header* Header = pPak;

	pPak = OffsetPointer(pPak, sizeof(struct Pak_Header));

	const int32_t NamedResourceCount = SwapEndian(*((int32_t*)pPak));

	pPak = OffsetPointer(pPak, sizeof(NamedResourceCount));

	for (int i = 0; i < NamedResourceCount; i++)
	{
		const struct Pak_NamedResource* NamedResource = pPak;
		
		pPak = OffsetPointer(pPak,
			sizeof(NamedResource->AssetType) +
			sizeof(NamedResource->AssetID) +
			sizeof(NamedResource->NameLength) +
			SwapEndian(NamedResource->NameLength));
	}

	const int32_t UnnamedResourceCount = SwapEndian(*((int32_t*)pPak));

	pPak = OffsetPointer(pPak, sizeof(UnnamedResourceCount));

	const struct Pak_UnnamedResource* UnnamedResourceTable = pPak;

	for (int i = 0; i < UnnamedResourceCount; i++)
	{
		if (SwapEndian(UnnamedResourceTable[i].AssetID) == AssetID)
		{
			void* AssetPtr;
			if (SwapEndian(UnnamedResourceTable[i].CompressionFlag))
			{
				const uint32_t* DecompressedSize = OffsetPointer(Header, SwapEndian(UnnamedResourceTable[i].Offset));

				void* AssetPtrDecompressed = malloc(SwapEndian(*DecompressedSize));

				uint32_t DecompressedSize_MUT = *DecompressedSize;

				if (uncompress(AssetPtrDecompressed, &DecompressedSize_MUT, DecompressedSize + 1, SwapEndian(UnnamedResourceTable[i].Size) - 4) != Z_OK)
				{
					DebugBreak();
				}

				AssetPtr = AssetPtrDecompressed;
			}
			else
			{
				AssetPtr = OffsetPointer(Header, SwapEndian(UnnamedResourceTable[i].Offset));
			}

			if (((char*)&UnnamedResourceTable[i].AssetType)[0] == 'S' &&
				((char*)&UnnamedResourceTable[i].AssetType)[1] == 'T' &&
				((char*)&UnnamedResourceTable[i].AssetType)[2] == 'R' &&
				((char*)&UnnamedResourceTable[i].AssetType)[3] == 'G')
			{
				struct STRG_LANG
				{
					FourCC LanguageID;
					FourCC LanguageStringsOffset;
				};

				struct STRG_TABLE
				{
					uint32_t StringTableSize;
					uint32_t StringOffsets;
					wchar_t StringArray;
				};

				struct STRG
				{
					uint32_t Magic;
					uint32_t Version;
					uint32_t LanguageCount;
					uint32_t StringCount;
					struct STRG_LANG Languages;
					struct STRG_TABLE StringTable;
				};

				const struct STRG* StrgAsset = AssetPtr;

				const uint32_t LanguageCount = SwapEndian(StrgAsset->LanguageCount);

				const uint32_t StringCount = SwapEndian(StrgAsset->StringCount);

				StrgAsset = OffsetPointer(StrgAsset, (LanguageCount - 1) * sizeof(StrgAsset->Languages));
			}
			else if (
				((char*)&UnnamedResourceTable[i].AssetType)[0] == 'T' &&
				((char*)&UnnamedResourceTable[i].AssetType)[1] == 'X' &&
				((char*)&UnnamedResourceTable[i].AssetType)[2] == 'T' &&
				((char*)&UnnamedResourceTable[i].AssetType)[3] == 'R')
			{
				struct TXTR
				{
					uint32_t ImageFormat;
					uint16_t Width;
					uint16_t Height;
					uint32_t MipCount;
				};

				const struct TXTR* TxtrAsset = AssetPtr;

				const uint16_t ImageWidth = SwapEndian(TxtrAsset->Width);

				const uint16_t ImageHeight = SwapEndian(TxtrAsset->Height);

				*output = malloc(ImageWidth * ImageHeight * 4 + sizeof(ImageWidth) + sizeof(ImageHeight));

				struct TextureOutputData* outputData = *output;
				outputData->Width = ImageWidth;
				outputData->Height = ImageHeight;
				const uint8_t* srcPixels = OffsetPointer(AssetPtr, sizeof(struct TXTR));

				switch (SwapEndian(TxtrAsset->ImageFormat))
				{
				case 0xA:
				{

					uint32_t ByteInBlock = 0;

					for (int y = 0; y < ImageHeight; y += 8)
					{
						for (int x = 0; x < ImageWidth; x += 8)
						{
							DecompressColorGCN(ImageWidth, OffsetPointer(&outputData->ImageData, 4 * (y * ImageWidth + x)), &srcPixels[ByteInBlock]);
							ByteInBlock += 8;
							DecompressColorGCN(ImageWidth, OffsetPointer(&outputData->ImageData, 4 * ((y)*ImageWidth + (x + 4))), &srcPixels[ByteInBlock]);
							ByteInBlock += 8;
							DecompressColorGCN(ImageWidth, OffsetPointer(&outputData->ImageData, 4 * ((y + 4) * ImageWidth + (x))), &srcPixels[ByteInBlock]);
							ByteInBlock += 8;
							DecompressColorGCN(ImageWidth, OffsetPointer(&outputData->ImageData, 4 * ((y + 4) * ImageWidth + (x + 4))), &srcPixels[ByteInBlock]);
							ByteInBlock += 8;
						}
					}
					break;
				}
				case 0x8:
				{
					int inputPixelIndex = 0;
					for (int y = 0; y < ImageHeight; y += 4)
					{
						for (int x = 0; x < ImageWidth; x += 4)
						{
							for (int j = 0; j < 4; j++)
							{
								for (int k = 0; k < 4; k++)
								{
									uint16_t pixel = SwapEndian(*OffsetPointer((uint16_t*)srcPixels, inputPixelIndex * sizeof(uint16_t)));

									uint32_t outputPixel = 0;
									if (pixel & 0b1000000000000000)
									{
										uint8_t pixelB = ((pixel & 0b0000000000011111) >> 0) * 0x8;
										uint8_t pixelG = ((pixel & 0b0000001111100000) >> 5) * 0x8;
										uint8_t pixelR = ((pixel & 0b0111110000000000) >> 10) * 0x8;

										*OffsetPointer((uint8_t*)&outputPixel, 0) = pixelR;
										*OffsetPointer((uint8_t*)&outputPixel, 1) = pixelG;
										*OffsetPointer((uint8_t*)&outputPixel, 2) = pixelB;
										*OffsetPointer((uint8_t*)&outputPixel, 3) = 255;
									}
									else
									{
										uint8_t pixelB = ((pixel & 0b0000000000001111) >> 0) * 0x11;
										uint8_t pixelG = ((pixel & 0b0000000011110000) >> 4) * 0x11;
										uint8_t pixelR = ((pixel & 0b0000111100000000) >> 8) * 0x11;
										uint8_t pixelA = ((pixel & 0b0111000000000000) >> 12) * 0x20;

										*OffsetPointer((uint8_t*)&outputPixel, 0) = pixelR;
										*OffsetPointer((uint8_t*)&outputPixel, 1) = pixelG;
										*OffsetPointer((uint8_t*)&outputPixel, 2) = pixelB;
										*OffsetPointer((uint8_t*)&outputPixel, 3) = pixelA;
									}
									*((uint32_t*)&OffsetPointer(outputData, 4 * (((y + j) * ImageWidth + x) + k))->ImageData) = outputPixel;
									inputPixelIndex++;
								}
							}
						}
					}
					break;
				}
				case 0x3:
				{
					int inputPixelIndex = 0;
					for (int y = 0; y < ImageHeight; y += 4)
					{
						for (int x = 0; x < ImageWidth; x += 4)
						{
							for (int j = 0; j < 4; j++)
							{
								for (int k = 0; k < 4; k++)
								{
									uint16_t pixel = SwapEndian(*OffsetPointer((uint16_t*)srcPixels, inputPixelIndex * sizeof(uint16_t)));

									uint32_t outputPixel = 0;
									

									*OffsetPointer((uint8_t*)&outputPixel, 0) = pixel & 0xFF;
									*OffsetPointer((uint8_t*)&outputPixel, 1) = pixel & 0xFF;
									*OffsetPointer((uint8_t*)&outputPixel, 2) = pixel & 0xFF;
									*OffsetPointer((uint8_t*)&outputPixel, 3) = (pixel & 0xFF00) >> 8;
									
									*((uint32_t*)&OffsetPointer(outputData, 4 * (((y + j) * ImageWidth + x) + k))->ImageData) = outputPixel;
									inputPixelIndex++;
								}
							}
						}
					}
					break;
				}
				case 0x0:
				{
					int inputPixelIndex = 0;
					for (int y = 0; y < ImageHeight; y += 8)
					{
						for (int x = 0; x < ImageWidth; x += 8)
						{
							for (int j = 0; j < 8; j++)
							{
								for (int k = 0; k < 8; k += 2)
								{
									uint8_t pixel = *OffsetPointer((uint8_t*)srcPixels, inputPixelIndex * sizeof(uint8_t));

									uint32_t outputPixel = 0;

									*OffsetPointer((uint8_t*)&outputPixel, 0) = (pixel & 0b11110000) >> 4;
									*OffsetPointer((uint8_t*)&outputPixel, 1) = (pixel & 0b11110000) >> 4;
									*OffsetPointer((uint8_t*)&outputPixel, 2) = (pixel & 0b11110000) >> 4;
									*OffsetPointer((uint8_t*)&outputPixel, 3) = 0xFF;

									*((uint32_t*)&OffsetPointer(outputData, 4 * (((y + j) * ImageWidth + x) + k))->ImageData) = outputPixel;

									*OffsetPointer((uint8_t*)&outputPixel, 0) = (pixel & 0b00001111);
									*OffsetPointer((uint8_t*)&outputPixel, 1) = (pixel & 0b00001111);
									*OffsetPointer((uint8_t*)&outputPixel, 2) = (pixel & 0b00001111);
									*OffsetPointer((uint8_t*)&outputPixel, 3) = 0xFF;


									*((uint32_t*)&OffsetPointer(outputData, 4 * (((y + j) * ImageWidth + x) + k + 1))->ImageData) = outputPixel;
									inputPixelIndex++;
								}
							}
						}
					}
					break;
				}
				case 0x2:
				{
					int inputPixelIndex = 0;
					for (int y = 0; y < ImageHeight; y += 4)
					{
						for (int x = 0; x < ImageWidth; x += 8)
						{
							for (int j = 0; j < 4; j++)
							{
								for (int k = 0; k < 8; k++)
								{
									uint8_t pixel = *OffsetPointer((uint8_t*)srcPixels, inputPixelIndex * sizeof(uint8_t));

									uint32_t outputPixel = 0;

									uint8_t grayScale = (pixel & 0b00001111) * 0x11;
									uint8_t alpha = ((pixel & 0b11110000) >> 4) * 0x11;

									*OffsetPointer((uint8_t*)&outputPixel, 0) = grayScale;
									*OffsetPointer((uint8_t*)&outputPixel, 1) = grayScale;
									*OffsetPointer((uint8_t*)&outputPixel, 2) = grayScale;
									*OffsetPointer((uint8_t*)&outputPixel, 3) = alpha;
									
									*((uint32_t*)&OffsetPointer(outputData, 4 * (((y + j) * ImageWidth + x) + k))->ImageData) = outputPixel;
									inputPixelIndex++;
								}
							}
						}
					}
					break;
				}
				case 0x7:
				{
					int inputPixelIndex = 0;
					for (int y = 0; y < ImageHeight; y += 4)
					{
						for (int x = 0; x < ImageWidth; x += 4)
						{
							for (int j = 0; j < 4; j++)
							{
								for (int k = 0; k < 4; k++)
								{
									uint16_t pixel = *OffsetPointer((uint16_t*)srcPixels, inputPixelIndex * sizeof(uint16_t));

									uint32_t outputPixel = 0;
									Unpack565(&pixel, &outputPixel);


									*((uint32_t*)&OffsetPointer(outputData, 4 * (((y + j) * ImageWidth + x) + k))->ImageData) = outputPixel;
									inputPixelIndex++;
								}
							}
						}
					}
					break;
				}
				default:
					DebugBreak();
				}
			}
			else if (
				((char*)&UnnamedResourceTable[i].AssetType)[0] == 'C' &&
				((char*)&UnnamedResourceTable[i].AssetType)[1] == 'M' &&
				((char*)&UnnamedResourceTable[i].AssetType)[2] == 'D' &&
				((char*)&UnnamedResourceTable[i].AssetType)[3] == 'L')
			{
				struct ModelOutputData* mod;

				{
					const int NumberOfTextureIDs = 57;
					const int NumberOfSurfaces = 68;
					const int NumberOfMaterials = 36;

					const SIZE_T requiredMemory =
						sizeof(struct ModelOutputData) +
						sizeof(uint32_t) * NumberOfTextureIDs +
						sizeof(struct PerSurfaceData) * NumberOfSurfaces +
						sizeof(struct PerMaterialData) * NumberOfMaterials +
						sizeof(struct PipelineStateData) * NumberOfMaterials +
						sizeof(struct MaterialConstantBuffers) * NumberOfMaterials;

					*output = VirtualAlloc(
						nullptr,
						requiredMemory,
						MEM_COMMIT | MEM_RESERVE,
						PAGE_READWRITE
					);

					mod = *output;

					mod->textureID = OffsetPointer(*output, sizeof(struct ModelOutputData));
					mod->pod = OffsetPointer(mod->textureID, sizeof(int) * NumberOfTextureIDs);
					mod->pmd = OffsetPointer(mod->pod, sizeof(struct PerSurfaceData) * NumberOfSurfaces);
					mod->psd = OffsetPointer(mod->pmd, sizeof(struct PerMaterialData) * NumberOfMaterials);
					mod->materials = OffsetPointer(mod->psd, sizeof(struct PipelineStateData) * NumberOfMaterials);

					const int maxVerts = 1024 * 1024;

					mod->VertexBuffer = VirtualAlloc(
						nullptr,
						maxVerts * sizeof(struct VertexData),
						MEM_RESERVE,
						PAGE_READWRITE
					);

					mod->IndexBuffer = VirtualAlloc(
						nullptr,
						maxVerts * sizeof(uint32_t),
						MEM_RESERVE,
						PAGE_READWRITE
					);
				}
				struct CMDL
				{
					uint32_t Magic;
					uint32_t Version;
					uint32_t Flags;
					float MAABB[6];
					uint32_t DataSectionCount;
					uint32_t MaterialSetCount;
				};

				const struct CMDL* CmdlAsset = AssetPtr;

				DEBUG_ASSERT(SwapEndian(CmdlAsset->Magic) == 0xDEADBABE);

				bool useShortUVs = SwapEndian(CmdlAsset->Flags) & 0x4;

				uint32_t DataSectionCount = SwapEndian(CmdlAsset->DataSectionCount);
				const uint32_t MaterialSetCount = SwapEndian(CmdlAsset->MaterialSetCount);

				AssetPtr = OffsetPointer(AssetPtr, sizeof(struct CMDL));

				const uint32_t* SectionSizeTable = AssetPtr;


				AssetPtr = OffsetPointer(AssetPtr, DataSectionCount * 4);

				AssetPtr = OffsetPointer(CmdlAsset, pad((((char*)AssetPtr) - ((char*)CmdlAsset)), 32));
				
				int ResidentSection = 0;
				const void* DataSectionPtr = AssetPtr;

				struct Material materials[1024];

				int totalTextureCount = 0;
				int TotalMaterialCount = 0;

				for(int MaterialSetNum = 0; MaterialSetNum < MaterialSetCount; MaterialSetNum++)
				{
					long TextureCount = SwapEndian(*(long*)AssetPtr);

					mod->TextureCount = TextureCount;
					AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

					const long* TextureIDTable = AssetPtr;
					for (int i = 0; i < TextureCount; i++)
					{
						mod->textureID[totalTextureCount] = SwapEndian(TextureIDTable[i]);
						totalTextureCount++;
					}

					AssetPtr = OffsetPointer(AssetPtr, TextureCount * sizeof(long));

					long MaterialCount = SwapEndian(*(long*)AssetPtr);
					mod->materialCount = MaterialCount;
					AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

					const long* MaterialEndOffsets = AssetPtr;

					AssetPtr = OffsetPointer(AssetPtr, MaterialCount * sizeof(long));

					const void* StartOfFirstMaterial = AssetPtr;


					for (int MaterialNum = 0; MaterialNum < MaterialCount; MaterialNum++)
					{
						ReadMaterial(AssetPtr, mod, materials, TotalMaterialCount);
						TotalMaterialCount++;
						AssetPtr = OffsetPointer(StartOfFirstMaterial, SwapEndian(MaterialEndOffsets[MaterialNum]));
					}

					DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[ResidentSection]));
					AssetPtr = DataSectionPtr;
					ResidentSection++;
				}

				mod->TextureCount = totalTextureCount;
				mod->materialCount = TotalMaterialCount;

				int totalVertexCount = 0;
				int totalIndexCount = 0;
				int totalSurfaceCount = 0;

				void* handler = AddVectoredExceptionHandler(0, PageFaultDetector);
				VALIDATE_HANDLE(handler);
				AssetPtr = ReadGeometry(CmdlAsset, DataSectionPtr, SectionSizeTable, &ResidentSection, mod, materials, &totalVertexCount, &totalIndexCount, &totalSurfaceCount, useShortUVs);
				DEBUG_ASSERT(RemoveVectoredExceptionHandler(handler));
				
				DataSectionPtr = AssetPtr;
				

				mod->VertexCount = totalVertexCount;
				mod->IndexCount = totalIndexCount;
				mod->surfaceCount = totalSurfaceCount;

				AssetPtr = CmdlAsset;
			}
			else if (
				((char*)&UnnamedResourceTable[i].AssetType)[0] == 'M' &&
				((char*)&UnnamedResourceTable[i].AssetType)[1] == 'R' &&
				((char*)&UnnamedResourceTable[i].AssetType)[2] == 'E' &&
				((char*)&UnnamedResourceTable[i].AssetType)[3] == 'A')
			{
				struct ModelOutputData* mod;
				struct levelArea* levelOutput = output;
				{
					const int NumberOfTextureIDs = 145;
					const int NumberOfSurfaces = 2034;
					const int NumberOfMaterials = 642;

					const SIZE_T requiredMemory =
						sizeof(struct ModelOutputData) +
						sizeof(uint32_t) * NumberOfTextureIDs +
						sizeof(struct PerSurfaceData) * NumberOfSurfaces +
						sizeof(struct PerMaterialData) * NumberOfMaterials +
						sizeof(struct PipelineStateData) * NumberOfMaterials +
						sizeof(struct MaterialConstantBuffers) * NumberOfMaterials;

					Models[levelOutput->modelIndex] = VirtualAlloc(
						nullptr,
						requiredMemory,
						MEM_COMMIT | MEM_RESERVE,
						PAGE_READWRITE
					);

					mod = Models[levelOutput->modelIndex];
					
					mod->textureID = OffsetPointer(mod, sizeof(struct ModelOutputData));
					mod->pod = OffsetPointer(mod->textureID, sizeof(int) * NumberOfTextureIDs);
					mod->pmd = OffsetPointer(mod->pod, sizeof(struct PerSurfaceData) * NumberOfSurfaces);
					mod->psd = OffsetPointer(mod->pmd, sizeof(struct PerMaterialData) * NumberOfMaterials);
					mod->materials = OffsetPointer(mod->psd, sizeof(struct PipelineStateData) * NumberOfMaterials);

					const int maxVerts = 1024 * 1024;

					mod->VertexBuffer = VirtualAlloc(
						nullptr,
						maxVerts * sizeof(struct VertexData),
						MEM_RESERVE,
						PAGE_READWRITE
					);

					mod->IndexBuffer = VirtualAlloc(
						nullptr,
						maxVerts * sizeof(uint32_t),
						MEM_RESERVE | MEM_COMMIT,
						PAGE_READWRITE
					);
				}

				struct MREA_Header
				{
					uint32_t Magic;
					uint32_t Version;
					float AreaTransform[12];
					uint32_t WorldModelCount;
					uint32_t DataSectionCount;
					uint32_t WorldGeometrySection;
					uint32_t ScriptLayerSection;
					uint32_t CollisionSection;
					uint32_t UnknownSection;
					uint32_t LightsSection;
					uint32_t VisibilityTreeSection;
					uint32_t PathSection;
					uint32_t AreaOctreeSection;
				};

				const struct MREA_Header* MreaAsset = AssetPtr;

				DEBUG_ASSERT(SwapEndian(MreaAsset->Magic) == 0xDEADBEEF);

				DEBUG_ASSERT(SwapEndian(MreaAsset->Version) == 0xF);

				const uint32_t DataSectionCount = SwapEndian(MreaAsset->DataSectionCount);

				AssetPtr = OffsetPointer(AssetPtr, sizeof(struct MREA_Header));

				const uint32_t* SectionSizeTable = AssetPtr;

				AssetPtr = OffsetPointer(AssetPtr, DataSectionCount * 4);

				AssetPtr = OffsetPointer(MreaAsset, pad((((char*)AssetPtr) - ((char*)MreaAsset)), 32));

				int ResidentSection = 0;
				const void* DataSectionPtr = AssetPtr;

				long TextureCount = SwapEndian(*(long*)AssetPtr);

				mod->TextureCount = TextureCount;
				AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

				const long* TextureIDTable = AssetPtr;
				for (int i = 0; i < TextureCount; i++)
				{
					mod->textureID[i] = SwapEndian(TextureIDTable[i]);
				}

				AssetPtr = OffsetPointer(AssetPtr, TextureCount * sizeof(long));

				long MaterialCount = SwapEndian(*(long*)AssetPtr);
				mod->materialCount = MaterialCount;
				AssetPtr = OffsetPointer(AssetPtr, sizeof(long));

				const long* MaterialEndOffsets = AssetPtr;


				AssetPtr = OffsetPointer(AssetPtr, MaterialCount * sizeof(long));

				const void* StartOfFirstMaterial = AssetPtr;

				struct Material materials[1024];

				for (int MaterialNum = 0; MaterialNum < MaterialCount; MaterialNum++)
				{
					ReadMaterial(AssetPtr, mod, materials, MaterialNum);
					AssetPtr = OffsetPointer(StartOfFirstMaterial, SwapEndian(MaterialEndOffsets[MaterialNum]));
				}

				DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[ResidentSection]));
				AssetPtr = DataSectionPtr;
				ResidentSection++;
				int totalVertexCount = 0;
				int totalIndexCount = 0;
				int totalSurfaceCount = 0;

				void* handler = AddVectoredExceptionHandler(0, PageFaultDetector);
				VALIDATE_HANDLE(handler);

				levelOutput->worldModelCount = SwapEndian(MreaAsset->WorldModelCount);
				levelOutput->worldModelSurfaceCounts = malloc(levelOutput->worldModelCount * sizeof(uint16_t));
				levelOutput->perWorldModelInfo = malloc(levelOutput->worldModelCount * sizeof(struct worldModel));
				

				for (int WorldModelNum = 0; WorldModelNum < SwapEndian(MreaAsset->WorldModelCount); WorldModelNum++)
				{
					const uint32_t VisorFlags = SwapEndian(*(uint32_t*)AssetPtr);
					levelOutput->perWorldModelInfo[WorldModelNum].visorFlags = VisorFlags;
					AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

					const float* WorldModelTransform = AssetPtr;

					AssetPtr = OffsetPointer(AssetPtr, sizeof(float) * 12);

					const float* AABB = AssetPtr;

					levelOutput->perWorldModelInfo[WorldModelNum].AABB[0] = SwapEndianFloat(AABB[0]);
					levelOutput->perWorldModelInfo[WorldModelNum].AABB[1] = SwapEndianFloat(AABB[1]);
					levelOutput->perWorldModelInfo[WorldModelNum].AABB[2] = SwapEndianFloat(AABB[2]);
					levelOutput->perWorldModelInfo[WorldModelNum].AABB[3] = SwapEndianFloat(AABB[3]);
					levelOutput->perWorldModelInfo[WorldModelNum].AABB[4] = SwapEndianFloat(AABB[4]);
					levelOutput->perWorldModelInfo[WorldModelNum].AABB[5] = SwapEndianFloat(AABB[5]);

					DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[ResidentSection]));
					ResidentSection++;

					uint32_t surfaceCountBefore = totalSurfaceCount;

					AssetPtr = ReadGeometry(MreaAsset, DataSectionPtr, SectionSizeTable, &ResidentSection, mod, materials, &totalVertexCount, &totalIndexCount, &totalSurfaceCount, true);

					DEBUG_ASSERT(totalSurfaceCount - surfaceCountBefore <= 0xFFFFu);

					levelOutput->worldModelSurfaceCounts[WorldModelNum] = totalSurfaceCount - surfaceCountBefore;
					levelOutput->perWorldModelInfo[WorldModelNum].surfaceCount = totalSurfaceCount - surfaceCountBefore;

					maxSurfacesPerWorldModel = max(totalSurfaceCount - surfaceCountBefore, maxSurfacesPerWorldModel);

					DataSectionPtr = AssetPtr;
				}

				DEBUG_ASSERT(RemoveVectoredExceptionHandler(handler));
				

				mod->VertexCount = totalVertexCount;
				mod->IndexCount = totalIndexCount;
				mod->surfaceCount = totalSurfaceCount;

				while (ResidentSection != SwapEndian(MreaAsset->ScriptLayerSection))
				{
					DataSectionPtr = OffsetPointer(DataSectionPtr, SwapEndian(SectionSizeTable[ResidentSection]));
					ResidentSection++;
				}

				AssetPtr = DataSectionPtr;


				uint32_t ScriptLayerMagic = SwapEndian(*(uint32_t*)AssetPtr);
				AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));
				DEBUG_ASSERT(ScriptLayerMagic == 0x53434C59);

				AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

				uint32_t ScriptLayerCount = SwapEndian(*(uint32_t*)AssetPtr);
				DEBUG_ASSERT(levelOutput->scriptLayerCount == ScriptLayerCount);
				levelOutput->scriptLayerBuffer = malloc(ScriptLayerCount * sizeof(struct ScriptLayer));
				AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));


				uint32_t* layerSizeTable = AssetPtr;

				for (int i = 0; i < ScriptLayerCount; i++)
				{
					AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));
				}

				maxScriptLayers = max(maxScriptLayers, ScriptLayerCount);

				for (int scriptLayerNum = 0; scriptLayerNum < ScriptLayerCount; scriptLayerNum++)
				{
					void* StartOfCurrentLayer = AssetPtr;
					AssetPtr = OffsetPointer(AssetPtr, sizeof(uint8_t));

					uint32_t ObjectCount = SwapEndian(*(uint32_t*)AssetPtr);
					AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

					levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectCount = ObjectCount;
					levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer = malloc(sizeof(struct ScriptObject) * ObjectCount);

					for (int j = 0; j < ObjectCount; j++)
					{
						uint8_t ObjectType = *(uint8_t*)AssetPtr;

						levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[j].objectType = ObjectType;
						switch (ObjectType)
						{
						case 0x0:
							break;
						case 0x1:
							DebugBreak();
							break;
						case 0x2:
							break;
						case 0x3:
							break;
						case 0x4:
							break;
						case 0x5:
							break;
						case 0x6:
							break;
						case 0x7:
							break;
						case 0x8:
							break;
						case 0x9:
							break;
						case 0xA:
							break;
						case 0xB:
							break;
						case 0xC:
							break;
						case 0xD:
							break;
						case 0xE:
							break;
						case 0xF:
							break;
						case 0x10:
							break;
						case 0x11:
							break;
						case 0x12:
							DebugBreak();
							break;
						case 0x13:
							break;
						case 0x14:
							break;
						case 0x15:
							break;
						case 0x16:
							break;
						case 0x17:
							break;
						case 0x18:
							break;
						case 0x19:
							break;
						case 0x1A:
							break;
						case 0x1B:
							break;
						case 0x1C:
							break;
						case 0x1D:
							break;
						case 0x20:
							break;
						case 0x21:
							break;
						case 0x22:
							DebugBreak();
							break;
						case 0x24:
							break;
						case 0x25:
							break;
						case 0x26:
							break;
						case 0x27:
							break;
						case 0x28:
							break;
						case 0x2A:
							break;
						case 0x2C:
							break;
						case 0x2D:
							break;
						case 0x2E:
							break;
						case 0x2F:
							break;
						case 0x30:
							break;
						case 0x31:
							break;
						case 0x32:
							break;
						case 0x33:
							break;
						case 0x34:
							break;
						case 0x35:
							break;
						case 0x36:
							break;
						case 0x37:
							break;
						case 0x38:
							break;
						case 0x39:
							break;
						case 0x3A:
							break;
						case 0x3B:
							break;
						case 0x3C:
							DebugBreak();
							break;
						case 0x3D:
							break;
						case 0x3E:
							break;
						case 0x3F:
							break;
						case 0x40:
							break;
						case 0x41:
							break;
						case 0x42:
							break;
						case 0x43:
							break;
						case 0x44:
							break;
						case 0x45:
							break;
						case 0x46:
							break;
						case 0x47:
							break;
						case 0x48:
							break;
						case 0x49:
							break;
						case 0x4A:
							break;
						case 0x4B:
							break;
						case 0x4C:
							break;
						case 0x4D:
							break;
						case 0x4E:
							break;
						case 0x4F:
							break;
						case 0x50:
							break;
						case 0x51:
							break;
						case 0x52:
							break;
						case 0x53:
							break;
						case 0x54:
							break;
						case 0x55:
							break;
						case 0x56:
							break;
						case 0x57:
							break;
						case 0x58:
							break;
						case 0x59:
							DebugBreak();
							break;
						case 0x5A:
							break;
						case 0x5B:
							break;
						case 0x5C:
							break;
						case 0x5D:
							break;
						case 0x5E:
							break;
						case 0x5F:
							break;
						case 0x60:
							break;
						case 0x61:
							break;
						case 0x62:
							break;
						case 0x63:
							break;
						case 0x64:
							break;
						case 0x65:
							break;
						case 0x66:
							break;
						case 0x67:
							break;
						case 0x68:
							break;
						case 0x69:
							break;
						case 0x6A:
							break;
						case 0x6B:
							break;
						case 0x6C:
							break;
						case 0x6D:
							break;
						case 0x6E:
							break;
						case 0x6F:
							break;
						case 0x70:
							break;
						case 0x71:
							break;
						case 0x72:
							break;
						case 0x73:
							break;
						case 0x74:
							break;
						case 0x75:
							break;
						case 0x77:
							break;
						case 0x78:
							break;
						case 0x79:
							break;
						case 0x7A:
							break;
						case 0x7B:
							break;
						case 0x7C:
							break;
						case 0x7D:
							break;
						case 0x7F:
							break;
						case 0x81:
							break;
						case 0x82:
							break;
						case 0x83:
							break;
						case 0x84:
							break;
						case 0x85:
							break;
						case 0x86:
							break;
						case 0x87:
							break;
						case 0x88:
							break;
						case 0x89:
							break;
						case 0x8A:
							break;
						case 0x8B:
							break;
						default:
							DebugBreak();
						}
						AssetPtr = OffsetPointer(AssetPtr, sizeof(uint8_t));

						uint32_t ObjectSize = SwapEndian(*(uint32_t*)AssetPtr);
						AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

						void* endOfObject = OffsetPointer(AssetPtr, ObjectSize);

						uint32_t ScriptObjectID = SwapEndian(*(uint32_t*)AssetPtr);
						levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[j].instanceID = ScriptObjectID;
						AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

						uint32_t ConnectionCount = SwapEndian(*(uint32_t*)AssetPtr);
						AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

						for (int k = 0; k < ConnectionCount; k++)
						{
							uint32_t State = SwapEndian(*(uint32_t*)AssetPtr);
							AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));
							
							uint32_t Message = SwapEndian(*(uint32_t*)AssetPtr);
							AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

							uint32_t ConnectedObject = SwapEndian(*(uint32_t*)AssetPtr);
							AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));
						}
						
						uint32_t PropertySectionSize = SwapEndian(*(uint32_t*)AssetPtr);
						AssetPtr = OffsetPointer(AssetPtr, sizeof(uint32_t));

						switch (ObjectType)
						{
						case 0x0:
						{
							struct Actor_gcn
							{
								float pos[3];
								float rot[3];
								float scale[3];
								float unknown1[3];
								float scanOffset[3];
								float unknown2;
								float unknown3;
								uint32_t shouldBe2;
								float health;
								float knockbackResistance;
								uint32_t shouldBe18;
								uint32_t powerBeamVulnerability;
								uint32_t iceBeamVulnerability;
								uint32_t waveBeamVulnerability;
								uint32_t plasmaBeamVulnerability;
								uint32_t bombVulnerability;
								uint32_t powerBombVulnerability;
								uint32_t missileVulnerability;
								uint32_t boostBallVulnerability;
								uint32_t phazonVulnerability;
								uint32_t aiVulnerability;
								uint32_t poisonWaterVulnerability;
								uint32_t lavaVulnerability;
								uint32_t hotVulnerability;
								uint32_t unusedWeapon1Vulnerability;
								uint32_t unusedWeapon2Vulnerability;
								uint32_t unusedWeapon3Vulnerability;
								uint32_t shouldBe5;
								uint32_t chargedPowerBeamVulnerability;
								uint32_t chargedIceBeamVulnerability;
								uint32_t chargedWaveBeamVulnerability;
								uint32_t chargedPlasmaBeamVulnerability;
								uint32_t chargedPhazonBeamVulnerability;
								uint32_t shouldBe5_k1;
								uint32_t superMissileVulnerability;
								uint32_t iceSpreaderVulnerability;
								uint32_t wavebusterVulnerability;
								uint32_t flamethrowerVulnerability;
								uint32_t phazonComboVulnerability;
								uint32_t CmdlID;
							};


							int nameLength = strlen(AssetPtr) + 1;
							AssetPtr = OffsetPointer(AssetPtr, nameLength);

							struct Actor_gcn* actor = AssetPtr;

							struct Script_Actor* actor_output = malloc(sizeof(struct Script_Actor));

							actor_output->pos[0] = SwapEndianFloat(actor->pos[0]);
							actor_output->pos[1] = SwapEndianFloat(actor->pos[1]);
							actor_output->pos[2] = SwapEndianFloat(actor->pos[2]);
							
							actor_output->rot[0] = SwapEndianFloat(actor->rot[0]);
							actor_output->rot[1] = SwapEndianFloat(actor->rot[1]);
							actor_output->rot[2] = SwapEndianFloat(actor->rot[2]);

							actor_output->scale[0] = SwapEndianFloat(actor->scale[0]);
							actor_output->scale[1] = SwapEndianFloat(actor->scale[1]);
							actor_output->scale[2] = SwapEndianFloat(actor->scale[2]);


							actor_output->scanOffset[0] = SwapEndianFloat(actor->scanOffset[0]);
							actor_output->scanOffset[1] = SwapEndianFloat(actor->scanOffset[1]);
							actor_output->scanOffset[2] = SwapEndianFloat(actor->scanOffset[2]);

							actor_output->CmdlID = SwapEndian(actor->CmdlID);

							levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[j].typeSpecificData = actor_output;
							
							DEBUG_ASSERT(SwapEndian(actor->shouldBe2) == 2);
							DEBUG_ASSERT(SwapEndian(actor->shouldBe18) == 18);
							DEBUG_ASSERT(SwapEndian(actor->shouldBe5) == 5);
							DEBUG_ASSERT(SwapEndian(actor->shouldBe5_k1) == 5);
							break;
						}
						case 0x8:
						{
							struct Platform_gcn
							{
								float pos[3];
								float rot[3];
								float scale[3];
								float unknown1[3];
								float scanOffset[3];
								uint32_t CmdlID;
							};


							int nameLength = strlen(AssetPtr) + 1;
							AssetPtr = OffsetPointer(AssetPtr, nameLength);

							struct Platform_gcn* platform = AssetPtr;

							struct Script_Platform* platform_output = malloc(sizeof(struct Script_Platform));

							platform_output->CmdlID = SwapEndian(platform->CmdlID);

							levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[j].typeSpecificData = platform_output;
							break;
						}
						case 0xF:
						{
							int nameLength = strlen(AssetPtr) + 1;
							AssetPtr = OffsetPointer(AssetPtr, nameLength);

							float* position = AssetPtr;

							Player.position[0] = SwapEndianFloat(position[0]);
							Player.position[1] = SwapEndianFloat(position[1]);
							Player.position[2] = SwapEndianFloat(position[2]);
							AssetPtr = OffsetPointer(AssetPtr, sizeof(float) * 3);

							break;
						}
						case 0x4E:
						{
#pragma pack(push, 1)
							struct AreaAttributes_gcn
							{
								uint32_t unknown;
								uint8_t showSkybox;
								uint32_t environmentalEffect;
								float initialEnvironmentalEffectDensity;
								float initialThermalHeatLevel;
								float xrayFogDistance;
								float initialWorldLightingLevel;
								uint32_t skyboxModel;
								uint32_t phazonType;
							};
#pragma pack(pop, 1)

							struct AreaAttributes_gcn* areaAttributes = AssetPtr;

							struct Script_AreaAttributes* areaAttributes_output = malloc(sizeof(struct Script_Platform));

							areaAttributes_output->showSkybox = areaAttributes->showSkybox;
							areaAttributes_output->environmentalEffect = SwapEndian(areaAttributes->environmentalEffect);
							areaAttributes_output->initialEnvironmentalEffectDensity = SwapEndianFloat(areaAttributes->initialEnvironmentalEffectDensity);
							areaAttributes_output->initialThermalHeatLevel = SwapEndianFloat(areaAttributes->initialThermalHeatLevel);
							areaAttributes_output->xrayFogDistance = SwapEndianFloat(areaAttributes->xrayFogDistance);
							areaAttributes_output->initialWorldLightingLevel = SwapEndianFloat(areaAttributes->initialWorldLightingLevel);
							areaAttributes_output->skyboxModel = SwapEndian(areaAttributes->skyboxModel);
							areaAttributes_output->phazonType = SwapEndian(areaAttributes->phazonType);

							levelOutput->scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[j].typeSpecificData = areaAttributes_output;
							break;
						}
						case 0x53:
						{
							int nameLength = strlen(AssetPtr) + 1;
							AssetPtr = OffsetPointer(AssetPtr, nameLength);
							break;
						}
						}
						
						AssetPtr = endOfObject;
					}
					AssetPtr = OffsetPointer(StartOfCurrentLayer, SwapEndian(layerSizeTable[scriptLayerNum]));
				}

				AssetPtr = MreaAsset;
			}
			else if (
				((char*)&UnnamedResourceTable[i].AssetType)[0] == 'M' &&
				((char*)&UnnamedResourceTable[i].AssetType)[1] == 'L' &&
				((char*)&UnnamedResourceTable[i].AssetType)[2] == 'V' &&
				((char*)&UnnamedResourceTable[i].AssetType)[3] == 'L')
			{
				struct GameLevel* level = output;

				void* MlvlPtr = AssetPtr;

				uint32_t Magic = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t Version = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t WorldNameSTRG = SwapEndian(*(uint32_t*)MlvlPtr);
				level->nameID = WorldNameSTRG;
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t SaveFileID = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t DefaultSkyboxID = SwapEndian(*(uint32_t*)MlvlPtr);
				level->skybox = DefaultSkyboxID;
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t MemoryRelayCount = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				struct MemoryRelay
				{
					uint32_t MemoryRelayInstanceID;
					uint32_t TargetInstanceID;
					uint16_t Message;
					bool Active;
				};

				for (int i = 0; i < MemoryRelayCount; i++)
				{
					struct MemoryRelay* relay = MlvlPtr;
					MlvlPtr = OffsetPointer(MlvlPtr, 0xB);
				}

				uint32_t AreaCount = SwapEndian(*(uint32_t*)MlvlPtr);
				level->areaCount = AreaCount;
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t Unknown1 = SwapEndian(*(uint32_t*)MlvlPtr);
				DEBUG_ASSERT(Unknown1 == 1);

				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				for (int AreaNum = 0; AreaNum < AreaCount; AreaNum++)
				{
					uint32_t AreaNameID = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					AreaPositions[AreaNum][0][0] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 0));
					AreaPositions[AreaNum][1][0] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 1));
					AreaPositions[AreaNum][2][0] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 2));
					AreaPositions[AreaNum][3][0] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 3));

					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(float) * 4);

					AreaPositions[AreaNum][0][1] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 0));
					AreaPositions[AreaNum][1][1] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 1));
					AreaPositions[AreaNum][2][1] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 2));
					AreaPositions[AreaNum][3][1] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 3));

					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(float) * 4);

					AreaPositions[AreaNum][0][2] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 0));
					AreaPositions[AreaNum][1][2] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 1));
					AreaPositions[AreaNum][2][2] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 2));
					AreaPositions[AreaNum][3][2] = SwapEndianFloat(*(float*)OffsetPointer(MlvlPtr, sizeof(float) * 3));

					AreaPositions[AreaNum][0][3] = 0.000000;
					AreaPositions[AreaNum][1][3] = 0.000000;
					AreaPositions[AreaNum][2][3] = 0.000000;
					AreaPositions[AreaNum][3][3] = 1.000000;

					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(float) * 4);

					const float* CollisionBoundingBox = MlvlPtr;

					level->areas[AreaNum].AABB[0] = SwapEndianFloat(CollisionBoundingBox[0]);
					level->areas[AreaNum].AABB[1] = SwapEndianFloat(CollisionBoundingBox[1]);
					level->areas[AreaNum].AABB[2] = SwapEndianFloat(CollisionBoundingBox[2]);
					level->areas[AreaNum].AABB[3] = SwapEndianFloat(CollisionBoundingBox[3]);
					level->areas[AreaNum].AABB[4] = SwapEndianFloat(CollisionBoundingBox[4]);
					level->areas[AreaNum].AABB[5] = SwapEndianFloat(CollisionBoundingBox[5]);

					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(float) * 6);


					uint32_t MreaID = SwapEndian(*(uint32_t*)MlvlPtr);

					level->areaIDs[AreaNum] = MreaID;

					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					uint32_t AreaIDInternal = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					uint32_t AttachedAreaCount = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					uint16_t* AttachedAreaIndexTable = MlvlPtr;

					for (int i = 0; i < AttachedAreaCount; i++)
					{
						uint16_t AttachedAreaIndex = SwapEndian(*(uint16_t*)MlvlPtr);
						MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint16_t));
					}

					uint32_t Unknown2 = SwapEndian(*(uint32_t*)MlvlPtr);
					DEBUG_ASSERT(Unknown2 == 0);

					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					uint32_t DependencyCount = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					for (int i = 0; i < DependencyCount; i++)
					{
						uint32_t DependencyID = SwapEndian(*(uint32_t*)MlvlPtr);
						MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

						FourCC DependencyType = SwapEndian(*(FourCC*)MlvlPtr);
						
						MlvlPtr = OffsetPointer(MlvlPtr, sizeof(FourCC));

						if (*OffsetPointer((char*)&DependencyType, 3) == 'C' &&
							*OffsetPointer((char*)&DependencyType, 2) == 'M' &&
							*OffsetPointer((char*)&DependencyType, 1) == 'D' &&
							*OffsetPointer((char*)&DependencyType, 0) == 'L')
						{
							ModelLoadQueue[ModelLoadQueueSize] = DependencyID;
							ModelLoadQueueSize++;
						}
					}

					uint32_t DependencyOffsetCount = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					for (int i = 0; i < DependencyOffsetCount; i++)
					{
						//todo: print the offsets
						MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));
					}

					uint32_t DockCount = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					for (int i = 0; i < DockCount; i++)
					{
						struct DockConnection
						{
							uint32_t AreaIndex;
							uint16_t DockIndex;
							uint16_t LoadOther;
						};

						uint32_t ConnectingDockCount = SwapEndian(*(uint32_t*)MlvlPtr);

						MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

						if (ConnectingDockCount != 0)
						{
							for (int j = 0; j < ConnectingDockCount; j++)
							{
								struct DockConnection* dc = MlvlPtr;
								MlvlPtr = OffsetPointer(MlvlPtr, sizeof(struct DockConnection));
							}
						}

						uint32_t DockCoordinateCount = SwapEndian(*(uint32_t*)MlvlPtr);
						MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

						for (int j = 0; j < DockCoordinateCount; j++)
						{
							MlvlPtr = OffsetPointer(MlvlPtr, sizeof(float) * 3);
						}
					}

				}

				uint32_t MapID = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint8_t Unknown2 = *(uint8_t*)MlvlPtr;
				DEBUG_ASSERT(Unknown2 == 0);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint8_t));

				uint32_t ScriptInstanceCount = SwapEndian(*(uint32_t*)MlvlPtr);
				DEBUG_ASSERT(ScriptInstanceCount == 0);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				uint32_t AudioGroupCount = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				for (int i = 0; i < AudioGroupCount; i++)
				{
					uint32_t GroupID = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

					uint32_t AgscID = SwapEndian(*(uint32_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));
				}

				uint8_t UnusedString = *(uint8_t*)MlvlPtr;
				DEBUG_ASSERT(UnusedString == 0);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint8_t));

				uint32_t AreaCount2 = SwapEndian(*(uint32_t*)MlvlPtr);
				DEBUG_ASSERT(AreaCount2 == AreaCount);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				for (int i = 0; i < AreaCount; i++)
				{
					uint32_t LayerCount = SwapEndian(*(uint32_t*)MlvlPtr);
					level->areas[i].scriptLayerCount = LayerCount;
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));


					uint64_t LayerFlags = SwapEndian(*(uint64_t*)MlvlPtr);
					MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint64_t));
				}

				uint32_t LayerNameCount = SwapEndian(*(uint32_t*)MlvlPtr);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));

				for (int i = 0; i < LayerNameCount; i++)
				{
					MlvlPtr = OffsetPointer(MlvlPtr, strlen(MlvlPtr) + 1);
				}

				uint32_t AreaCount3 = SwapEndian(*(uint32_t*)MlvlPtr);
				DEBUG_ASSERT(AreaCount3 == AreaCount);
				MlvlPtr = OffsetPointer(MlvlPtr, sizeof(uint32_t));
			}
			else if (
				((char*)&UnnamedResourceTable[i].AssetType)[0] == 'C' &&
				((char*)&UnnamedResourceTable[i].AssetType)[1] == 'T' &&
				((char*)&UnnamedResourceTable[i].AssetType)[2] == 'W' &&
				((char*)&UnnamedResourceTable[i].AssetType)[3] == 'K')
			{
				switch (AssetID)
				{
				case 0x953a7c63:
					struct GameSettings_gcn
					{
						char WorldPakPrefix[8];
						char unknown1[12];
						float FieldOfView;
					};

					struct GameSettings_gcn* gameSettings = AssetPtr;

					Settings.fov = SwapEndianFloat(gameSettings->FieldOfView);
					break;
				case 0x264a4972:
					Settings.NormalGravityAccel = SwapEndianFloat(*OffsetPointer((const float*)AssetPtr, 0xC0));
					Settings.FluidGravityAccel = SwapEndianFloat(*OffsetPointer((const float*)AssetPtr, 0xC4));
					Settings.VerticalJumpAccel = SwapEndianFloat(*OffsetPointer((const float*)AssetPtr, 0xC8));
					Settings.PlayerHeight = SwapEndianFloat(*OffsetPointer((const float*)AssetPtr, 0x2BC));
					break;
				default:
					DebugBreak();
				}
				
			}
			if (SwapEndian(UnnamedResourceTable[i].CompressionFlag))
				free(AssetPtr);

			break;
		}
	}
}

int main()
{
	HWND Window;
	ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		PageSize = systemInfo.dwPageSize;
	}


	OPENFILENAME ofn = { 0 };
	WCHAR szFile[260] = { 0 };

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"Metroid Prime\0*.iso\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	THROW_ON_FALSE(GetOpenFileNameW(&ofn));

	HANDLE file = CreateFileW(
		szFile,
		GENERIC_READ,
		0,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	VALIDATE_HANDLE(file);

	HANDLE hMapFile = CreateFileMappingW(
		file,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr);
	VALIDATE_HANDLE(hMapFile);

	__assume(hMapFile != 0);

	void* GameImageAddress = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
	__assume(GameImageAddress != nullptr);

	void* Metroid1 = nullptr;
	void* Metroid2 = nullptr;
	void* Metroid3 = nullptr;
	void* Metroid4 = nullptr;
	void* Metroid5 = nullptr;
	void* Metroid6 = nullptr;
	void* Metroid7 = nullptr;
	void* Metroid8 = nullptr;

	void* Tweaks = nullptr;

	struct DiskHeader
	{
		uint32_t GameCode;
		uint16_t MakerCode;
		uint8_t DiskID;
		uint8_t Version;
		uint8_t AudioStreaming;
		uint8_t StreamBufferSize;
		uint8_t unused1[18];
		uint32_t Magic;
		char GameName[992];
		uint32_t DebugMonitorOffset;
		uint32_t Unknown;
		uint8_t unused2[24];
		uint32_t DOLOffset;
		uint32_t FSTOffset;
		uint32_t FSTSize;
		uint32_t MaxFSTSize;
	};

	struct DiskHeader* dh = GameImageAddress;

	struct FileEntry
	{
		uint8_t Flags;
		uint8_t FileNameOffsetp1;
		uint8_t FileNameOffsetp2;
		uint8_t FileNameOffsetp3;
		uint32_t FileOffset;
		uint32_t Unknown;
	};

	struct FileEntry* FST = OffsetPointer(GameImageAddress, SwapEndian(dh->FSTOffset));
	void* StringTable = OffsetPointer(FST, SwapEndian(FST->Unknown) * sizeof(struct FileEntry));
	int NumEntries = SwapEndian(FST->Unknown);

	for (int i = 0; i < NumEntries; i++)
	{
		struct FileEntry* FE = FST + i;
		switch (FE->Flags)
		{
		case 0:
			break;
		case 1:
			break;
		default:
			DebugBreak();
		}
		uint32_t FileNameOffset = (FE->FileNameOffsetp1 << 16) | (FE->FileNameOffsetp2 << 8) | FE->FileNameOffsetp3;
		if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid1.pak") == 0)
		{
			Metroid1 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid2.pak") == 0)
		{
			Metroid2 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid3.pak") == 0)
		{
			Metroid3 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid4.pak") == 0)
		{
			Metroid4 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "metroid5.pak") == 0)
		{
			Metroid5 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid6.pak") == 0)
		{
			Metroid6 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid7.pak") == 0)
		{
			Metroid7 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Metroid8.pak") == 0)
		{
			Metroid8 = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
		else if (strcmp(OffsetPointer(StringTable, FileNameOffset), "Tweaks.Pak") == 0)
		{
			Tweaks = OffsetPointer(GameImageAddress, SwapEndian(FE->FileOffset));
		}
	}

	ReadPak(Tweaks, GetNamedAssetID(Tweaks, "Game"), &Settings);
	ReadPak(Tweaks, GetNamedAssetID(Tweaks, "Player"), &Settings);

	void* CurrentPak = Metroid1;
	
	uint32_t objectBlacklistIDs[] = 
	{
		0x1C0B0BEF,
		0xD07305A7,
		0xDEBA01D7
	};

	//get offsets and such
	CurrentPak = Metroid1;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!1IntroWorld0310a"), &LoadedLevel);//Metroid1 (25 materials, 28 areas)
	//CurrentPak = Metroid2;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!2RuinsWorld0310"), &LoadedLevel);//Metroid2 (21 materials, 64 areas)
	//CurrentPak = Metroid3;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!3IceWorld0310"), &LoadedLevel);//Metroid3 (33 materials, 56 areas)
	//CurrentPak = Metroid4;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!4OverWorld0310"), &LoadedLevel);//Metroid4 (21 materials, 44 areas)
	//CurrentPak = Metroid5;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!5MinesWorld0310"), &LoadedLevel);//Metroid5 (18 materials, 42 areas)
	//CurrentPak = Metroid6;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!6LavaWorld0310"), &LoadedLevel);//Metroid6 (13 materials, 29 areas)
	//CurrentPak = Metroid7;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "!7CraterWorld0310"), &LoadedLevel);//Metroid7 (12 materials, 12 areas)
	//CurrentPak = Metroid8;ReadPak(CurrentPak, GetNamedAssetID(CurrentPak, "z8EndCinema0310"), &LoadedLevel);//Metroid8 (5 materials, 1 area)
	
	for (int i = 0; i < LoadedLevel.areaCount; i++)
	{
		LoadedLevel.areas[i].modelIndex = ModelCount;
		ReadPak(CurrentPak, LoadedLevel.areaIDs[i], &LoadedLevel.areas[i]);
		Models[ModelCount]->firstMaterialIndex = GlobalMaterialCount;
		GlobalMaterialCount += Models[ModelCount]->materialCount;
		ModelCount++;

	}
	Player.currentAreaIndex = 0;

	int maxMaterials = 0;

	for (int dependencyNum = 0; dependencyNum < ModelLoadQueueSize; dependencyNum++)
	{
		bool alreadyInList = false;

		for (int i = 0; i < countof(objectBlacklistIDs); i++)
		{
			if (ModelLoadQueue[dependencyNum] == objectBlacklistIDs[i])
			{
				alreadyInList = true;
				break;
			}
		}

		if (alreadyInList)
			continue;

		for (int i = 0; i < ModelStandbyListSize; i++)
		{
			if (ModelStandbyList[i].cmdlID == ModelLoadQueue[dependencyNum])
			{
				ModelLoadQueue[dependencyNum] = ModelStandbyList[i].modelIndex;
				alreadyInList = true;
				break;
			}
		}

		if (alreadyInList == false)
		{
			ReadPak(CurrentPak, ModelLoadQueue[dependencyNum], &Models[ModelCount]);
			Models[ModelCount]->firstMaterialIndex = GlobalMaterialCount;
			GlobalMaterialCount += Models[ModelCount]->materialCount;

			ModelStandbyList[ModelStandbyListSize].cmdlID = ModelLoadQueue[dependencyNum];
			ModelStandbyList[ModelStandbyListSize].modelIndex = ModelCount;
			ModelStandbyListSize++;

			maxMaterials = max(maxMaterials, Models[ModelCount]->materialCount);

			ModelLoadQueue[dependencyNum] = ModelCount;
			ModelCount++;
		}
	}

	for (int areaNum = 0; areaNum < LoadedLevel.areaCount; areaNum++)
	{
		for (int scriptLayerNum = 0; scriptLayerNum < LoadedLevel.areas[areaNum].scriptLayerCount; scriptLayerNum++)
		{
			for (int scriptObjectNum = 0; scriptObjectNum < LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectCount; scriptObjectNum++)
			{
				uint32_t* cmdl;
				switch (LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].objectType)
				{
				case 0x0:
				{
					struct Script_Actor* actor = LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].typeSpecificData;
					cmdl = &actor->CmdlID;
					break;
				}
				case 0x8:
				{
					struct Script_Platform* platform = LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].typeSpecificData;
					cmdl = &platform->CmdlID;

					break;
				}
				case 0x4E:
				{
					struct Script_AreaAttributes* areaAttributes = LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].typeSpecificData;
					cmdl = &areaAttributes->skyboxModel;
					
					if (*cmdl == 0xFFFFFFFF)
					{
						*cmdl = LoadedLevel.skybox;
					}

					break;
				}
				default:
					continue;
				}

				for (int i = 0; i < countof(objectBlacklistIDs); i++)
				{
					if (*cmdl == objectBlacklistIDs[i])
					{
						*cmdl = 0xFFFFFFFF;
						break;
					}
				}

				if (*cmdl == 0xFFFFFFFF)
					continue;

				bool alreadyInList = false;


				for (int i = 0; i < ModelStandbyListSize; i++)
				{
					if (ModelStandbyList[i].cmdlID == *cmdl)
					{
						*cmdl = ModelStandbyList[i].modelIndex;
						alreadyInList = true;
						break;
					}
				}

				if (alreadyInList == false)
				{
					ReadPak(CurrentPak, *cmdl, &Models[ModelCount]);
					Models[ModelCount]->firstMaterialIndex = GlobalMaterialCount;
					GlobalMaterialCount += Models[ModelCount]->materialCount;
					ModelStandbyList[ModelStandbyListSize].cmdlID = *cmdl;
					ModelStandbyList[ModelStandbyListSize].modelIndex = ModelCount;
					ModelStandbyListSize++;
					*cmdl = ModelCount;


					maxMaterials = max(maxMaterials, Models[ModelCount]->materialCount);
					ModelCount++;
				}

			nextScriptObject:

			}
		}
	}

	
	for (int i = 0; i < ModelCount; i++)
	{
		for (int j = 0; j < Models[i]->materialCount; j++)
		{
			HashData(
				&Models[i]->materials[j].pixelCB,
				sizeof(Models[i]->materials[j].pixelCB),
				&Models[i]->psd[j].materialHash,
				sizeof(Models[i]->psd[j].materialHash)
			);

			for (int k = 0; k < PixelShaderCount; k++)
			{
				if (PixelShaders[k].hash == Models[i]->psd[j].materialHash)
				{
					Models[i]->psd[j].shaderIndex = k;
					goto nextMaterial;
				}
			}

			GeneratePixelShader(&Models[i]->materials[j].pixelCB, &PixelShaders[PixelShaderCount].bytecode);
			PixelShaders[PixelShaderCount].hash = Models[i]->psd[j].materialHash;
			Models[i]->psd[j].shaderIndex = PixelShaderCount;

			PixelShaderCount++;
			
			nextMaterial:
			;
		}
	}

	for (int i = 0; i < ModelCount; i++)
	{
		for (int j = 0; j < Models[i]->TextureCount; j++)
		{
			bool bAlreadyInQueue = false;
			for (int k = 0; k < TextureLoadQueueSize; k++)
			{
				if (TextureLoadQueue[k] == Models[i]->textureID[j])
				{
					bAlreadyInQueue = true;
					break;
				}
			}

			if (!bAlreadyInQueue)
			{

				TextureLoadQueue[TextureLoadQueueSize] = Models[i]->textureID[j];
				TextureLoadQueueSize++;
			}
		}
	}

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	WNDCLASSEXW WindowClass = { 0 };
	WindowClass.cbSize = sizeof(WNDCLASSEXW);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = &PreInitProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = GetModuleHandleW(nullptr);
	WindowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
	WindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	WindowClass.lpszMenuName = nullptr;
	WindowClass.lpszClassName = WindowName;
	WindowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

	THROW_ON_FALSE((!RegisterClassExW(&WindowClass)) == true);

	Window = CreateWindowExW(
		0,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		Width,
		Height,
		nullptr,
		nullptr,
		GetModuleHandleW(nullptr),
		nullptr);

	VALIDATE_HANDLE(Window);

	if (FullScreen)
	{
		HMONITOR hmon = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		THROW_ON_FALSE(GetMonitorInfoW(hmon, &mi));
		SetWindowLongPtrW(Window, GWL_STYLE, 0);
	}

	THROW_ON_FALSE(QueryPerformanceFrequency(&ProcessorFrequency));

#ifdef USE_DEBUG_LAYERS
	ID3D12Debug* DebugInterface;
	THROW_ON_FAIL(D3D12GetDebugInterface(&IID_ID3D12Debug, &DebugInterface));
	ID3D12Debug_EnableDebugLayer(DebugInterface);
#endif

	UINT DxgiFactoryFlags = 0;
#ifdef USE_DEBUG_LAYERS
	DxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	IDXGIFactory6* DxgiFactory;
	THROW_ON_FAIL(CreateDXGIFactory2(DxgiFactoryFlags, &IID_IDXGIFactory6, &DxgiFactory));

	BOOL allowTearing = FALSE;

	THROW_ON_FAIL(IDXGIFactory6_CheckFeatureSupport(
		DxgiFactory,
		DXGI_FEATURE_PRESENT_ALLOW_TEARING,
		&allowTearing,
		sizeof(allowTearing)));

	bTearingSupport = (allowTearing == TRUE);

	IDXGIAdapter1* Adapter;

	if (bWarp)
	{
		IDXGIFactory6_EnumWarpAdapter(DxgiFactory, &IID_IDXGIAdapter1, &Adapter);
	}
	else
	{
		IDXGIFactory6_EnumAdapterByGpuPreference(DxgiFactory, 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter1, &Adapter);
	}

	THROW_ON_FAIL(D3D12CreateDevice((IUnknown*)Adapter, D3D_FEATURE_LEVEL_12_1, &IID_ID3D12Device, &Device));

	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS MultiSampleLevels = { 0 };
		MultiSampleLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		MultiSampleLevels.SampleCount = MSAACount;
		MultiSampleLevels.NumQualityLevels = 0;
		MultiSampleLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

		while (SUCCEEDED(ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &MultiSampleLevels, sizeof(MultiSampleLevels))))
		{
			if (MultiSampleLevels.NumQualityLevels > 0)
				MSAACount = MultiSampleLevels.SampleCount;
			MultiSampleLevels.SampleCount *= 2;
		}
	}

	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = { 0 };
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		RootSignatureVersion = featureData.HighestVersion;
	}

#ifdef USE_DEBUG_LAYERS
	ID3D12InfoQueue* InfoQueue;
	THROW_ON_FAIL(ID3D12Device_QueryInterface(Device, &IID_ID3D12InfoQueue, &InfoQueue));

	THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
	THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_WARNING, TRUE));

	D3D12_MESSAGE_ID MessageIDs[] = {
		D3D12_MESSAGE_ID_DEVICE_CLEARVIEW_EMPTYRECT
	};

	D3D12_INFO_QUEUE_FILTER NewFilter = { 0 };
	NewFilter.DenyList.NumSeverities = 0;
	NewFilter.DenyList.pSeverityList = nullptr;
	NewFilter.DenyList.NumIDs = countof(MessageIDs);
	NewFilter.DenyList.pIDList = &MessageIDs;

	THROW_ON_FAIL(ID3D12InfoQueue_PushStorageFilter(InfoQueue, &NewFilter));
#endif

	{
		D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = { 0 };
		CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		THROW_ON_FAIL(ID3D12Device_CreateCommandQueue(Device, &CommandQueueDesc, &IID_ID3D12CommandQueue, &CommandQueue));
	}
	
	DXGI_MODE_DESC BackBufferDesc = { 0 };
	BackBufferDesc.Width = Width;
	BackBufferDesc.Height = Height;
	BackBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = { 0 };
	SwapChainDesc.BufferCount = FrameBufferCount;
	SwapChainDesc.BufferDesc = BackBufferDesc;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.OutputWindow = Window;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.Windowed = !FullScreen;
	SwapChainDesc.Flags = bTearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	IDXGISwapChain* tempSwapChain;

	THROW_ON_FAIL(IDXGIFactory6_CreateSwapChain(DxgiFactory, (IUnknown*)CommandQueue, &SwapChainDesc, &tempSwapChain));

	IDXGISwapChain_QueryInterface(tempSwapChain, &IID_IDXGISwapChain3, &SwapChain);

	THROW_ON_FAIL(IDXGIFactory6_MakeWindowAssociation(DxgiFactory, Window, DXGI_MWA_NO_ALT_ENTER));

	FrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(SwapChain);

	{
		D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc = { 0 };
		RtvHeapDesc.NumDescriptors = FrameBufferCount * 2;
		RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateDescriptorHeap(Device, &RtvHeapDesc, &IID_ID3D12DescriptorHeap, &rtvDescriptorHeap));
		ID3D12DescriptorHeap_SetName(rtvDescriptorHeap, L"Render Target Resource Heap");

		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtvDescriptorHeap, &rtvHeapStart);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc = { 0 };
		DsvHeapDesc.NumDescriptors = 1;
		DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		DsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateDescriptorHeap(Device, &DsvHeapDesc, &IID_ID3D12DescriptorHeap, &dsDescriptorHeap));
		ID3D12DescriptorHeap_SetName(dsDescriptorHeap, L"Depth/Stencil Resource Heap");

		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dsDescriptorHeap, &dsvHeapStart);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = { 0 };
		HeapDesc.NumDescriptors = TotalTextureSlots;
		HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		THROW_ON_FAIL(ID3D12Device_CreateDescriptorHeap(Device, &HeapDesc, &IID_ID3D12DescriptorHeap, &TextureDescriptorHeap));
		ID3D12DescriptorHeap_SetName(TextureDescriptorHeap, L"Texture Resource Heap");

		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(TextureDescriptorHeap, &textureHeapStart);
	}

	rtvDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	cbvDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeapStart;

		OptimizedClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		OptimizedClearValue.Color[0] = 0.0f;
		OptimizedClearValue.Color[1] = 0.2f;
		OptimizedClearValue.Color[2] = 0.4f;
		OptimizedClearValue.Color[3] = 0.0f;

		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDesc = { 0 };
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		ResourceDesc.Alignment = 0;
		ResourceDesc.Width = Width;
		ResourceDesc.Height = Height;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ResourceDesc.SampleDesc.Count = MSAACount;
		ResourceDesc.SampleDesc.Quality = 0;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		for (int i = 0; i < FrameBufferCount; i++)
		{
			ID3D12Device_CreateCommittedResource(
				Device,
				&HeapProperties,
				D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&ResourceDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&OptimizedClearValue,
				&IID_ID3D12Resource,
				&RenderTargetsMSAA[i]
			);

			ID3D12Device_CreateRenderTargetView(Device, RenderTargetsMSAA[i], nullptr, rtvHandle);
			ID3D12Resource_SetName(RenderTargetsMSAA[i], L"Multisampled Render Target");

			rtvHandle.ptr += rtvDescriptorSize;
		}

		for (int i = 0; i < FrameBufferCount; i++)
		{
			THROW_ON_FAIL(IDXGISwapChain3_GetBuffer(SwapChain, i, &IID_ID3D12Resource, &RenderTargets[i]));

			ID3D12Device_CreateRenderTargetView(Device, RenderTargets[i], nullptr, rtvHandle);
			ID3D12Resource_SetName(RenderTargets[i], L"Presentation Render Target");

			rtvHandle.ptr += rtvDescriptorSize;
		}
	}

	{
		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		ResourceDescription.Height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_D32_FLOAT;
		ResourceDescription.SampleDesc.Count = MSAACount;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		D3D12_RESOURCE_ALLOCATION_INFO AllocInfo;
		ID3D12Device_GetResourceAllocationInfo(Device, &AllocInfo, 0, 1, &ResourceDescription);

		ResourceDescription.Width = Width;
		ResourceDescription.Height = Height;

		D3D12_HEAP_DESC HeapDesc = { 0 };
		HeapDesc.SizeInBytes = AllocInfo.SizeInBytes;
		HeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapDesc.Properties.CreationNodeMask = 1;
		HeapDesc.Properties.VisibleNodeMask = 1;
		HeapDesc.Alignment = AllocInfo.Alignment;
		HeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;

		THROW_ON_FAIL(ID3D12Device_CreateHeap(Device, &HeapDesc, &IID_ID3D12Heap, &DepthBufferHeap));


		D3D12_CLEAR_VALUE DepthOptimizedClearValue = { 0 };
		DepthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		DepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		DepthOptimizedClearValue.DepthStencil.Stencil = 0;

		THROW_ON_FAIL(ID3D12Device_CreatePlacedResource(Device, DepthBufferHeap, 0 , &ResourceDescription, D3D12_RESOURCE_STATE_DEPTH_WRITE, &DepthOptimizedClearValue, &IID_ID3D12Resource, &DepthStencilBuffer));

		D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilDesc = { 0 };
		DepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		DepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		ID3D12Device_CreateDepthStencilView(Device, DepthStencilBuffer, &DepthStencilDesc, dsvHeapStart);
	}

	for (int i = 0; i < FrameBufferCount; i++)
	{
		THROW_ON_FAIL(ID3D12Device_CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, &CommandAllocator[i]));
	}

	THROW_ON_FAIL(ID3D12Device_CreateCommandList(Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator[FrameIndex], nullptr, &IID_ID3D12GraphicsCommandList, &GraphicsCommandList));
	

	for (int i = 0; i < FrameBufferCount; i++)
	{
		FenceValue[i] = 0;
		THROW_ON_FAIL(ID3D12Device_CreateFence(Device, FenceValue[i], D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &fence[i]));
	}

	{
		D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = { 0 };
		CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

		THROW_ON_FAIL(ID3D12Device_CreateCommandQueue(Device, &CommandQueueDesc, &IID_ID3D12CommandQueue, &CopyCommandQueue));

		THROW_ON_FAIL(ID3D12Device_CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_COPY, &IID_ID3D12CommandAllocator, &CopyCommandAllocator));
		THROW_ON_FAIL(ID3D12Device_CreateCommandList(Device, 0, D3D12_COMMAND_LIST_TYPE_COPY, CopyCommandAllocator, nullptr, &IID_ID3D12GraphicsCommandList, &CopyCommandList));

		CopyCommandFenceValue = 0;
		THROW_ON_FAIL(ID3D12Device_CreateFence(Device, CopyCommandFenceValue, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &CopyCommandFence));

		FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		VALIDATE_HANDLE(FenceEvent);
	}

	D3D12_DESCRIPTOR_RANGE1  DescriptorTableRanges[1] = { 0 };
	DescriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	{
		int maxTextureCount = 0;
		for (int i = 0; i < ModelCount; i++)
			maxTextureCount = max(maxTextureCount, Models[i]->TextureCount);
		DescriptorTableRanges[0].NumDescriptors = maxTextureCount;
	}
	DescriptorTableRanges[0].BaseShaderRegister = 0;
	DescriptorTableRanges[0].RegisterSpace = 0;
	DescriptorTableRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
	DescriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER1  RootParameters[4] = { 0 };
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	RootParameters[1].Descriptor.ShaderRegister = 1;
	RootParameters[1].Descriptor.RegisterSpace = 0;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	RootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[2].DescriptorTable.pDescriptorRanges = &DescriptorTableRanges[0];
	RootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	RootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[3].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	RootParameters[3].Descriptor.ShaderRegister = 0;
	RootParameters[3].Descriptor.RegisterSpace = 0;
	RootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC Sampler = { 0 };
	Sampler.Filter = D3D12_FILTER_ANISOTROPIC;
	Sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	Sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	Sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	Sampler.MipLODBias = 0;
	Sampler.MaxAnisotropy = 16;
	Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	Sampler.MinLOD = 0.0f;
	Sampler.MaxLOD = D3D12_FLOAT32_MAX;
	Sampler.ShaderRegister = 0;
	Sampler.RegisterSpace = 0;
	Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc = { 0 };
	RootSignatureDesc.Version = RootSignatureVersion;
	RootSignatureDesc.Desc_1_1.NumParameters = countof(RootParameters);
	RootSignatureDesc.Desc_1_1.pParameters = RootParameters;
	RootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
	RootSignatureDesc.Desc_1_1.pStaticSamplers = &Sampler;
	RootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	ID3DBlob* signature;
	ID3DBlob* error;
	THROW_ON_FAIL(D3D12SerializeVersionedRootSignature(&RootSignatureDesc, &signature, &error));

	THROW_ON_FAIL(ID3D12Device_CreateRootSignature(Device, 0, ID3D10Blob_GetBufferPointer(signature), ID3D10Blob_GetBufferSize(signature), &IID_ID3D12RootSignature, &RootSignature));
	ID3D12RootSignature_SetName(RootSignature, L"Graphics Root Signature");

	HANDLE VertexShaderFile = CreateFileW(
		L"VertexShader.cso",
		GENERIC_READ,
		0,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	VALIDATE_HANDLE(VertexShaderFile);

	SIZE_T VertexShaderSize;

	{
		LARGE_INTEGER tempLongInteger;

		THROW_ON_FALSE(GetFileSizeEx(VertexShaderFile, &tempLongInteger));

		VertexShaderSize = tempLongInteger.QuadPart;
	}

	HANDLE VertexShaderFileMap = CreateFileMappingW(
		VertexShaderFile,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr);

	VALIDATE_HANDLE(VertexShaderFileMap);

	void* VertexShaderBytecode = MapViewOfFile(VertexShaderFileMap, FILE_MAP_READ, 0, 0, 0);



	HANDLE PredicationVertexShaderFile = CreateFileW(
		L"PredicationVertexShader.cso",
		GENERIC_READ,
		0,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	VALIDATE_HANDLE(PredicationVertexShaderFile);

	SIZE_T PredicationVertexShaderSize;

	{
		LARGE_INTEGER tempLongInteger;

		THROW_ON_FALSE(GetFileSizeEx(PredicationVertexShaderFile, &tempLongInteger));

		PredicationVertexShaderSize = tempLongInteger.QuadPart;
	}

	HANDLE PredicationVertexShaderFileMap = CreateFileMappingW(
		PredicationVertexShaderFile,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr);

	VALIDATE_HANDLE(PredicationVertexShaderFileMap);

	void* PredicationVertexShaderBytecode = MapViewOfFile(PredicationVertexShaderFileMap, FILE_MAP_READ, 0, 0, 0);


	HANDLE PixelShaderFile = CreateFileW(
		L"PixelShader.cso",
		GENERIC_READ,
		0,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	VALIDATE_HANDLE(PixelShaderFile);

	SIZE_T PixelShaderSize;

	{
		LARGE_INTEGER tempLongInteger;

		THROW_ON_FALSE(GetFileSizeEx(PixelShaderFile, &tempLongInteger));

		PixelShaderSize = tempLongInteger.QuadPart;
	}

	HANDLE PixelShaderFileMap = CreateFileMappingW(
		PixelShaderFile,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr);

	VALIDATE_HANDLE(PixelShaderFileMap);

	void* PixelShaderBytecode = MapViewOfFile(PixelShaderFileMap, FILE_MAP_READ, 0, 0, 0);


	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 1, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 2, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 3, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 5, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 6, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8 * 7, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendDesc = { 0 };
	DefaultRenderTargetBlendDesc.BlendEnable = TRUE;
	DefaultRenderTargetBlendDesc.LogicOpEnable = FALSE;
	DefaultRenderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	DefaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	DefaultRenderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	DefaultRenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	DefaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	DefaultRenderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	DefaultRenderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	DefaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = { 0 };
	PsoDesc.InputLayout.NumElements = countof(inputLayout);
	PsoDesc.InputLayout.pInputElementDescs = inputLayout;
	PsoDesc.pRootSignature = RootSignature;
	PsoDesc.VS.BytecodeLength = VertexShaderSize;
	PsoDesc.VS.pShaderBytecode = VertexShaderBytecode;
	PsoDesc.PS.BytecodeLength = PixelShaderSize;
	PsoDesc.PS.pShaderBytecode = PixelShaderBytecode;
	PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	PsoDesc.SampleDesc.Count = MSAACount;
	PsoDesc.SampleDesc.Quality = 0;
	PsoDesc.SampleMask = 0xFFFFFFFF;
	PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	PsoDesc.RasterizerState.FrontCounterClockwise = TRUE;
	PsoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	PsoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	PsoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	PsoDesc.RasterizerState.DepthClipEnable = TRUE;
	PsoDesc.RasterizerState.MultisampleEnable = TRUE;
	PsoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	PsoDesc.RasterizerState.ForcedSampleCount = 0;
	PsoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	PsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	PsoDesc.BlendState.IndependentBlendEnable = FALSE;
	for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) PsoDesc.BlendState.RenderTarget[i] = DefaultRenderTargetBlendDesc;
	PsoDesc.NumRenderTargets = 1;
	PsoDesc.DepthStencilState.DepthEnable = TRUE;
	PsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	PsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	PsoDesc.DepthStencilState.StencilEnable = FALSE;
	PsoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	PsoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	PsoDesc.DepthStencilState.FrontFace = (D3D12_DEPTH_STENCILOP_DESC){ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	PsoDesc.DepthStencilState.BackFace = (D3D12_DEPTH_STENCILOP_DESC){ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };

	PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PsoDesc.DepthStencilState.DepthEnable = FALSE;
	PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	THROW_ON_FAIL(ID3D12Device_CreateGraphicsPipelineState(Device, &PsoDesc, &IID_ID3D12PipelineState, &debugPipeline));
	ID3D12PipelineState_SetName(debugPipeline, L"debug pipeline");

	PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PsoDesc.DepthStencilState.DepthEnable = TRUE;
	PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

	for (int i = 0; i < ModelCount; i++)
	{
		for (int j = 0; j < Models[i]->materialCount; j++)
		{
			uint64_t pipelineStateHash;
			HashData(
				&Models[i]->psd[j],
				sizeof(Models[i]->psd[j]),
				&pipelineStateHash,
				sizeof(pipelineStateHash)
			);

			for (int k = 0; k < PipelineStateCount; k++)
			{
				if (PipelineStates[k].hash == pipelineStateHash)
				{
					Models[i]->pmd[j].pipelineStateIndex = k;
					goto nextPipelineState;
				}
			}

			PsoDesc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(PixelShaders[Models[i]->psd[j].shaderIndex].bytecode);
			PsoDesc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(PixelShaders[Models[i]->psd[j].shaderIndex].bytecode);

			switch (Models[i]->psd[j].BlendSourceFactor)
			{
			case 0x1:
				DefaultRenderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
				DefaultRenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
				break;
			case 0x4:
				DefaultRenderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
				DefaultRenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
				break;
			default:
				DebugBreak();
			}

			switch (Models[i]->psd[j].BlendDestinationFactor)
			{
			case 0x0:
				DefaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
				DefaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
				break;
			case 0x1:
				DefaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_ONE;
				DefaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
				break;
			case 0x5:
				DefaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				DefaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
				break;
			default:
				DebugBreak();
			}

			for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) PsoDesc.BlendState.RenderTarget[i] = DefaultRenderTargetBlendDesc;

			if (Models[i]->psd[j].enableDepth)
			{
				PsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			}
			else
			{
				PsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			}

			THROW_ON_FAIL(ID3D12Device_CreateGraphicsPipelineState(Device, &PsoDesc, &IID_ID3D12PipelineState, &PipelineStates[PipelineStateCount].pso));

			{
				wchar_t buffer[40];
				_snwprintf_s(buffer, 40, _TRUNCATE, L"pipeline %i (0x%llX)", PipelineStateCount, pipelineStateHash);
				ID3D12PipelineState_SetName(PipelineStates[PipelineStateCount].pso, buffer);
			}

			PipelineStates[PipelineStateCount].hash = pipelineStateHash;

			Models[i]->pmd[j].pipelineStateIndex = PipelineStateCount;


			PipelineStateCount++;

		nextPipelineState:
			;
		}
	}

	DefaultRenderTargetBlendDesc.RenderTargetWriteMask = 0;
	for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		PsoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;
	PsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	PsoDesc.PS.BytecodeLength = PredicationVertexShaderSize;
	PsoDesc.PS.pShaderBytecode = PredicationVertexShaderBytecode;

	PsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;


	THROW_ON_FAIL(ID3D12Device_CreateGraphicsPipelineState(Device, &PsoDesc, &IID_ID3D12PipelineState, &predicationPipeline));
	ID3D12PipelineState_SetName(predicationPipeline, L"predication pipeline");


	PsoDesc.RasterizerState.MultisampleEnable = FALSE;
	PsoDesc.SampleDesc.Count = 1;

	THROW_ON_FAIL(ID3D12Device_CreateGraphicsPipelineState(Device, &PsoDesc, &IID_ID3D12PipelineState, &PipelineStateObject_FinalStage));

	for (int i = 0; i < ModelCount; i++)
	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = sizeof(struct VertexData) * Models[i]->VertexCount;
		ResourceDescription.Height = 1;
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDescription.SampleDesc.Count = 1;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &IID_ID3D12Resource, &VertexBuffer[i]));

		ID3D12Resource_SetName(VertexBuffer[i], L"Vertex Buffer Heap");

		D3D12_HEAP_PROPERTIES UploadHeapProperties = { 0 };
		UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		UploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		UploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		UploadHeapProperties.CreationNodeMask = 1;
		UploadHeapProperties.VisibleNodeMask = 1;

		ID3D12Resource* vBufferUploadHeap;
		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &vBufferUploadHeap));

		ID3D12Resource_SetName(vBufferUploadHeap, L"Vertex Buffer Upload Heap");

		void* pData;
		THROW_ON_FAIL(ID3D12Resource_Map(vBufferUploadHeap, 0, nullptr, &pData));

		memcpy(pData, Models[i]->VertexBuffer, Models[i]->VertexCount * sizeof(struct VertexData));
		

		DEBUG_ASSERT(VirtualFree(Models[i]->VertexBuffer, 0, MEM_RELEASE) != 0);

		ID3D12Resource_Unmap(vBufferUploadHeap, 0, nullptr);

		ID3D12GraphicsCommandList_CopyBufferRegion(CopyCommandList, VertexBuffer[i], 0, vBufferUploadHeap, 0, Models[i]->VertexCount * sizeof(struct VertexData));


		D3D12_RESOURCE_BARRIER Barrier = { 0 };
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.pResource = VertexBuffer[i];
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, 1, &Barrier);
	}

	for (int i = 0; i < ModelCount; i++)
	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = Models[i]->IndexCount * sizeof(uint32_t);
		ResourceDescription.Height = 1;
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDescription.SampleDesc.Count = 1;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &IID_ID3D12Resource, &IndexBuffer[i]));

		ID3D12Resource_SetName(VertexBuffer[i], L"Index Buffer Heap");

		D3D12_HEAP_PROPERTIES UploadHeapProperties = { 0 };
		UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		UploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		UploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		UploadHeapProperties.CreationNodeMask = 1;
		UploadHeapProperties.VisibleNodeMask = 1;

		ID3D12Resource* IndexBufferUploadHeap;
		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &IndexBufferUploadHeap));

		ID3D12Resource_SetName(IndexBufferUploadHeap, L"Index Buffer Upload Heap");

		void* pData;
		THROW_ON_FAIL(ID3D12Resource_Map(IndexBufferUploadHeap, 0, nullptr, &pData));

		memcpy(pData, Models[i]->IndexBuffer, Models[i]->IndexCount * sizeof(uint32_t));

		DEBUG_ASSERT(VirtualFree(Models[i]->IndexBuffer, 0, MEM_RELEASE) != 0);

		ID3D12Resource_Unmap(IndexBufferUploadHeap, 0, nullptr);

		ID3D12GraphicsCommandList_CopyBufferRegion(CopyCommandList, IndexBuffer[i], 0, IndexBufferUploadHeap, 0, Models[i]->IndexCount * sizeof(uint32_t));

		D3D12_RESOURCE_BARRIER Barrier = { 0 };
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.pResource = IndexBuffer[i];
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, 1, &Barrier);
	}

	{
		int totalWorldModels = 0;

		for (int i = 0; i < LoadedLevel.areaCount; i++)
			totalWorldModels += LoadedLevel.areas[i].worldModelCount;

		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = (sizeof(struct VertexData) * 8 * totalWorldModels) + (sizeof(uint32_t) * 12 * 3);
		ResourceDescription.Height = 1;
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDescription.SampleDesc.Count = 1;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &IID_ID3D12Resource, &OcclusionVertexBuffer));

		ID3D12Resource_SetName(OcclusionVertexBuffer, L"Occlusion Mesh Vertex Buffer Heap");

		D3D12_HEAP_PROPERTIES UploadHeapProperties = { 0 };
		UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		UploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		UploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		UploadHeapProperties.CreationNodeMask = 1;
		UploadHeapProperties.VisibleNodeMask = 1;

		ID3D12Resource* vBufferUploadHeap;
		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &vBufferUploadHeap));

		ID3D12Resource_SetName(vBufferUploadHeap, L"Occlusion Mesh Vertex Buffer Upload Heap");

		void* pData;
		THROW_ON_FAIL(ID3D12Resource_Map(vBufferUploadHeap, 0, nullptr, &pData));

		struct VertexData* TempBuffer = VirtualAlloc(nullptr, ResourceDescription.Width, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		

		int OcclusionMeshNum = 0;
		for (int areaNum = 0; areaNum < LoadedLevel.areaCount; areaNum++)
		{
			for (int worldModelNum = 0; worldModelNum < LoadedLevel.areas[areaNum].worldModelCount; worldModelNum++, OcclusionMeshNum++)
			{
				const float extensionSize = .01;
				vec3 p1 = {
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[0] - extensionSize,
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[1] - extensionSize,
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[2] - extensionSize
				};

				vec3 p2 = {
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[3] + extensionSize,
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[4] + extensionSize,
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[5] + extensionSize
				};

				TempBuffer[OcclusionMeshNum * 8 + 0] = (struct VertexData){ p2[0], p2[1], p2[2] };
				TempBuffer[OcclusionMeshNum * 8 + 1] = (struct VertexData){ p2[0], p1[1], p2[2] };
				TempBuffer[OcclusionMeshNum * 8 + 2] = (struct VertexData){ p2[0], p2[1], p1[2] };
				TempBuffer[OcclusionMeshNum * 8 + 3] = (struct VertexData){ p2[0], p1[1], p1[2] };
				TempBuffer[OcclusionMeshNum * 8 + 4] = (struct VertexData){ p1[0], p2[1], p2[2] };
				TempBuffer[OcclusionMeshNum * 8 + 5] = (struct VertexData){ p1[0], p1[1], p2[2] };
				TempBuffer[OcclusionMeshNum * 8 + 6] = (struct VertexData){ p1[0], p2[1], p1[2] };
				TempBuffer[OcclusionMeshNum * 8 + 7] = (struct VertexData){ p1[0], p1[1], p1[2] };
			}
		}

		uint32_t* TempIndexBuffer = OffsetPointer(TempBuffer, sizeof(struct VertexData) * 8 * totalWorldModels);

		TempIndexBuffer[0] = 4;
		TempIndexBuffer[1] = 0;
		TempIndexBuffer[2] = 2;

		TempIndexBuffer[3] = 2;
		TempIndexBuffer[4] = 3;
		TempIndexBuffer[5] = 7;

		TempIndexBuffer[6] = 6;
		TempIndexBuffer[7] = 7;
		TempIndexBuffer[8] = 5;

		TempIndexBuffer[9] = 1;
		TempIndexBuffer[10] = 5;
		TempIndexBuffer[11] = 7;

		TempIndexBuffer[12] = 0;
		TempIndexBuffer[13] = 1;
		TempIndexBuffer[14] = 3;

		TempIndexBuffer[15] = 4;
		TempIndexBuffer[16] = 5;
		TempIndexBuffer[17] = 1;

		TempIndexBuffer[18] = 4;
		TempIndexBuffer[19] = 2;
		TempIndexBuffer[20] = 6;

		TempIndexBuffer[21] = 2;
		TempIndexBuffer[22] = 7;
		TempIndexBuffer[23] = 6;

		TempIndexBuffer[24] = 6;
		TempIndexBuffer[25] = 5;
		TempIndexBuffer[26] = 4;

		TempIndexBuffer[27] = 1;
		TempIndexBuffer[28] = 7;
		TempIndexBuffer[29] = 3;

		TempIndexBuffer[30] = 0;
		TempIndexBuffer[31] = 3;
		TempIndexBuffer[32] = 2;

		TempIndexBuffer[33] = 4;
		TempIndexBuffer[34] = 1;
		TempIndexBuffer[35] = 0;


		memcpy(pData, TempBuffer, ResourceDescription.Width);
		
		DEBUG_ASSERT(VirtualFree(TempBuffer, 0, MEM_RELEASE) != 0);

		ID3D12Resource_Unmap(vBufferUploadHeap, 0, nullptr);

		ID3D12GraphicsCommandList_CopyBufferRegion(CopyCommandList, OcclusionVertexBuffer, 0, vBufferUploadHeap, 0, ResourceDescription.Width);


		D3D12_RESOURCE_BARRIER Barrier = { 0 };
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.pResource = OcclusionVertexBuffer;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, 1, &Barrier);
	}

	for (int i = 0; i < FrameBufferCount; i++)
	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = PerObjectDataAlignedSize * TotalObjectSlots;
		ResourceDescription.Height = 1;
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDescription.SampleDesc.Count = 1;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &PerObjectConstantBuffer[i]));

		ID3D12Resource_SetName(PerObjectConstantBuffer[i], L"Vertex Constant Buffer Upload Resource Heap");

		D3D12_RANGE ReadRange = { 0 };
		ReadRange.Begin = 0;
		ReadRange.End = 0;

		THROW_ON_FAIL(ID3D12Resource_Map(PerObjectConstantBuffer[i], 0, &ReadRange, &perObjectCbvGPUAddress[i]));

		memcpy(perObjectCbvGPUAddress[i], &cbPerObject, sizeof(cbPerObject));
		memcpy(perObjectCbvGPUAddress[i] + PerObjectDataAlignedSize, &cbPerObject, sizeof(cbPerObject));
	}

	D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle2;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(TextureDescriptorHeap, &CpuDescriptorHandle2);

	int texturesUploaded = 0;

	uint64_t totalTextureAllocationSize = 0;

	for (int TempTextureBeingCreated = 0; TempTextureBeingCreated < TextureLoadQueueSize; TempTextureBeingCreated++)
	{
		int textureBytesPerRow = 0;
		struct TextureOutputData* NativeTexture;
		ReadPak(CurrentPak, TextureLoadQueue[TempTextureBeingCreated], &NativeTexture);
		
		textureBytesPerRow = (NativeTexture->Width * 4);

		THROW_ON_FALSE(NativeTexture->Width * NativeTexture->Height > 0);

		D3D12_RESOURCE_DESC textureDesc = { 0 };
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Alignment = 0;
		textureDesc.Width = NativeTexture->Width;
		textureDesc.Height = NativeTexture->Height;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		{
			D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
			HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			HeapProperties.CreationNodeMask = 1;
			HeapProperties.VisibleNodeMask = 1;

			THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &IID_ID3D12Resource, &TextureBuffers[TempTextureBeingCreated]));
		}

		{
			wchar_t buffer[40];
			_snwprintf_s(buffer, 50, _TRUNCATE, L"Texture %i Buffer Resource Heap", TempTextureBeingCreated);
			ID3D12Resource_SetName(TextureBuffers[TempTextureBeingCreated], buffer);
		}

		{
			D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
			HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			HeapProperties.CreationNodeMask = 1;
			HeapProperties.VisibleNodeMask = 1;

			UINT64 textureSize;
			ID3D12Device_GetCopyableFootprints(Device, &textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureSize);

			totalTextureAllocationSize += textureSize;
			D3D12_RESOURCE_DESC ResourceDescription = { 0 };
			ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			ResourceDescription.Alignment = 0;
			ResourceDescription.Width = textureSize;
			ResourceDescription.Height = 1;
			ResourceDescription.DepthOrArraySize = 1;
			ResourceDescription.MipLevels = 1;
			ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
			ResourceDescription.SampleDesc.Count = 1;
			ResourceDescription.SampleDesc.Quality = 0;
			ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

			THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &TextureBufferUploadHeaps[TempTextureBeingCreated]));
		}

		ID3D12Resource_SetName(TextureBufferUploadHeaps[TempTextureBeingCreated], L"Texture Buffer Upload Resource Heap");

		{
			BYTE* pData;
			THROW_ON_FAIL(ID3D12Resource_Map(TextureBufferUploadHeaps[TempTextureBeingCreated], 0, nullptr, &pData));

			for (int i = 0; i < NativeTexture->Height; i++)
			{
				memcpy(
					OffsetPointer(pData, pad256(NativeTexture->Width * 4) * i),
					OffsetPointer(&NativeTexture->ImageData, textureBytesPerRow * i),
					NativeTexture->Width * 4);
			}

			free(NativeTexture);

			ID3D12Resource_Unmap(TextureBufferUploadHeaps[TempTextureBeingCreated], 0, nullptr);

			D3D12_TEXTURE_COPY_LOCATION Dst = { 0 };
			Dst.pResource = TextureBuffers[TempTextureBeingCreated];
			Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			Dst.SubresourceIndex = 0;

			D3D12_RESOURCE_DESC Desc;
			ID3D12Resource_GetDesc(TextureBuffers[TempTextureBeingCreated], &Desc);

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
			ID3D12Device_GetCopyableFootprints(Device, &Desc, 0, 1, 0, &footprint, nullptr, nullptr, nullptr);

			D3D12_TEXTURE_COPY_LOCATION Src = { 0 };
			Src.pResource = TextureBufferUploadHeaps[TempTextureBeingCreated];
			Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			Src.PlacedFootprint = footprint;

			ID3D12GraphicsCommandList_CopyTextureRegion(CopyCommandList, &Dst, 0, 0, 0, &Src, nullptr);

			D3D12_RESOURCE_BARRIER Barrier = { 0 };
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.Transition.pResource = TextureBuffers[TempTextureBeingCreated];
			Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, 1, &Barrier);
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = { 0 };
		SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SrvDesc.Texture2D.MipLevels = 1;

		ID3D12Device_CreateShaderResourceView(Device, TextureBuffers[TempTextureBeingCreated], &SrvDesc, CpuDescriptorHandle2);

		CpuDescriptorHandle2.ptr += cbvDescriptorSize;
		texturesUploaded++;

		TextureStandbyList[TextureStandbyListSize] = TextureLoadQueue[TempTextureBeingCreated];
		TextureStandbyListSize++;
	}

	TextureLoadQueueSize = 0;

	for (int i = texturesUploaded; i < TotalTextureSlots; i++)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = { 0 };
		SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SrvDesc.Texture2D.MipLevels = 1;

		ID3D12Device_CreateShaderResourceView(Device, nullptr, &SrvDesc, CpuDescriptorHandle2);
		CpuDescriptorHandle2.ptr += cbvDescriptorSize;
	}

	for (int modelNum = 0; modelNum < ModelCount; modelNum++)
	{
		for (int materialNum = 0; materialNum < Models[modelNum]->materialCount; materialNum++)
		{
			for (int TEVStage = 0; TEVStage < Models[modelNum]->materials[materialNum].pixelCB.TEVStageCount; TEVStage++)
			{
				if (Models[modelNum]->materials[materialNum].textureIndices.indices[TEVStage] == TEV_NO_TEXTURE_PRESENT)
					Models[modelNum]->materials[materialNum].textureIndices.indices[TEVStage] = 0;

				int ObjectLocalTextureIndex = Models[modelNum]->materials[materialNum].textureIndices.indices[TEVStage];
				
				uint32_t TextureToFind = Models[modelNum]->textureID[ObjectLocalTextureIndex];

				if (TextureToFind == 0)
				{
					Models[modelNum]->materials[materialNum].textureIndices.indices[TEVStage] = 0;
					continue;
				}


				for (int trueTextureIndex = 0; trueTextureIndex < TextureStandbyListSize; trueTextureIndex++)
				{
					if (TextureStandbyList[trueTextureIndex] == TextureToFind)
					{
						Models[modelNum]->materials[materialNum].textureIndices.indices[TEVStage] = trueTextureIndex;
						goto NextTEVStage;
					}
				}
				DebugBreak();

			NextTEVStage:
				continue;
			}
		}
	}

	for (int i = 0; i < FrameBufferCount; i++)
	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = VertexPerObjectConstantsAlignedSize * GlobalMaterialCount;
		ResourceDescription.Height = 1;
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDescription.SampleDesc.Count = 1;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &VertexShaderConstantBuffer[i]));

		ID3D12Resource_SetName(VertexShaderConstantBuffer[i], L"Vertex Constant Buffer Upload Resource Heap");

		D3D12_RANGE ReadRange = { 0 };
		ReadRange.Begin = 0;
		ReadRange.End = 0;

		THROW_ON_FAIL(ID3D12Resource_Map(VertexShaderConstantBuffer[i], 0, &ReadRange, &vertexCbvGPUAddress[i]));

		int cbOffset = 0;
		for (int modelNum = 0; modelNum < ModelCount; modelNum++)
		{
			for (int materialNum = 0; materialNum < Models[modelNum]->materialCount; materialNum++)
			{
				memcpy(vertexCbvGPUAddress[i] + VertexPerObjectConstantsAlignedSize * cbOffset, &Models[modelNum]->materials[materialNum].vertexCB, sizeof(Models[modelNum]->materials[materialNum].vertexCB));
				cbOffset++;
			}
		}
	}

	for (int i = 0; i < FrameBufferCount; i++)
	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDescription = { 0 };
		ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDescription.Alignment = 0;
		ResourceDescription.Width = PixelPerObjectConstantsAlignedSize * GlobalMaterialCount;
		ResourceDescription.Height = 1;
		ResourceDescription.DepthOrArraySize = 1;
		ResourceDescription.MipLevels = 1;
		ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDescription.SampleDesc.Count = 1;
		ResourceDescription.SampleDesc.Quality = 0;
		ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource, &PixelShaderConstantBuffer[i]));

		ID3D12Resource_SetName(PixelShaderConstantBuffer[i], L"Pixel Constant Buffer Upload Resource Heap");

		D3D12_RANGE ReadRange = { 0 };
		ReadRange.Begin = 0;
		ReadRange.End = 0;

		THROW_ON_FAIL(ID3D12Resource_Map(PixelShaderConstantBuffer[i], 0, &ReadRange, &pixelCbvGPUAddress[i]));

		int cbOffset = 0;
		for (int modelNum = 0; modelNum < ModelCount; modelNum++)
		{
			for (int materialNum = 0; materialNum < Models[modelNum]->materialCount; materialNum++)
			{
				memcpy(pixelCbvGPUAddress[i] + PixelPerObjectConstantsAlignedSize * cbOffset, &Models[modelNum]->materials[materialNum].textureIndices, sizeof(Models[modelNum]->materials[materialNum].textureIndices));
				cbOffset++;
			}
		}
	}

	{
		int predicateCount = 0;

		for (int i = 0; i < LoadedLevel.areaCount; i++)
		{
			predicateCount += LoadedLevel.areas[i].worldModelCount;
		}

		D3D12_QUERY_HEAP_DESC queryHeapDesc = { 0 };
		queryHeapDesc.Count = predicateCount;
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;

		THROW_ON_FAIL(ID3D12Device_CreateQueryHeap(
			Device,
			&queryHeapDesc,
			&IID_ID3D12QueryHeap,
			&PredicationQueryHeap
		));

		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProperties.CreationNodeMask = 1;
		HeapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC queryResultDesc = { 0 };
		queryResultDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		queryResultDesc.Alignment = 0;
		queryResultDesc.Width = predicateCount * 8;
		queryResultDesc.Height = 1;
		queryResultDesc.DepthOrArraySize = 1;
		queryResultDesc.MipLevels = 1;
		queryResultDesc.Format = DXGI_FORMAT_UNKNOWN;
		queryResultDesc.SampleDesc.Count = 1;
		queryResultDesc.SampleDesc.Quality = 0;
		queryResultDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		queryResultDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(
			Device,
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&queryResultDesc,
			D3D12_RESOURCE_STATE_PREDICATION,
			nullptr,
			&IID_ID3D12Resource,
			&PredicationBuffer
		));
	}

	ID3D12QueryHeap_SetName(PredicationQueryHeap, L"Predication Query Heap");
	ID3D12Resource_SetName(PredicationBuffer, L"Predication Buffer");

	THROW_ON_FAIL(ID3D12GraphicsCommandList_Close(GraphicsCommandList));
	THROW_ON_FAIL(ID3D12GraphicsCommandList_Close(CopyCommandList));

	{
		ID3D12CommandList* ppCommandLists[] = { (ID3D12CommandList*)CopyCommandList };
		ID3D12CommandQueue_ExecuteCommandLists(CopyCommandQueue, countof(ppCommandLists), ppCommandLists);
	}

	CopyCommandFenceValue++;
	THROW_ON_FAIL(ID3D12Fence_SetEventOnCompletion(CopyCommandFence, CopyCommandFenceValue, FenceEvent));
	THROW_ON_FAIL(ID3D12CommandQueue_Signal(CopyCommandQueue, CopyCommandFence, CopyCommandFenceValue));

	WaitForSingleObject(FenceEvent, INFINITE);

	{
		ID3D12CommandList* ppCommandLists[] = { (ID3D12CommandList*)GraphicsCommandList };
		ID3D12CommandQueue_ExecuteCommandLists(CommandQueue, countof(ppCommandLists), ppCommandLists);
	}

	FenceValue[FrameIndex]++;
	THROW_ON_FAIL(ID3D12CommandQueue_Signal(CommandQueue, fence[FrameIndex], FenceValue[FrameIndex]));

	for (int modelNum = 0; modelNum < ModelCount; modelNum++)
	{
		VertexBufferView[modelNum].BufferLocation = ID3D12Resource_GetGPUVirtualAddress(VertexBuffer[modelNum]);
		VertexBufferView[modelNum].StrideInBytes = sizeof(struct VertexData);
		VertexBufferView[modelNum].SizeInBytes = sizeof(struct VertexData) * Models[modelNum]->VertexCount;

		IndexBufferView[modelNum].BufferLocation = ID3D12Resource_GetGPUVirtualAddress(IndexBuffer[modelNum]);
		IndexBufferView[modelNum].Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView[modelNum].SizeInBytes = Models[modelNum]->IndexCount * sizeof(uint32_t);
	}

	OcclusionMeshVertexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(OcclusionVertexBuffer);
	OcclusionMeshVertexBufferView.StrideInBytes = sizeof(struct VertexData);
	{
		int totalWorldModels = 0;

		for (int i = 0; i < LoadedLevel.areaCount; i++)
			totalWorldModels += LoadedLevel.areas[i].worldModelCount;
		OcclusionMeshVertexBufferView.SizeInBytes = sizeof(struct VertexData) * 8 * totalWorldModels;
	}

	OcclusionMeshIndexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(OcclusionVertexBuffer) + OcclusionMeshVertexBufferView.SizeInBytes;
	OcclusionMeshIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	OcclusionMeshIndexBufferView.SizeInBytes = 36 * sizeof(uint32_t);



	Viewport.TopLeftX = 0;
	Viewport.TopLeftY = 0;
	Viewport.Width = Width;
	Viewport.Height = Height;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	ScissorRect.left = 0;
	ScissorRect.top = 0;
	ScissorRect.right = Width;
	ScissorRect.bottom = Height;

	glm_perspective_lh_zo(glm_rad(Settings.fov), (float)Width / (float)Height, 0.1f, 1000.0f, cameraProjMat);

	THROW_ON_FALSE(GetCursorPos(&cursorPos));


	ShowWindow(Window, SW_SHOW);

	THROW_ON_FALSE(SetWindowLongPtrA(Window, GWLP_WNDPROC, (LONG_PTR)WndProc) != 0);

	MSG Message = { 0 };

	while (Message.message != WM_QUIT)
	{
		if (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message);
			DispatchMessageW(&Message);
		}
	}

	WaitForPreviousFrame();
	CloseHandle(FenceEvent);

	BOOL fs = FALSE;
	if (SUCCEEDED(IDXGISwapChain3_GetFullscreenState(SwapChain, &fs, nullptr)))
		IDXGISwapChain3_SetFullscreenState(SwapChain, false, nullptr);

	ID3D12Device_Release(Device);
	IDXGISwapChain3_Release(SwapChain);
	ID3D12CommandQueue_Release(CommandQueue);
	ID3D12CommandQueue_Release(CopyCommandQueue);
	ID3D12DescriptorHeap_Release(rtvDescriptorHeap);
	ID3D12GraphicsCommandList_Release(GraphicsCommandList);
	ID3D12GraphicsCommandList_Release(CopyCommandList);

	for (int i = 0; i < FrameBufferCount; i++)
	{
		ID3D12Resource_Release(RenderTargets[i]);
		ID3D12CommandAllocator_Release(CommandAllocator[i]);
		ID3D12Fence_Release(fence[i]);
	}
	ID3D12PipelineState_Release(PipelineStateObject_FinalStage);
	ID3D12RootSignature_Release(RootSignature);

	for (int modelNum = 0; modelNum < ModelCount; modelNum++)
	{
		ID3D12Resource_Release(VertexBuffer[modelNum]);
		ID3D12Resource_Release(IndexBuffer[modelNum]);
	}

	ID3D12Resource_Release(DepthStencilBuffer);
	ID3D12DescriptorHeap_Release(dsDescriptorHeap);

	for (int i = 0; i < FrameBufferCount; i++)
	{
		ID3D12Resource_Release(PerObjectConstantBuffer[i]);
	}

	return 0;
}

LRESULT CALLBACK PreInitProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK IdleProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		Sleep(25);
		break;
	case WM_SIZE:
		if (!IsIconic(hwnd))
			THROW_ON_FALSE(SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc) != 0);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}
	return 0;
}
struct DepthSortedObject
{
	int modelIndex;
	int objectIndex;
	int constantBufferIndex;
	float objectDistance;
};

struct nonStaticObject
{
	int modelIndex;
	int constBufferIndex;
	vec3 position;
};

struct nonStaticObject nonStaticObjects[1024];
int nonStaticObjectCount = 0;

struct DepthSortedObject transparentObjectBuffer[1024 * 4];

void swap(struct DepthSortedObject* a, struct DepthSortedObject* b) {
	struct DepthSortedObject temp = *a;
	*a = *b;
	*b = temp;
}

int partition(struct DepthSortedObject* arr, int low, int high) {
	struct DepthSortedObject pivot = arr[high];

	int i = low - 1;

	for (int j = low; j <= high - 1; j++)
	{
		bool shouldSwap = false;
		if (arr[j].objectDistance < pivot.objectDistance)
		{
			shouldSwap = true;
		}
		else if (arr[j].objectDistance == pivot.objectDistance)
		{
			if (arr[j].modelIndex == pivot.modelIndex)
			{
				if (arr[j].objectIndex < pivot.objectIndex)
				{
					shouldSwap = true;
				}
			}
			else
			{
				if (arr[j].modelIndex < pivot.modelIndex)
				{
					shouldSwap = true;
				}
			}

		}

		if (shouldSwap)
		{
			i++;
			swap(&arr[i], &arr[j]);
		}
	}

	swap(&arr[i + 1], &arr[high]);
	return i + 1;
}

void quickSort(struct DepthSortedObject* arr, int low, int high)
{
	if (low >= high) {
		return;
	}

	int pi = partition(arr, low, high);

	quickSort(arr, low, pi - 1);

	quickSort(arr, pi + 1, high);
}



void GenerateTexgenTransforms(float secondsmod900, mat4 pretransformMatrix_out, mat4 posttransformMatrix_out, struct uvanim* animation, int modelIndex)
{
	switch (animation->mode)
	{
	case 0:
	{
		mat4 view_inv;
		glm_mat4_inv(cameraViewMat, view_inv);

		mat4 modelMatrix = INIT_MAT4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);


		glm_mat4_mul(view_inv, modelMatrix, pretransformMatrix_out);

		pretransformMatrix_out[3][0] = pretransformMatrix_out[3][1] = pretransformMatrix_out[3][2] = 0;

		mat4 posttransformMatrix = INIT_MAT4
		(
			0.5, 0.0, 0.0, 0.5,
			0.0, 0.0, 0.5, 0.5,
			0.0, 0.0, 0.0, 1.0,
			0.0, 0.0, 0.0, 1.0
		);

		glm_mat4_copy(posttransformMatrix, posttransformMatrix_out);
		break;
	}
	case 2:
	{
		float offsetA = animation->par1;
		float offsetB = animation->par2;
		float scaleA = animation->par3;
		float scaleB = animation->par4;

		float uOffset = (secondsmod900 * scaleA) + offsetA;
		float vOffset = (secondsmod900 * scaleB) + offsetB;

		mat4 pretransformMatrix = INIT_MAT4
		(
			1, 0, 0, uOffset,
			0, 1, 0, vOffset,
			0, 0, 1, 0,
			0, 0, 0, 1
		);


		glm_mat4_copy(pretransformMatrix, pretransformMatrix_out);


		mat4 posttransformMatrix = INIT_MAT4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		glm_mat4_copy(posttransformMatrix, posttransformMatrix_out);
		break;
	}
	case 4:
	{
		float scale = animation->par1;
		float numFrames = animation->par2;
		float step = animation->par3;
		float offset = animation->par4;

		float value = step * scale * (offset + secondsmod900);
		float uv_offset = (float)(short)(float)(numFrames * fmod(value, 1.0f)) * step;

		mat4 pretransformMatrix = INIT_MAT4
		(
			1, 0, 0, uv_offset,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);


		glm_mat4_copy(pretransformMatrix, pretransformMatrix_out);


		mat4 posttransformMatrix = INIT_MAT4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		glm_mat4_copy(posttransformMatrix, posttransformMatrix_out);
		break;
	}
	case 5:
	{
		float scale = animation->par1;
		float numFrames = animation->par2;
		float step = animation->par3;
		float offset = animation->par4;

		float value = step * scale * (offset + secondsmod900);
		float uv_offset = (float)(short)(float)(numFrames * fmod(value, 1.0f)) * step;

		mat4 pretransformMatrix = INIT_MAT4
		(
			1, 0, 0, 0,
			0, 1, 0, uv_offset,
			0, 0, 1, 0,
			0, 0, 0, 1
		);


		glm_mat4_copy(pretransformMatrix, pretransformMatrix_out);


		mat4 posttransformMatrix = INIT_MAT4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		glm_mat4_copy(posttransformMatrix, posttransformMatrix_out);
		break;
	}
	case 7:
	{
		//todo: this section seems broken
		float ParamA = animation->par1;
		float ParamB = animation->par2;


		mat4 pretransformMatrix = INIT_MAT4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		mat4 view_inv;
		glm_mat4_inv(cameraViewMat, view_inv);

		glm_mat4_mul(view_inv, ObjectPositions[modelIndex].WorldMat, pretransformMatrix);

		pretransformMatrix[0][3] = pretransformMatrix[1][3] = pretransformMatrix[2][3] = 0;

		glm_mat4_copy(pretransformMatrix, pretransformMatrix_out);

		float xy = (cameraViewMat[0][3] + cameraViewMat[1][3]) * 0.025f * ParamB;

		xy = (xy - (int)xy);

		float z = cameraViewMat[2][3] * 0.05f * ParamB;

		z = (z - (int)z);

		float halfA = ParamA * 0.5f;

		mat4 posttransformMatrix = INIT_MAT4
		(
			halfA, 0.0f, 0.0f, xy,
			0.0f, 0.0f, halfA, z,
			0.0f, 0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
		glm_mat4_copy(posttransformMatrix, posttransformMatrix_out);
		break;
	}
	}
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static LARGE_INTEGER tickCount = { 0 };
	static double movementFactor = 0;

	static bool up = false;
	static bool down = false;
	static bool left = false;
	static bool right = false;
	static bool climb = false;
	static bool fall = false;
	static bool sonic = false;
	static bool toggle = false;
	RECT ClientRect;
	switch (msg)
	{
	case WM_KEYUP:
		if (wParam == 'T')
		{
			toggle = !toggle;
		}
		if (wParam == 'F')
		{
			sonic = false;
		}
		if (wParam == 'W')
		{
			up = false;
		}
		else if (wParam == 'A')
		{
			left = false;
		}
		else if (wParam == 'S')
		{
			down = false;
		}
		else if (wParam == 'D')
		{
			right = false;
		}
		else if (wParam == VK_SPACE)
		{

			Player.velocity[2] = Settings.VerticalJumpAccel;
			climb = false;
		}
		else if (wParam == VK_SHIFT)
		{
			fall = false;
		}
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			Running = false;
			DestroyWindow(hwnd);
		}
		else if (wParam == 'V')
		{
			bVsync = !bVsync;
		}
		else if (wParam == 'F')
		{
			sonic = true;
		}
		else if (wParam == 'G')
		{
			Player.currentAreaIndex++;
		}
		else if (wParam == 'W')
		{
			up = true;
		}
		else if (wParam == 'A')
		{
			left = true;
		}
		else if (wParam == 'S')
		{
			down = true;
		}
		else if (wParam == 'D')
		{
			right = true;
		}
		else if (wParam == VK_SPACE)
		{
			climb = true;
		}
		else if (wParam == VK_SHIFT)
		{
			fall = true;
		}
		else if (wParam == 'O')
		{
			WaitForPreviousFrame();
		}
		break;
	case WM_SIZE:
		if (IsIconic(hwnd))
		{
			THROW_ON_FALSE(SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)IdleProc) != 0);
			break;
		}

		THROW_ON_FALSE(GetClientRect(hwnd, &ClientRect));

		if (Width != (ClientRect.right - ClientRect.left) || Height != (ClientRect.bottom - ClientRect.top))
		{
			Width = ClientRect.right - ClientRect.left;
			Height = ClientRect.bottom - ClientRect.top;
			WaitForPreviousFrame();

			for (int i = 0; i < FrameBufferCount; i++)
			{
				THROW_ON_FAIL(ID3D12Resource_Release(RenderTargetsMSAA[i]));
				RenderTargetsMSAA[i] = nullptr;
				THROW_ON_FAIL(ID3D12Resource_Release(RenderTargets[i]));
				RenderTargets[i] = nullptr;

				FenceValue[i] = FenceValue[FrameIndex] + 1;
			}
			DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
			THROW_ON_FAIL(IDXGISwapChain4_GetDesc(SwapChain, &swapChainDesc));
			THROW_ON_FAIL(IDXGISwapChain4_ResizeBuffers(SwapChain, FrameBufferCount, Width, Height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

			FrameIndex = IDXGISwapChain4_GetCurrentBackBufferIndex(SwapChain);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = { 0 };
			ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtvDescriptorHeap, &rtvHandle);

			for (int i = 0; i < FrameBufferCount; i++)
			{
				D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
				HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
				HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				HeapProperties.CreationNodeMask = 1;
				HeapProperties.VisibleNodeMask = 1;

				D3D12_RESOURCE_DESC ResourceDesc = { 0 };
				ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				ResourceDesc.Alignment = 0;
				ResourceDesc.Width = Width;
				ResourceDesc.Height = Height;
				ResourceDesc.DepthOrArraySize = 1;
				ResourceDesc.MipLevels = 1;
				ResourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				ResourceDesc.SampleDesc.Count = MSAACount;
				ResourceDesc.SampleDesc.Quality = 0;
				ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

				ID3D12Device_CreateCommittedResource(
					Device,
					&HeapProperties,
					D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
					&ResourceDesc,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					&OptimizedClearValue,
					&IID_ID3D12Resource,
					&RenderTargetsMSAA[i]
				);

				ID3D12Resource_SetName(RenderTargetsMSAA[i], L"Multisampled Render Target (resized)");
				ID3D12Device_CreateRenderTargetView(Device, RenderTargetsMSAA[i], nullptr, rtvHandle);

				rtvHandle.ptr += rtvDescriptorSize;
			}

			for (int i = 0; i < FrameBufferCount; i++)
			{
				THROW_ON_FAIL(IDXGISwapChain3_GetBuffer(SwapChain, i, &IID_ID3D12Resource, &RenderTargets[i]));

				ID3D12Device_CreateRenderTargetView(Device, RenderTargets[i], nullptr, rtvHandle);

				rtvHandle.ptr += rtvDescriptorSize;
			}

			//now the depth buffer:
			ID3D12Resource_Release(DepthStencilBuffer);
			DepthStencilBuffer = nullptr;

			D3D12_RESOURCE_DESC ResourceDescription = { 0 };
			ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			ResourceDescription.Alignment = 0;
			ResourceDescription.Width = Width;
			ResourceDescription.Height = Height;
			ResourceDescription.DepthOrArraySize = 1;
			ResourceDescription.MipLevels = 1;
			ResourceDescription.Format = DXGI_FORMAT_D32_FLOAT;
			ResourceDescription.SampleDesc.Count = MSAACount;
			ResourceDescription.SampleDesc.Quality = 0;
			ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			ResourceDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE DepthOptimizedClearValue = { 0 };
			DepthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			DepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			DepthOptimizedClearValue.DepthStencil.Stencil = 0;

			THROW_ON_FAIL(ID3D12Device_CreatePlacedResource(Device, DepthBufferHeap, 0, &ResourceDescription, D3D12_RESOURCE_STATE_DEPTH_WRITE, &DepthOptimizedClearValue, &IID_ID3D12Resource, &DepthStencilBuffer));


			D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
			ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dsDescriptorHeap, &CpuDescriptorHandle);

			D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilDesc = { 0 };
			DepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			DepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			DepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			ID3D12Device_CreateDepthStencilView(Device, DepthStencilBuffer, &DepthStencilDesc, CpuDescriptorHandle);

			//change some other stuff

			Viewport.TopLeftX = 0;
			Viewport.TopLeftY = 0;
			Viewport.Width = (FLOAT)Width;
			Viewport.Height = (FLOAT)Height;
			Viewport.MinDepth = 0.0F;
			Viewport.MaxDepth = 1.0F;

			ScissorRect.left = 0;
			ScissorRect.top = 0;
			ScissorRect.right = (LONG)Width;
			ScissorRect.bottom = (LONG)Height;

			//update transformation matrices

			glm_perspective_lh_zo(glm_rad(Settings.fov), (float)Width / (float)Height, 0.1f, 1000.0f, cameraProjMat);
		}
		break;
	case WM_PAINT:
		WaitForPreviousFrame();


		LARGE_INTEGER tickCountNow;
		QueryPerformanceCounter(&tickCountNow);
		ULONGLONG tickCountDelta = tickCountNow.QuadPart - tickCount.QuadPart;

		float DeltaTime = (float)tickCountDelta / ProcessorFrequency.QuadPart;

		float secondsmod900 = (float)(tickCountNow.QuadPart % (ProcessorFrequency.QuadPart * 900)) / ProcessorFrequency.QuadPart;
		tickCount.QuadPart = tickCountNow.QuadPart;
		int frameIndex_local = FrameIndex;
		movementFactor = min((tickCountDelta / ((double)ProcessorFrequency.QuadPart)) * 8.f, .13f);

		vec3 cameraUp = { 0.0f , 0.0f, 1.0f };

		vec3 cameraDirection = { -cosf(glm_rad(CameraYaw)), sinf(glm_rad(CameraYaw)), 0 };



		if (up || down || right || left)
		{
			glm_vec3_scale(cameraDirection, movementFactor * (sonic ? 6 : .7), cameraDirection);

			float rotation = 1;

			if ((up ^ down) ^ (left ^ right))
			{
				if (up)
				{
					rotation = 0;
				}
				if (right)
				{
					rotation = 90;
				}
				if (down)
				{
					rotation = 180;
				}
				if (left)
				{
					rotation = 270;
				}

			}
			else
			{
				if (up && right)
				{
					rotation = 45;
				}
				if (down && right)
				{
					rotation = 135;
				}
				if (down && left)
				{
					rotation = 225;
				}
				if (up && left)
				{
					rotation = 315;
				}
			}

			if (rotation != 1)
			{
				glm_vec3_rotate(cameraDirection, -glm_rad(rotation), cameraUp);

				glm_vec3_add(Player.position, cameraDirection, Player.position);
			}
		}

		if (climb ^ fall)
		{
			if (climb)
			{
				Player.position[2] += movementFactor * (sonic ? 6 : .7);
			}
			else if (fall)
			{
				Player.position[2] -= movementFactor * (sonic ? 6 : .7);
			}
		}
		
		cameraPosition[0] = Player.position[0];
		cameraPosition[1] = Player.position[1];
		cameraPosition[2] = Player.position[2] + Settings.PlayerHeight;
		
		if (GetForegroundWindow() == hwnd)
		{
			POINT newCursorPos;
			THROW_ON_FALSE(GetCursorPos(&newCursorPos));

			CameraYaw += movementFactor * .5 * (newCursorPos.x - cursorPos.x);
			CameraPitch += movementFactor * .5 * (newCursorPos.y - cursorPos.y);
			CameraPitch = max(CameraPitch, -89.99f);
			CameraPitch = min(CameraPitch, 89.99f);
			SetCursorPos(cursorPos.x, cursorPos.y);
		}


		vec3 OffsetFromCamera = {1, 0, 0};
		
		glm_vec3_rotate(OffsetFromCamera, glm_rad(CameraPitch), (vec3) { 0, 1, 0 });
		glm_vec3_rotate(OffsetFromCamera, glm_rad(CameraYaw), (vec3) { 0, 0, 1 });

		cameraPosition[0] *= -1;

		vec3 cameraFront;

		glm_vec3_add(cameraPosition, OffsetFromCamera, cameraFront);

		glm_lookat_lh(cameraPosition, cameraFront, cameraUp, cameraViewMat);

		cameraViewMat[0][0] *= -1.f;
		cameraViewMat[0][1] *= -1.f;
		cameraViewMat[0][2] *= -1.f;
		cameraViewMat[0][3] *= -1.f;


		cameraPosition[0] *= -1;

		glm_mat4_identity(cbPerObject.model);
		glm_mat4_copy(cameraViewMat, cbPerObject.view);
		glm_mat4_copy(cameraProjMat, cbPerObject.projection);
		glm_mat4_inv(cameraViewMat, cbPerObject.view_inv);

		cbPerObject.secondsmod900 = secondsmod900;
		memcpy(perObjectCbvGPUAddress[FrameIndex], &cbPerObject, sizeof(cbPerObject));

		for (int areaNum = 0; areaNum < LoadedLevel.areaCount; areaNum++)
		{
			if (Models[LoadedLevel.areas[areaNum].modelIndex]->AABB[0] < Player.position[0] &&
				Models[LoadedLevel.areas[areaNum].modelIndex]->AABB[1] < Player.position[1] &&
				Models[LoadedLevel.areas[areaNum].modelIndex]->AABB[2] < Player.position[2] &&
				Models[LoadedLevel.areas[areaNum].modelIndex]->AABB[3] > Player.position[0] &&
				Models[LoadedLevel.areas[areaNum].modelIndex]->AABB[4] > Player.position[1] &&
				Models[LoadedLevel.areas[areaNum].modelIndex]->AABB[5] > Player.position[2])
			{
				Player.currentAreaIndex = areaNum;
				break;
			}
		}

		nonStaticObjectCount = 0;
		for (int areaNum = 0; areaNum < LoadedLevel.areaCount; areaNum++)
		{
			for (int scriptLayerNum = 0; scriptLayerNum < LoadedLevel.areas[areaNum].scriptLayerCount; scriptLayerNum++)
			{
				for (int scriptObjectNum = 0; scriptObjectNum < LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectCount; scriptObjectNum++)
				{
					switch (LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].objectType)
					{
					case 0x0:
					{
						struct Script_Actor* actor = LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].typeSpecificData;

						if (actor->CmdlID == 0xFFFFFFFF)
							continue;

						mat4 nonStaticObjectPosition = GLM_MAT4_IDENTITY_INIT;

						glm_translate(nonStaticObjectPosition, actor->pos);


						glm_rotate_z(nonStaticObjectPosition, glm_rad(actor->rot[2]), nonStaticObjectPosition);
						glm_rotate_y(nonStaticObjectPosition, glm_rad(actor->rot[1]), nonStaticObjectPosition);
						glm_rotate_x(nonStaticObjectPosition, glm_rad(actor->rot[0]), nonStaticObjectPosition);


						glm_scale(nonStaticObjectPosition, actor->scale);

						glm_mat4_copy(nonStaticObjectPosition, cbPerObject.model);
						glm_mat4_copy(cameraViewMat, cbPerObject.view);
						glm_mat4_copy(cameraProjMat, cbPerObject.projection);
						glm_mat4_inv(cameraViewMat, cbPerObject.view_inv);


						cbPerObject.secondsmod900 = secondsmod900;

						nonStaticObjects[nonStaticObjectCount].constBufferIndex = nonStaticObjectCount + 1;

						memcpy(perObjectCbvGPUAddress[FrameIndex] + PerObjectDataAlignedSize * nonStaticObjects[nonStaticObjectCount].constBufferIndex, &cbPerObject, sizeof(cbPerObject));

						nonStaticObjects[nonStaticObjectCount].modelIndex = actor->CmdlID;

						nonStaticObjects[nonStaticObjectCount].position[0] = actor->pos[0] + Models[actor->CmdlID]->pod->centroid[0];
						nonStaticObjects[nonStaticObjectCount].position[1] = actor->pos[1] + Models[actor->CmdlID]->pod->centroid[1];
						nonStaticObjects[nonStaticObjectCount].position[2] = actor->pos[2] + Models[actor->CmdlID]->pod->centroid[2];

						nonStaticObjectCount++;
						break;
					}
					case 0x8:
					{
						struct Script_Platform* platform = LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].typeSpecificData;

						if (platform->CmdlID == 0xFFFFFFFF)
							continue;

						mat4 nonStaticObjectPosition = GLM_MAT4_IDENTITY_INIT;

						glm_translate(nonStaticObjectPosition, platform->pos);


						glm_rotate_z(nonStaticObjectPosition, glm_rad(platform->rot[2]), nonStaticObjectPosition);
						glm_rotate_y(nonStaticObjectPosition, glm_rad(platform->rot[1]), nonStaticObjectPosition);
						glm_rotate_x(nonStaticObjectPosition, glm_rad(platform->rot[0]), nonStaticObjectPosition);


						glm_scale(nonStaticObjectPosition, platform->scale);


						glm_mat4_copy(nonStaticObjectPosition, cbPerObject.model);
						glm_mat4_copy(cameraViewMat, cbPerObject.view);
						glm_mat4_copy(cameraProjMat, cbPerObject.projection);
						glm_mat4_inv(cameraViewMat, cbPerObject.view_inv);


						cbPerObject.secondsmod900 = secondsmod900;

						nonStaticObjects[nonStaticObjectCount].constBufferIndex = nonStaticObjectCount + 1;

						memcpy(perObjectCbvGPUAddress[FrameIndex] + PerObjectDataAlignedSize * nonStaticObjects[nonStaticObjectCount].constBufferIndex, &cbPerObject, sizeof(cbPerObject));

						nonStaticObjects[nonStaticObjectCount].modelIndex = platform->CmdlID;

						nonStaticObjects[nonStaticObjectCount].position[0] = platform->pos[0] + Models[platform->CmdlID]->pod->centroid[0];
						nonStaticObjects[nonStaticObjectCount].position[1] = platform->pos[1] + Models[platform->CmdlID]->pod->centroid[1];
						nonStaticObjects[nonStaticObjectCount].position[2] = platform->pos[2] + Models[platform->CmdlID]->pod->centroid[2];

						nonStaticObjectCount++;
						break;

					}
					case 0x4E:
					{
						if (Player.currentAreaIndex == areaNum)
						{
							struct Script_AreaAttributes* areaAttributes = LoadedLevel.areas[areaNum].scriptLayerBuffer[scriptLayerNum].scriptObjectBuffer[scriptObjectNum].typeSpecificData;

							if (areaAttributes->showSkybox)
							{
								mat4 skyboxPosition = GLM_MAT4_IDENTITY_INIT;

								glm_translate(skyboxPosition, cameraPosition);

								glm_scale(skyboxPosition, (vec3) { 10, 10, 10 });

								glm_mat4_copy(skyboxPosition, cbPerObject.model);
								glm_mat4_copy(cameraViewMat, cbPerObject.view);
								glm_mat4_copy(cameraProjMat, cbPerObject.projection);
								glm_mat4_inv(cameraViewMat, cbPerObject.view_inv);

								cbPerObject.secondsmod900 = secondsmod900;

								nonStaticObjects[nonStaticObjectCount].constBufferIndex = nonStaticObjectCount + 1;

								memcpy(perObjectCbvGPUAddress[FrameIndex] + PerObjectDataAlignedSize * nonStaticObjects[nonStaticObjectCount].constBufferIndex, &cbPerObject, sizeof(cbPerObject));


								nonStaticObjects[nonStaticObjectCount].modelIndex = areaAttributes->skyboxModel;


								nonStaticObjectCount++;
							}
						}
						break;

					}
					default:
						break;
					}
				}
			}
		}

		THROW_ON_FAIL(ID3D12CommandAllocator_Reset(CommandAllocator[FrameIndex]));

		THROW_ON_FAIL(ID3D12GraphicsCommandList_Reset(GraphicsCommandList, CommandAllocator[FrameIndex], PipelineStateObject_FinalStage));

		{
			D3D12_RESOURCE_BARRIER Barrier = { 0 };
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.Transition.pResource = RenderTargets[FrameIndex];
			Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, 1, &Barrier);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE HeapStart;
		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtvDescriptorHeap, &HeapStart);

		D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle_MSAA = { 0 };
		D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = { 0 };

		RtvHandle_MSAA.ptr = HeapStart.ptr + ((SIZE_T)FrameIndex * rtvDescriptorSize);

		RtvHandle.ptr = HeapStart.ptr + (FrameBufferCount * rtvDescriptorSize) + ((SIZE_T)FrameIndex * rtvDescriptorSize);

		D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle;
		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dsDescriptorHeap, &DsvHandle);

		int CurrentPipelineIndex = 0;
		ID3D12GraphicsCommandList_SetPipelineState(GraphicsCommandList, PipelineStates[CurrentPipelineIndex].pso);

		ID3D12GraphicsCommandList_SetGraphicsRootSignature(GraphicsCommandList, RootSignature);
		ID3D12GraphicsCommandList_OMSetRenderTargets(GraphicsCommandList, 1, &RtvHandle_MSAA, FALSE, &DsvHandle);

		ID3D12GraphicsCommandList_ClearRenderTargetView(GraphicsCommandList, RtvHandle_MSAA, &OptimizedClearValue.Color, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dsDescriptorHeap, &CpuDescriptorHandle);
		ID3D12GraphicsCommandList_ClearDepthStencilView(GraphicsCommandList, CpuDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		ID3D12GraphicsCommandList_ClearDepthStencilView(GraphicsCommandList, CpuDescriptorHandle, D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		ID3D12DescriptorHeap* DescriptorHeaps[] = { TextureDescriptorHeap };
		ID3D12GraphicsCommandList_SetDescriptorHeaps(GraphicsCommandList, countof(DescriptorHeaps), DescriptorHeaps);

		ID3D12GraphicsCommandList_RSSetViewports(GraphicsCommandList, 1, &Viewport);
		ID3D12GraphicsCommandList_RSSetScissorRects(GraphicsCommandList, 1, &ScissorRect);
		ID3D12GraphicsCommandList_IASetPrimitiveTopology(GraphicsCommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandle;
		ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(TextureDescriptorHeap, &GpuDescriptorHandle);

		int transparentObjectCount = 0;

		D3D12_GPU_VIRTUAL_ADDRESS VertexShaderConstantBufferLocation = ID3D12Resource_GetGPUVirtualAddress(VertexShaderConstantBuffer[FrameIndex]);
		D3D12_GPU_VIRTUAL_ADDRESS PixelShaderConstantBufferLocation = ID3D12Resource_GetGPUVirtualAddress(PixelShaderConstantBuffer[FrameIndex]);

		ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(GraphicsCommandList, 2, GpuDescriptorHandle);

		ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
			GraphicsCommandList,
			0,
			ID3D12Resource_GetGPUVirtualAddress(PerObjectConstantBuffer[FrameIndex])
		);
		
		int worldModelNum_Total = 0;

		for (int areaNum = 0; areaNum < LoadedLevel.areaCount; areaNum++)
		{
			const int modelIndex = LoadedLevel.areas[areaNum].modelIndex;
			int MaterialIndexOffset = Models[modelIndex]->firstMaterialIndex;

			ID3D12GraphicsCommandList_IASetVertexBuffers(GraphicsCommandList, 0, 1, &VertexBufferView[modelIndex]);
			ID3D12GraphicsCommandList_IASetIndexBuffer(GraphicsCommandList, &IndexBufferView[modelIndex]);

			int AreaSurfaceNum = 0;
			int previousMaterialIndex = 0x7FFFFFFF;
			for (int worldModelNum = 0; worldModelNum < LoadedLevel.areas[areaNum].worldModelCount; worldModelNum++)
			{
				if ((LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].visorFlags & 0x2) != 0)
				{
					worldModelNum_Total++;
					continue;
				}

				bool IsCameraInOcclusionMesh = false;

				if (LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[0] < cameraPosition[0] &&
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[1] < cameraPosition[1] &&
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[2] < cameraPosition[2] &&
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[3] > cameraPosition[0] &&
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[4] > cameraPosition[1] &&
					LoadedLevel.areas[areaNum].perWorldModelInfo[worldModelNum].AABB[5] > cameraPosition[2])
				{
					IsCameraInOcclusionMesh = true;
				}

				if(!IsCameraInOcclusionMesh)
					ID3D12GraphicsCommandList_SetPredication(GraphicsCommandList, PredicationBuffer, worldModelNum_Total * 8, D3D12_PREDICATION_OP_EQUAL_ZERO);


				for (int worldModelSurfaceNum = 0; worldModelSurfaceNum < LoadedLevel.areas[areaNum].worldModelSurfaceCounts[worldModelNum]; worldModelSurfaceNum++, AreaSurfaceNum++)
				{
					if (Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].animationCount != 0)
					{
						struct VertexConstantBuffer vcb;
						memcpy(&vcb, vertexCbvGPUAddress[FrameIndex] + VertexPerObjectConstantsAlignedSize * (Models[modelIndex]->pod[AreaSurfaceNum].materialIndex + MaterialIndexOffset), sizeof(vcb));


						for (int animNum = 0; animNum < Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].animationCount; animNum++)
						{
							GenerateTexgenTransforms(
								secondsmod900,
								vcb.pretransformMatrices[animNum + 1],
								vcb.posttransformMatrices[animNum + 1],
								&Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].animations[animNum],
								modelIndex
							);
						}
						memcpy(vertexCbvGPUAddress[FrameIndex] + VertexPerObjectConstantsAlignedSize * (Models[modelIndex]->pod[AreaSurfaceNum].materialIndex + MaterialIndexOffset), &vcb, sizeof(vcb));
					}

					if (Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].occlusionMesh)
					{
						continue;
					}

					if (Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].transparent)
					{
						transparentObjectBuffer[transparentObjectCount].modelIndex = modelIndex;
						transparentObjectBuffer[transparentObjectCount].objectIndex = AreaSurfaceNum;
						transparentObjectBuffer[transparentObjectCount].constantBufferIndex = 0;
						transparentObjectBuffer[transparentObjectCount].objectDistance = glm_vec3_distance(Models[modelIndex]->pod[AreaSurfaceNum].centroid, cameraPosition);
						transparentObjectCount++;
						continue;
					}

					if (previousMaterialIndex != Models[modelIndex]->pod[AreaSurfaceNum].materialIndex)
					{
						if (Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].pipelineStateIndex != CurrentPipelineIndex)
						{
							CurrentPipelineIndex = Models[modelIndex]->pmd[Models[modelIndex]->pod[AreaSurfaceNum].materialIndex].pipelineStateIndex;
							ID3D12GraphicsCommandList_SetPipelineState(GraphicsCommandList, PipelineStates[CurrentPipelineIndex].pso);
						}

						ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
							GraphicsCommandList,
							1,
							VertexShaderConstantBufferLocation + VertexPerObjectConstantsAlignedSize * (Models[modelIndex]->pod[AreaSurfaceNum].materialIndex + MaterialIndexOffset));

						ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
							GraphicsCommandList,
							3,
							PixelShaderConstantBufferLocation + PixelPerObjectConstantsAlignedSize * (Models[modelIndex]->pod[AreaSurfaceNum].materialIndex + MaterialIndexOffset));

						previousMaterialIndex = Models[modelIndex]->pod[AreaSurfaceNum].materialIndex;
					}

					ID3D12GraphicsCommandList_DrawIndexedInstanced(
						GraphicsCommandList,
						Models[modelIndex]->pod[AreaSurfaceNum].surfaceSize,
						1,
						Models[modelIndex]->pod[AreaSurfaceNum].surfaceOffset,
						0,
						0
					);
				}

				if (!IsCameraInOcclusionMesh)
					ID3D12GraphicsCommandList_SetPredication(GraphicsCommandList, nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);

				worldModelNum_Total++;
			}
		}

		ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
			GraphicsCommandList,
			0,
			ID3D12Resource_GetGPUVirtualAddress(PerObjectConstantBuffer[FrameIndex])
		);

		ID3D12GraphicsCommandList_SetPipelineState(GraphicsCommandList, predicationPipeline);
		CurrentPipelineIndex = 0xFFFF;

		worldModelNum_Total = 0;

		ID3D12GraphicsCommandList_IASetVertexBuffers(GraphicsCommandList, 0, 1, &OcclusionMeshVertexBufferView);
		ID3D12GraphicsCommandList_IASetIndexBuffer(GraphicsCommandList, &OcclusionMeshIndexBufferView);

		for (int areaNum = 0; areaNum < LoadedLevel.areaCount; areaNum++)
		{
			for (int worldModelNum = 0; worldModelNum < LoadedLevel.areas[areaNum].worldModelCount; worldModelNum++)
			{

				ID3D12GraphicsCommandList_BeginQuery(
					GraphicsCommandList,
					PredicationQueryHeap,
					D3D12_QUERY_TYPE_BINARY_OCCLUSION,
					worldModelNum_Total
				);

				ID3D12GraphicsCommandList_DrawIndexedInstanced(
					GraphicsCommandList,
					36,
					1,
					0,
					worldModelNum_Total * 8,
					0
				);

				ID3D12GraphicsCommandList_EndQuery(
					GraphicsCommandList,
					PredicationQueryHeap,
					D3D12_QUERY_TYPE_BINARY_OCCLUSION,
					worldModelNum_Total
				);

				worldModelNum_Total++;
			}
		}

		for (int nonstaticObjectNum = 0; nonstaticObjectNum < nonStaticObjectCount; nonstaticObjectNum++)
		{
			const int nonstaticModelIndex = nonStaticObjects[nonstaticObjectNum].modelIndex;

			if (nonstaticModelIndex == 0xFFFFFFFF)
				continue;

			int MaterialIndexOffset = Models[nonstaticModelIndex]->firstMaterialIndex;

			ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
				GraphicsCommandList,
				0,
				ID3D12Resource_GetGPUVirtualAddress(PerObjectConstantBuffer[FrameIndex]) + nonStaticObjects[nonstaticObjectNum].constBufferIndex * PerObjectDataAlignedSize
			);


			ID3D12GraphicsCommandList_IASetVertexBuffers(GraphicsCommandList, 0, 1, &VertexBufferView[nonstaticModelIndex]);
			ID3D12GraphicsCommandList_IASetIndexBuffer(GraphicsCommandList, &IndexBufferView[nonstaticModelIndex]);


			for (int SurfaceNum = 0; SurfaceNum < Models[nonstaticModelIndex]->surfaceCount; SurfaceNum++)
			{
				if (Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].animationCount != 0)
				{
					struct VertexConstantBuffer vcb;
					memcpy(&vcb, vertexCbvGPUAddress[FrameIndex] + VertexPerObjectConstantsAlignedSize * (Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex + MaterialIndexOffset), sizeof(vcb));

					for (int animNum = 0; animNum < Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].animationCount; animNum++)
					{
						GenerateTexgenTransforms(
							secondsmod900,
							vcb.pretransformMatrices[animNum + 1],
							vcb.posttransformMatrices[animNum + 1],
							&Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].animations[animNum],
							nonstaticModelIndex
						);
					}
					
					memcpy(vertexCbvGPUAddress[FrameIndex] + VertexPerObjectConstantsAlignedSize * (Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex + MaterialIndexOffset), &vcb, sizeof(vcb));
				}

				if (Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].occlusionMesh)
				{
					continue;
				}

				if (Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].transparent)
				{
					transparentObjectBuffer[transparentObjectCount].modelIndex = nonstaticModelIndex;
					transparentObjectBuffer[transparentObjectCount].objectIndex = SurfaceNum;
					transparentObjectBuffer[transparentObjectCount].constantBufferIndex = nonStaticObjects[nonstaticObjectNum].constBufferIndex;
					transparentObjectBuffer[transparentObjectCount].objectDistance = glm_vec3_distance(nonStaticObjects[nonstaticObjectNum].position, cameraPosition);
					transparentObjectCount++;
					continue;
				}

				if (Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].pipelineStateIndex != CurrentPipelineIndex)
				{
					CurrentPipelineIndex = Models[nonstaticModelIndex]->pmd[Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex].pipelineStateIndex;
					ID3D12GraphicsCommandList_SetPipelineState(GraphicsCommandList, PipelineStates[CurrentPipelineIndex].pso);
				}

				ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
					GraphicsCommandList,
					1,
					VertexShaderConstantBufferLocation + VertexPerObjectConstantsAlignedSize * (Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex + MaterialIndexOffset));

				ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
					GraphicsCommandList,
					3,
					PixelShaderConstantBufferLocation + PixelPerObjectConstantsAlignedSize * (Models[nonstaticModelIndex]->pod[SurfaceNum].materialIndex + MaterialIndexOffset));

				ID3D12GraphicsCommandList_DrawIndexedInstanced(
					GraphicsCommandList,
					Models[nonstaticModelIndex]->pod[SurfaceNum].surfaceSize,
					1,
					Models[nonstaticModelIndex]->pod[SurfaceNum].surfaceOffset,
					0,
					0
				);
			}
		}

		if (transparentObjectCount != 0)
		{
			quickSort(transparentObjectBuffer, 0, transparentObjectCount - 1);

			int ModelNum = 0xFFFF;
			for (int i = transparentObjectCount - 1; i > -1; i--)
			{
				if (ModelNum != transparentObjectBuffer[i].modelIndex)
				{
					ModelNum = transparentObjectBuffer[i].modelIndex;
					ID3D12GraphicsCommandList_IASetVertexBuffers(GraphicsCommandList, 0, 1, &VertexBufferView[ModelNum]);
					ID3D12GraphicsCommandList_IASetIndexBuffer(GraphicsCommandList, &IndexBufferView[ModelNum]);
				}

				int SurfaceNum = transparentObjectBuffer[i].objectIndex;

				if (Models[ModelNum]->pmd[Models[ModelNum]->pod[SurfaceNum].materialIndex].pipelineStateIndex != CurrentPipelineIndex)
				{
					CurrentPipelineIndex = Models[ModelNum]->pmd[Models[ModelNum]->pod[SurfaceNum].materialIndex].pipelineStateIndex;
					ID3D12GraphicsCommandList_SetPipelineState(GraphicsCommandList, PipelineStates[CurrentPipelineIndex].pso);
				}

				ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
					GraphicsCommandList,
					0,
					ID3D12Resource_GetGPUVirtualAddress(PerObjectConstantBuffer[FrameIndex]) + transparentObjectBuffer[i].constantBufferIndex * PerObjectDataAlignedSize
				);
				
				ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
					GraphicsCommandList,
					1,
					VertexShaderConstantBufferLocation + VertexPerObjectConstantsAlignedSize * (Models[ModelNum]->pod[SurfaceNum].materialIndex + Models[ModelNum]->firstMaterialIndex));

				ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
					GraphicsCommandList,
					3,
					PixelShaderConstantBufferLocation + PixelPerObjectConstantsAlignedSize * (Models[ModelNum]->pod[SurfaceNum].materialIndex + Models[ModelNum]->firstMaterialIndex));

				ID3D12GraphicsCommandList_DrawIndexedInstanced(
					GraphicsCommandList,
					Models[ModelNum]->pod[SurfaceNum].surfaceSize,
					1,
					Models[ModelNum]->pod[SurfaceNum].surfaceOffset,
					0,
					0
				);
			}
		}

		

		ID3D12GraphicsCommandList_SetPipelineState(GraphicsCommandList, PipelineStateObject_FinalStage);
		ID3D12GraphicsCommandList_OMSetRenderTargets(GraphicsCommandList, 1, &RtvHandle, FALSE, nullptr);
		ID3D12GraphicsCommandList_ClearRenderTargetView(GraphicsCommandList, RtvHandle, &OptimizedClearValue.Color, 0, nullptr);

		
		{
			D3D12_RESOURCE_BARRIER Barriers[3] = { 0 };
			Barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barriers[0].Transition.pResource = RenderTargetsMSAA[FrameIndex];
			Barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			Barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

			Barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barriers[1].Transition.pResource = RenderTargets[FrameIndex];
			Barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			Barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;

			Barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barriers[2].Transition.pResource = PredicationBuffer;
			Barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_PREDICATION;
			Barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			Barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, countof(Barriers), Barriers);
		}


		ID3D12GraphicsCommandList_ResolveSubresource(GraphicsCommandList, RenderTargets[FrameIndex], 0, RenderTargetsMSAA[FrameIndex], 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		
		ID3D12GraphicsCommandList_ResolveQueryData(GraphicsCommandList, PredicationQueryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, worldModelNum_Total, PredicationBuffer, 0);

		{
			D3D12_RESOURCE_BARRIER Barriers[3] = { 0 };
			Barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barriers[0].Transition.pResource = PredicationBuffer;
			Barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			Barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PREDICATION;
			Barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		
			Barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barriers[1].Transition.pResource = RenderTargetsMSAA[FrameIndex];
			Barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
			Barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			Barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barriers[2].Transition.pResource = RenderTargets[FrameIndex];
			Barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
			Barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;


			ID3D12GraphicsCommandList_ResourceBarrier(GraphicsCommandList, countof(Barriers), Barriers);
		}

		THROW_ON_FAIL(ID3D12GraphicsCommandList_Close(GraphicsCommandList));

		ID3D12CommandList* ppCommandLists[] = { GraphicsCommandList };

		ID3D12CommandQueue_ExecuteCommandLists(CommandQueue, countof(ppCommandLists), ppCommandLists);

		THROW_ON_FAIL(ID3D12CommandQueue_Signal(CommandQueue, fence[FrameIndex], FenceValue[FrameIndex]));

		THROW_ON_FAIL(IDXGISwapChain3_Present(SwapChain, bVsync ? 1 : 0, (bTearingSupport && !bVsync) ? DXGI_PRESENT_ALLOW_TEARING : 0));

		if (frameIndex_local != FrameIndex)
			DebugBreak();
		break;

	case WM_DESTROY:
		Running = false;
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
	return 0;
}

void WaitForPreviousFrame()
{
	FrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(SwapChain);
	THROW_ON_FAIL(ID3D12CommandQueue_Signal(CommandQueue, fence[FrameIndex], ++FenceValue[FrameIndex]));

	if (ID3D12Fence_GetCompletedValue(fence[FrameIndex]) < FenceValue[FrameIndex])
	{
		if (FAILED(ID3D12Fence_SetEventOnCompletion(fence[FrameIndex], FenceValue[FrameIndex], FenceEvent)))
		{
			Running = false;
		}

		WaitForSingleObject(FenceEvent, INFINITE);
	}
}