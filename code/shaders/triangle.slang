struct VSInput
{
  float2 inPosition;
  float3 inColor;
  float2 inTexCoord;
};

struct VSOutput
{
  float4 pos : SV_Position;
  float3 fragColor;
  float2 fragTexCoord;
};

struct UniformBuffer
{
  float4x4 model;
  float4x4 view;
  float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VSOutput
vertMain(VSInput input)
{
  VSOutput output;
  float4 pos = float4(input.inPosition, 0.0, 1.0);
  pos = mul(ubo.model, pos);
  pos = mul(ubo.view, pos);
  pos = mul(ubo.proj, pos);
  output.pos = pos;
  output.fragColor = input.inColor;
  output.fragTexCoord = input.inTexCoord;
  return output;
}

Sampler2D texture;
[shader("fragment")]
float4
fragMain(VSOutput inVert)
  : SV_Target
{
  return texture.Sample(inVert.fragTexCoord);
}
