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

#define INIT_MAT4(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
	a, e, i, m,\
	b, f, j, n,\
	c, g, k, o,\
	d, h, l, p

#define s pod.secondsmod900


ConstantBuffer<PerObjectData> pod : register(b0, space0);
ConstantBuffer<Material> Materialcb : register(b1, space0);

float3 float3from2(float2 input)
{
	return float3(input[0], input[1], 1);
}

float4 float4from2(float2 input)
{
	return float4(input[0], input[1], 0, 1);
}

float4 float4from3(float3 input)
{
	return float4(input[0], input[1], input[2], 1);
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	
	float4x4 wvpMat = mul(pod.projection, mul(pod.view, pod.model));
	output.pos = mul(wvpMat, input.pos);
	
	float4 TexgenInputs[10];
	TexgenInputs[0] = input.pos;
	TexgenInputs[1] = float4from3(input.nrm);
	TexgenInputs[2] = float4from2(input.uv0);
	TexgenInputs[3] = float4from2(input.uv1);
	TexgenInputs[4] = float4from2(input.uv2);
	TexgenInputs[5] = float4from2(input.uv3);
	TexgenInputs[6] = float4from2(input.uv4);
	TexgenInputs[7] = float4from2(input.uv5);
	TexgenInputs[8] = float4from2(input.uv6);
	TexgenInputs[9] = float4from2(input.uv7);
	
	float2 TexgenOutputs[8];
	int i = 0;
	
	for (; i < Materialcb.TexgenCount; i++)
	{
		float4 temp;
		temp = TexgenInputs[Materialcb.Texgens[i].TexCoordSource];
		
		temp = mul(Materialcb.preTransformMatrices[Materialcb.Texgens[i].PreTransformIndex], temp);
		
		float3 temp2 = float3(temp.x / temp.w, temp.y / temp.w, temp.z / temp.w);
		
		if (Materialcb.Texgens[i].bNormalize)
			temp2 = normalize(temp2);
		
		temp2 = mul(Materialcb.postTransformMatrices[Materialcb.Texgens[i].PreTransformIndex], temp2);
		
		TexgenOutputs[i] = temp2;
	}
	
	for (; i < 8; i++)
	{
		TexgenOutputs[i] = float2(0, 0);
	}
	
	output.uv0 = TexgenOutputs[0];
	output.uv1 = TexgenOutputs[1];
	output.uv2 = TexgenOutputs[2];
	output.uv3 = TexgenOutputs[3];
	output.uv4 = TexgenOutputs[4];
	
	output.uv5 = input.uv5;
	output.uv6 = input.uv6;
	output.uv7 = input.uv7;
	
	
	output.nrm = mul((float3x3) pod.view_inv, input.nrm);
	
	output.nrm[0] = (output.nrm[0] + 1) / 2;
	output.nrm[1] = (output.nrm[1] + 1) / 2;
	output.nrm[2] = (output.nrm[2] + 1) / 2;
	
    float3 ambientColor = { 0.113568,0.126453, 0.156000 };
	
    float ambientBrightness = 0.8;
	
    output.color = ambientColor * ambientBrightness;
	
	return output;
}
