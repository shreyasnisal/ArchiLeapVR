#include "Game/TileDefinition.hpp"

#include "Game/GameCommon.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"


std::map<std::string, TileDefinition> TileDefinition::s_definitions;


TileDefinition::TileDefinition(XmlElement const* element)
{
	m_name = ParseXmlAttribute(*element, "name", m_name);
	m_isSolid = ParseXmlAttribute(*element, "solid", false);
	m_bounds = ParseXmlAttribute(*element, "bounds", m_bounds);

	XmlElement const* modelElement = element->FirstChildElement("Model");
	if (modelElement)
	{
		m_model = g_modelLoader->CreateOrGetModelFromXml(modelElement);
	}
}

void TileDefinition::CreateFromXml()
{
	const std::string FNAME = "Data/Definitions/Tiles.xml";

	XmlDocument xmlDoc;
	XmlResult result = xmlDoc.LoadFile(FNAME.c_str());
	if (result != XmlResult::XML_SUCCESS)
	{
		ERROR_AND_DIE(Stringf("Unable to open or read file \"%s\"", FNAME.c_str()));
	}
	XmlElement const* rootElement = xmlDoc.RootElement();
	if (!rootElement)
	{
		ERROR_AND_DIE(Stringf("XML file \"%s\" contains no XML element!", FNAME.c_str()));
	}

	XmlElement const* definitionElement = rootElement->FirstChildElement("TileDefinition");

	while (definitionElement)
	{
		TileDefinition newDef(definitionElement);
		s_definitions[newDef.m_name] = newDef;

		definitionElement = definitionElement->NextSiblingElement();
	}
}
