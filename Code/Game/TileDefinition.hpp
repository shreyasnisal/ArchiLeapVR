#pragma once

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Core/XMLUtils.hpp"
#include "Engine/Math/AABB3.hpp"

#include <map>

class TileDefinition
{
public:
	~TileDefinition() = default;
	TileDefinition() = default;
	TileDefinition(XmlElement const* element);

public:
	std::string m_name = "";
	Model* m_model = nullptr;
	bool m_isSolid = false;
	AABB3 m_bounds;

public:
	static void CreateFromXml();
	static std::map<std::string, TileDefinition> s_definitions;
};

