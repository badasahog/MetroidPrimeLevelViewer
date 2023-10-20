struct VS_INPUT
{
    float4 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
    float2 uv4 : TEXCOORD4;
    float2 uv5 : TEXCOORD5;
    float2 uv6 : TEXCOORD6;
    float2 uv7 : TEXCOORD7;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 nrm : NORMAL;
    float3 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
    float2 uv4 : TEXCOORD4;
    float2 uv5 : TEXCOORD5;
    float2 uv6 : TEXCOORD6;
    float2 uv7 : TEXCOORD7;
};

struct Texgen
{
    int TexCoordGenType;
    int TexCoordSource;
    int PreTransformIndex;
    int bNormalize;
    int PostTransformIndex;
};

struct Material
{
    float4x4 preTransformMatrices[5];
    float4x4 postTransformMatrices[5];
    int TexgenCount;
    Texgen Texgens[5];
	
};

struct PerObjectData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 view_inv;
    float secondsmod900;
};


ConstantBuffer<PerObjectData> pod : register(b0, space0);
ConstantBuffer<Material> Materialcb : register(b1, space0);

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
	
    float4x4 wvpMat = mul(pod.projection, mul(pod.view, pod.model));
    output.pos = mul(wvpMat, input.pos);
	
    return output;
}
