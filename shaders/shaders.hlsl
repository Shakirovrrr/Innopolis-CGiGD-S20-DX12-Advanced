cbuffer ConstantBuffer : register(b0) {
	float4x4 mwpMatrix;
	float4 light;
}

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PSInput {
	float4 position : SV_POSITION;
	float4 color : COLOR0;
	float4 intensity : COLOR1;
	float2 uv : TEXCOORD;
};

PSInput VSMain(float4 position : POSITION, float4 diffuseColor : DIFFUSE, float4 normal : NORMAL, float4 texcoord : TEXCOORD) {
	PSInput result;

	result.position = mul(mwpMatrix, position);
	result.color = diffuseColor;
	result.intensity = dot(normal, normalize(light - position.xyz)); //clamp(3.0f - length(position - light) / 3.0f, 0.0f, 1.0f);
	result.uv = texcoord.xy;

	return result;
}

float4 PSMain_texture(PSInput input) : SV_TARGET {
	return clamp(g_texture.Sample(g_sampler, input.uv) * input.intensity, 0.0f, 1.0f);
}

float4 PSMain_color(PSInput input) : SV_TARGET {
	return clamp(input.color * input.intensity, 0.0f, 1.0f);
}
