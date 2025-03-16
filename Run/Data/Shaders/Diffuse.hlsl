//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 localPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 localTangent : TANGENT;
	float3 localBitangent : BITANGENT;
	float3 localNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 tangent : TANGENT;
	float4 bitangent : BITANGENT;
	float4 normal : NORMAL;
	float3 worldPosition : WORLD_POSITION;
};

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b1)
{
	float3 SunDirection;
	float SunIntensity;
	float AmbientIntensity;
	float3 padding0;
	float4x4 LightViewMatrix;
	float4x4 LightProjectionMatrix;
	float3 WorldEyePosition;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 ViewMatrix;
	float4x4 ProjectionMatrix;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelMatrix;
	float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
cbuffer ArchiLeapShaderConstants : register(b4)
{
	float4 SkyColor;
	float FogStartDistance;
	float FogEndDistance;
	float FogMaxAlpha;
};

//------------------------------------------------------------------------------------------------
Texture2D diffuseTexture : register(t0);

//------------------------------------------------------------------------------------------------
SamplerState diffuseSampler : register(s0);

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 localPosition = float4(input.localPosition, 1);
	float4 worldPosition = mul(ModelMatrix, localPosition);
	float4 viewPosition = mul(ViewMatrix, worldPosition);
	float4 clipPosition = mul(ProjectionMatrix, viewPosition);
	float4 localNormal = float4(input.localNormal, 0);
	float4 worldNormal = mul(ModelMatrix, localNormal);

	v2p_t v2p;
	v2p.position = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.tangent = float4(0, 0, 0, 0);
	v2p.bitangent = float4(0, 0, 0, 0);
	v2p.normal = worldNormal;
	v2p.worldPosition = worldPosition;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
	float ambient = AmbientIntensity;
	float directional = SunIntensity * saturate(dot(normalize(input.normal.xyz), -SunDirection));
	float4 lightColor = float4((ambient + directional).xxx, 1);
	float4 textureColor = diffuseTexture.Sample(diffuseSampler, input.uv);
	float4 vertexColor = input.color;
	float4 modelColor = ModelColor;
	float4 color = lightColor * textureColor * vertexColor * modelColor;
	clip(color.a - 0.01f);
	
	// Compute the fog
	float3 dispCamToPixel = input.worldPosition.xyz - WorldEyePosition.xyz;
	float distCamToPixel = length( dispCamToPixel );
	float fogDensity = FogMaxAlpha * saturate( (distCamToPixel - FogStartDistance) / (FogEndDistance - FogStartDistance) );
	float3 finalRGB = lerp( color.rgb, SkyColor.rgb, fogDensity );
	float finalAlpha = saturate( color.a + fogDensity ); // fog can add opacity
	float4 finalColor = float4( finalRGB, finalAlpha );
	
	return finalColor;
}
