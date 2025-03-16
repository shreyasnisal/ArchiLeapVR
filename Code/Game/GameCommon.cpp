#include "Game/GameCommon.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"

VertexBuffer* g_translationBasisVBO = nullptr;
VertexBuffer* g_rotationBasisVBO = nullptr;
VertexBuffer* g_scalingBasisVBO = nullptr;

std::string GetAxisLockDirectionStr(AxisLockDirection axisLockDirection)
{
	switch (axisLockDirection)
	{
		case AxisLockDirection::X:				return "X";
		case AxisLockDirection::Y:				return "Y";
		case AxisLockDirection::Z:				return "Z";
	}

	return "None";
}
