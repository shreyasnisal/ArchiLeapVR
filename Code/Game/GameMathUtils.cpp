#include "Game/GameMathUtils.hpp"

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/OBB2.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"


bool DoZCylinderAndZOBB3Overlap(Vec3 const& cylinderBaseCenter, Vec3 const& cylinderTopCenter, float cylinderRadius, OBB3 const& zOrientedBox)
{
	AABB3 zOrientedBoxInLocalSpace(-zOrientedBox.m_halfDimensions, zOrientedBox.m_halfDimensions);
	Vec3 cylinderBaseCenterInOBB3LocalSpace = zOrientedBox.GetLocalPosForWorldPos(cylinderBaseCenter);
	Vec3 cylinderTopCenterInOBB3LocalSpace = zOrientedBox.GetLocalPosForWorldPos(cylinderTopCenter);

	return DoZCylinderAndAABB3Overlap(cylinderBaseCenterInOBB3LocalSpace, cylinderTopCenterInOBB3LocalSpace, cylinderRadius, zOrientedBoxInLocalSpace);
}

bool PushZCylinderOutOfFixedZOBB3(Vec3& cylinderBaseCenter, Vec3& cylinderTopCenter, float cylinderRadius, OBB3 const& zOrientedBox)
{
	if (!DoZCylinderAndZOBB3Overlap(cylinderBaseCenter, cylinderTopCenter, cylinderRadius, zOrientedBox))
	{
		return false;
	}

	AABB3 zOrientedBoxInLocalSpace(-zOrientedBox.m_halfDimensions, zOrientedBox.m_halfDimensions);

	Vec3 cylinderBaseCenterInOBB3LocalSpace = zOrientedBox.GetLocalPosForWorldPos(cylinderBaseCenter);
	Vec3 cylinderTopCenterInOBB3LocalSpace = zOrientedBox.GetLocalPosForWorldPos(cylinderTopCenter);

	PushZCylinderOutOfFixedAABB3(cylinderBaseCenterInOBB3LocalSpace, cylinderTopCenterInOBB3LocalSpace, cylinderRadius, zOrientedBoxInLocalSpace);

	cylinderBaseCenter = zOrientedBox.GetWorldPosForLocalPos(cylinderBaseCenterInOBB3LocalSpace);
	cylinderTopCenter = zOrientedBox.GetWorldPosForLocalPos(cylinderTopCenterInOBB3LocalSpace);

	return true;
}

bool PushZOBB3OutOfFixedZCylinder(OBB3& zOrientedBox, Vec3 const& cylinderBaseCenter, Vec3 const& cylinderTopCenter, float cylinderRadius)
{
	if (!DoZCylinderAndZOBB3Overlap(cylinderBaseCenter, cylinderTopCenter, cylinderRadius, zOrientedBox))
	{
		return false;
	}

	AABB3 zOrientedBoxInLocalSpace(-zOrientedBox.m_halfDimensions, zOrientedBox.m_halfDimensions);

	Mat44 obbTransformMatrix = Mat44(zOrientedBox.m_iBasis, zOrientedBox.m_jBasis, zOrientedBox.m_kBasis, zOrientedBox.m_center);
	Mat44 obbTransformMatrixInverse = obbTransformMatrix.GetOrthonormalInverse();

	Vec3 cylinderBaseCenterInOBB3LocalSpace = zOrientedBox.GetLocalPosForWorldPos(cylinderBaseCenter);
	Vec3 cylinderTopCenterInOBB3LocalSpace = zOrientedBox.GetLocalPosForWorldPos(cylinderTopCenter);

	PushAABB3OutOfFixedZCylinder(zOrientedBoxInLocalSpace, cylinderBaseCenterInOBB3LocalSpace, cylinderTopCenterInOBB3LocalSpace, cylinderRadius);
	zOrientedBox.m_center = obbTransformMatrix.TransformPosition3D(zOrientedBoxInLocalSpace.GetCenter());

	return true;
}

