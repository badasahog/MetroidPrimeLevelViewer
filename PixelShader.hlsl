#define GX_TEVPREV 0

#define GX_TEVREG0 1

#define GX_TEVREG1 2

#define GX_TEVREG2 3

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

struct TEVConstantBuffer
{
    int Tex0Index;
    int Tex1Index;
    int Tex2Index;
    int Tex3Index;
    int Tex4Index;
    int Tex5Index;
    int Tex6Index;
    int Tex7Index;
    int Tex8Index;
    int Tex9Index;
    int Tex10Index;
    int Tex11Index;
    int Tex12Index;
    int Tex13Index;
    int Tex14Index;
    int Tex15Index;
};

ConstantBuffer<TEVConstantBuffer> Materialcb : register(b0, space0);
Texture2D textures[] : register(t0);
SamplerState mySampler : register(s0);


float4 main(VS_OUTPUT input) : SV_TARGET
{
    return float4(0, 0, input.pos.w * .001, 1);
}