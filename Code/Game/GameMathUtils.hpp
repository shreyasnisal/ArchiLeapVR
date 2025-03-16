#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/OBB3.hpp"


bool DoZCylinderAndZOBB3Overlap(Vec3 const& cylinderBaseCenter, Vec3 const& cylinderTopCenter, float cylinderRadius, OBB3 const& zOrientedBox);
bool PushZCylinderOutOfFixedZOBB3(Vec3& cylinderBaseCenter, Vec3& cylinderTopCenter, float cylinderRadius, OBB3 const& zOrientedBox);
bool PushZOBB3OutOfFixedZCylinder(OBB3& zOrientedBox, Vec3 const& cylinderBaseCenter, Vec3 const& cylinderTopCenter, float cylinderRadius);
bool DoZOBB3Overlap(OBB3 const& zOrientedBoxA, OBB3 const& zOrientedBoxB);
bool PushZOBB3OutOfFixedZOBB3(OBB3& mobileBox, OBB3 const& fixedBox);