bool DoZOBB3Overlap(OBB3 const& zOrientedBoxA, OBB3 const& zOrientedBoxB)
{
	Vec3 const& separatingAxisZ = Vec3::SKYWARD;
	FloatRange floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisZ);
	FloatRange floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisZ);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}

	Vec3 const& separatingAxisAi = zOrientedBoxA.m_iBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAi);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAi);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}

	Vec3 const& separatingAxisAj = zOrientedBoxA.m_jBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAj);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAj);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}

	Vec3 const& separatingAxisBi = zOrientedBoxB.m_iBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBi);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBi);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}

	Vec3 const& separatingAxisBj = zOrientedBoxB.m_jBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBj);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBj);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}

	return true;
}

bool PushZOBB3OutOfFixedZOBB3(OBB3& zOrientedBoxA, OBB3 const& zOrientedBoxB)
{
	Vec3 pushDirection = Vec3::ZERO;
	float pushDist = 0.f;

	Vec3 const& separatingAxisZ = Vec3::SKYWARD;
	FloatRange floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisZ);
	FloatRange floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisZ);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}
	if (fabsf(floatRangeA.m_max - floatRangeB.m_min) < fabsf(floatRangeB.m_max - floatRangeA.m_min))
	{
		pushDirection = separatingAxisZ;
		pushDist = floatRangeA.m_max - floatRangeB.m_min;
	}
	else
	{
		pushDirection = -separatingAxisZ;
		pushDist = floatRangeA.m_min - floatRangeB.m_max;
	}

	Vec3 const& separatingAxisAi = zOrientedBoxA.m_iBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAi);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAi);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}
	if (fabsf(floatRangeA.m_max - floatRangeB.m_min) < fabsf(floatRangeB.m_max - floatRangeA.m_min))
	{
		float pushDistForThisAxis = floatRangeA.m_max - floatRangeB.m_min;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = separatingAxisAi;
			pushDist = pushDistForThisAxis;
		}
	}
	else
	{
		float pushDistForThisAxis = floatRangeA.m_min - floatRangeB.m_max;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = -separatingAxisAi;
			pushDist = pushDistForThisAxis;
		}
	}

	Vec3 const& separatingAxisAj = zOrientedBoxA.m_jBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAj);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisAj);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}
	if (fabsf(floatRangeA.m_max - floatRangeB.m_min) < fabsf(floatRangeB.m_max - floatRangeA.m_min))
	{
		float pushDistForThisAxis = floatRangeA.m_max - floatRangeB.m_min;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = separatingAxisAj;
			pushDist = pushDistForThisAxis;
		}
	}
	else
	{
		float pushDistForThisAxis = floatRangeA.m_min - floatRangeB.m_max;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = -separatingAxisAj;
			pushDist = pushDistForThisAxis;
		}
	}

	Vec3 const& separatingAxisBi = zOrientedBoxB.m_iBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBi);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBi);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}
	if (fabsf(floatRangeA.m_max - floatRangeB.m_min) < fabsf(floatRangeB.m_max - floatRangeA.m_min))
	{
		float pushDistForThisAxis = floatRangeA.m_max - floatRangeB.m_min;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = separatingAxisBi;
			pushDist = pushDistForThisAxis;
		}
	}
	else
	{
		float pushDistForThisAxis = floatRangeA.m_min - floatRangeB.m_max;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = -separatingAxisBi;
			pushDist = pushDistForThisAxis;
		}
	}

	Vec3 const& separatingAxisBj = zOrientedBoxB.m_jBasis;
	floatRangeA = zOrientedBoxA.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBj);
	floatRangeB = zOrientedBoxB.GetFloatRangeForPointsProjectedOntoAxis(separatingAxisBj);
	if (!floatRangeA.IsOverlappingWith(floatRangeB))
	{
		return false;
	}
	if (fabsf(floatRangeA.m_max - floatRangeB.m_min) < fabsf(floatRangeB.m_max - floatRangeA.m_min))
	{
		float pushDistForThisAxis = floatRangeA.m_max - floatRangeB.m_min;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = separatingAxisBj;
			pushDist = pushDistForThisAxis;
		}
	}
	else
	{
		float pushDistForThisAxis = floatRangeA.m_min - floatRangeB.m_max;
		if (fabsf(pushDistForThisAxis) < fabsf(pushDist))
		{
			pushDirection = -separatingAxisBj;
			pushDist = pushDistForThisAxis;
		}
	}

	zOrientedBoxA.m_center += pushDist * pushDirection;
	return true;
}
