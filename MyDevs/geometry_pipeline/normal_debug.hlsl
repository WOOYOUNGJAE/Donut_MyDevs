/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma pack_matrix(row_major)

#include <donut/shaders/bindless.h>
#include <donut/shaders/view_cb.h>

#ifdef SPIRV
#define VK_PUSH_CONSTANT [[vk::push_constant]]
#define VK_BINDING(reg,dset) [[vk::binding(reg,dset)]]
#else
#define VK_PUSH_CONSTANT
#define VK_BINDING(reg,dset) 
#endif


struct InstanceConstants
{
	uint instance;
	uint geometryInMesh;
};

ConstantBuffer<PlanarViewConstants> g_View : register(b0);
VK_PUSH_CONSTANT ConstantBuffer<InstanceConstants> g_Instance : register(b1);
StructuredBuffer<InstanceData> t_InstanceData : register(t0);
StructuredBuffer<GeometryData> t_GeometryData : register(t1);
StructuredBuffer<MaterialConstants> t_MaterialConstants : register(t2);
SamplerState s_MaterialSampler : register(s0);

VK_BINDING(0, 1) ByteAddressBuffer t_BindlessBuffers[] : register(t0, space1);
VK_BINDING(1, 1) Texture2D t_BindlessTextures[] : register(t0, space2);

struct VSOutput
{
    float4 position : POSITION0;
    float3 normal : NORMAL0;
};

void main_vs(
	in uint i_vertexID : SV_VertexID,
    out VSOutput vsOutput
)
{
    InstanceData instance = t_InstanceData[g_Instance.instance];
    GeometryData geometry = t_GeometryData[instance.firstGeometryIndex + g_Instance.geometryInMesh];

    ByteAddressBuffer indexBuffer = t_BindlessBuffers[geometry.indexBufferIndex];
    ByteAddressBuffer vertexBuffer = t_BindlessBuffers[geometry.vertexBufferIndex];

    uint index = indexBuffer.Load(geometry.indexOffset + i_vertexID * 4);

    float4 objectSpacePosition = float4(asfloat(vertexBuffer.Load3(geometry.positionOffset + index * c_SizeOfPosition)), 1.0);
    float3 objectSpaceNormal = asfloat(vertexBuffer.Load3(geometry.normalOffset + index * c_SizeOfNormal));

    vsOutput.position = objectSpacePosition;
    vsOutput.normal = objectSpaceNormal;
}

struct GSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR0;
};

[maxvertexcount(6)]
void main_gs(
    triangle VSOutput i_gsIn[3],
    inout LineStream<GSOutput> outStream
)
{
    float normalLength = 0.02;
    float3x4 transform = t_InstanceData[g_Instance.instance].transform;

    for (int i = 0; i < 3; ++i)
    {
        //float3 pos
        float3 localPos = i_gsIn[i].position.xyz;
        float3 localNormal = i_gsIn[i].normal.xyz;

        // point of line (p0)
        GSOutput output = (GSOutput)0;
        output.position = mul(mul(float4(localPos, 1.0), transform), g_View.matWorldToClip);
        output.color = float3(1.0, 0.0, 0.0);
        outStream.Append(output);

        // point of line (p1)
        output.position = mul(mul(float4(localPos + localNormal * normalLength, 1.0), transform), g_View.matWorldToClip);
        output.color = float3(0.0, 0.0, 1.0);
        outStream.Append(output);

        outStream.RestartStrip();
    }
}

void main_ps(
    float3 i_color : COLOR0,
    out float4 o_color : SV_Target0
)
{
    o_color = float4(i_color, 1.0);
}