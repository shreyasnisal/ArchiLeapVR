#include "Game/Map.hpp"

#include "Game/App.hpp"
#include "Game/Button.hpp"
#include "Game/Coin.hpp"
#include "Game/Crate.hpp"
#include "Game/Door.hpp"
#include "Game/Enemy_Orc.hpp"
#include "Game/Entity.hpp"
#include "Game/EntityUID.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Goal.hpp"
#include "Game/Lever.hpp"
#include "Game/MovingPlatform.hpp"
#include "Game/Particle.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"
#include "Game/PlayerStart.hpp"
#include "Game/Tile.hpp"
#include "Game/TileDefinition.hpp"

#include "Game/GameMathUtils.hpp"

#include "Engine/Core/BufferParser.hpp"
#include "Engine/Core/BufferWriter.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"
#include "Engine/UI/UISystem.hpp"
#include "Engine/VirtualReality/VRController.hpp"


Map::~Map()
{
	delete m_shaderCBO;
}

Map::Map(Game* game)
	: m_game(game)
{
	LoadAssets();

	unsigned int entityIndex = (unsigned int)m_entities.size();
	EntityUID uid(entityIndex, m_entityUIDSalt);
	m_entityUIDSalt++;
	m_playerStart = new PlayerStart(this, uid, Vec3::ZERO, EulerAngles::ZERO);

	InitializeTiles();

	m_shaderCBO = g_renderer->CreateConstantBuffer(sizeof(ArchiLeapShaderConstants));

	SubscribeEventCallbackFunction("ToggleLinkLines", Event_ToggleLinkLines, "Toggles link lines rendering");
	SubscribeEventCallbackFunction("ResetTransform", Event_ResetTransform, "Resets transform for an entity");
	SubscribeEventCallbackFunction("SaveMap", Event_SaveMap, "Saves the map");
	SubscribeEventCallbackFunction("ChangeMovementDirection", Event_ChangeMovementDirection, "Changes the movement direction for a moving platform");
}

Map::Map(Game* game, std::string mapFileName, MapMode mode)
	: m_game(game)
	, m_mode(mode)
{
	LoadAssets();
	m_shaderCBO = g_renderer->CreateConstantBuffer(sizeof(ArchiLeapShaderConstants));

	SubscribeEventCallbackFunction("ToggleLinkLines", Event_ToggleLinkLines, "Toggles link lines rendering");
	SubscribeEventCallbackFunction("ResetTransform", Event_ResetTransform, "Resets transform for an entity");
	SubscribeEventCallbackFunction("SaveMap", Event_SaveMap, "Saves the map");
	SubscribeEventCallbackFunction("ChangeMovementDirection", Event_ChangeMovementDirection, "Changes the movement direction for a moving platform");

	LoadFromFile(mapFileName);

	Strings mapFilePathSplit;
	int numSplitsInFileName = SplitStringOnDelimiter(mapFilePathSplit, mapFileName, '/');
	std::string mapFileNameWithNoDirectory = mapFilePathSplit[numSplitsInFileName - 1];
	Strings splitMapName;
	int numSplits = SplitStringOnDelimiter(splitMapName, mapFileNameWithNoDirectory, '.');
	std::string mapDisplayName = "";
	for (int splitIndex = 0; splitIndex < numSplits - 1; splitIndex++)
	{
		mapDisplayName += splitMapName[splitIndex];
	}
	m_game->m_mapNameInputField->SetText(mapDisplayName);

	if (m_mode == MapMode::PLAY)
	{
		m_game->m_player->m_pawn->m_position = m_playerStart->m_position;
		m_game->m_player->m_pawn->m_orientation = m_playerStart->m_orientation;
		m_game->m_player->m_pawn->m_velocity = Vec3::ZERO;
		m_game->m_player->m_pawn->m_acceleration = Vec3::ZERO;
		m_game->m_player->m_state = PlayerState::PLAY;
	}
}

void Map::LoadAssets()
{
	m_diffuseShader = g_renderer->CreateOrGetShader("Data/Shaders/Diffuse", VertexType::VERTEX_PCUTBN);

	std::vector<Vertex_PCUTBN> cubeVerts;
	std::vector<unsigned int> cubeIndexes;
	AddVertsForAABB3(cubeVerts, cubeIndexes, AABB3(Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, 0.5f)), Rgba8::WHITE, AABB2::ZERO_TO_ONE);
	m_cubeModel = g_modelLoader->CreateOrGetModelFromVertexes("Cube", cubeVerts, cubeIndexes);
}

void Map::InitializeTiles()
{
	for (int xPos = -NEW_MAP_HALF_DIMENSIONS; xPos <= NEW_MAP_HALF_DIMENSIONS; xPos++)
	{
		for (int yPos = -NEW_MAP_HALF_DIMENSIONS; yPos <= NEW_MAP_HALF_DIMENSIONS; yPos++)
		{
			SpawnNewEntityOfType(EntityType::TILE_GRASS, Vec3((float)xPos, (float)yPos, -1.f), EulerAngles::ZERO, 1.f);
		}
	}
}

void Map::LoadFromFile(std::string filename)
{
	std::vector<uint8_t> mapRawData;
	FileReadToBuffer(mapRawData, filename);

	GUARANTEE_OR_DIE(!mapRawData.empty(), "Could not read data in map file!");

	BufferParser parser(mapRawData);

	uint8_t saveFile4ccCode0 = parser.ParseChar();
	GUARANTEE_OR_DIE(saveFile4ccCode0 == SAVEFILE_4CC_CODE[0], "File code mismatch! Are you sure this is a .almap file?");
	uint8_t saveFile4ccCode1 = parser.ParseChar();
	GUARANTEE_OR_DIE(saveFile4ccCode1 == SAVEFILE_4CC_CODE[1], "File code mismatch! Are you sure this is a .almap file?");
	uint8_t saveFile4ccCode2 = parser.ParseChar();
	GUARANTEE_OR_DIE(saveFile4ccCode2 == SAVEFILE_4CC_CODE[2], "File code mismatch! Are you sure this is a .almap file?");
	uint8_t saveFile4ccCode3 = parser.ParseChar();
	GUARANTEE_OR_DIE(saveFile4ccCode3 == SAVEFILE_4CC_CODE[3], "File code mismatch! Are you sure this is a .almap file?");
	uint8_t saveFileVersion = parser.ParseByte();
	GUARANTEE_OR_DIE(saveFileVersion == SAVEFILE_VERSION, "Save file version mismatch!");

	uint32_t numEntities = parser.ParseUint32();

	uint8_t playerStartUnnecessaryType = parser.ParseByte();
	UNUSED(playerStartUnnecessaryType);
	EntityUID playerStartUID(parser.ParseUint32());
	Vec3 playerStartPosition = parser.ParseVec3();
	EulerAngles playerStartOrientation = parser.ParseEulerAngles();
	float playerStartScale = parser.ParseFloat();
	m_playerStart = new PlayerStart(this, playerStartUID, playerStartPosition, playerStartOrientation);
	m_playerStart->m_scale = playerStartScale;

	for (int entityIndex = 0; entityIndex < (int)numEntities; entityIndex++)
	{
		uint8_t entityTypeIndex = parser.ParseByte();
		if (entityTypeIndex == 0xFF)
		{
			// This is an invalid entity
			// To maintain indexes, push a nullptr in the entities list
			m_entities.push_back(nullptr);
			continue;
		}

		EntityType entityType = EntityType(entityTypeIndex);

		EntityUID entityUID(parser.ParseUint32());
		Vec3 entityPosition = parser.ParseVec3();
		EulerAngles entityOrientation = parser.ParseEulerAngles();
		float entityScale = parser.ParseFloat();
		Entity* entity = CreateEntityOfTypeWithUID(entityType, entityUID, entityPosition, entityOrientation, entityScale);
		if (entityType == EntityType::BUTTON || entityType == EntityType::LEVER)
		{
			EntityUID activatableUID(parser.ParseUint32());
			Activator* activator = (Activator*)entity;
			activator->SetActivatable(activatableUID);
		}
		else if (entityType == EntityType::DOOR || entityType == EntityType::MOVING_PLATFORM)
		{
			EntityUID activatorUID(parser.ParseUint32());
			Activatable* activatable = (Activatable*)entity;
			activatable->m_activatorUID = activatorUID;
			
			if (entityType == EntityType::MOVING_PLATFORM)
			{
				MovementDirection movementDirection = (MovementDirection)parser.ParseByte();
				MovingPlatform* movingPlatform = (MovingPlatform*)activatable;
				movingPlatform->m_movementDirection = movementDirection;
			}
		}
		m_entities.push_back(entity);
	}
}

void Map::Update()
{
	if (m_isUnsaved)
	{
		m_game->m_saveButtonWidget->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT);
	}
	else
	{
		m_game->m_saveButtonWidget->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT);
	}

	m_game->m_player->m_pawn->Update();
	m_playerStart->Update();
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->Update();
	}

	m_game->m_coinsCollectedTextWidget->SetText(Stringf("%d", m_coinsCollected));

	HandlePlayerPawnEntityInteractions();
	HandleMovingPlatformsVsEntities();
	HandleCratesVsEntities();
	HandleOrcsVsEntities();
	UpdateParticles();
	UpdateShaderConstants();
}

void Map::Render() const
{
	g_renderer->BeginRenderEvent("Map");

	g_renderer->BeginRenderEvent("Entities");
	g_renderer->BindShader(m_diffuseShader);
	Vec3 eyePosition = g_app->GetCurrentCamera().GetPosition();
	g_renderer->SetLightConstants(SUN_DIRECTION.GetNormalized(), SUN_INTENSITY, 1.f - SUN_INTENSITY, eyePosition);
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->Render();
	}
	g_renderer->EndRenderEvent("Entities");

	Vec3 playerEyeCenterPosition = m_game->m_player->GetPlayerPosition();
	VRController leftController = g_openXR->GetLeftController();
	EulerAngles leftControllerOrientation = leftController.GetOrientation_iFwd_jLeft_kUp();
	Vec3 leftControllerPosition = leftController.GetPosition_iFwd_jLeft_kUp();

	Vec3 leftControllerFwd, leftControllerLeft, leftControllerUp;
	leftControllerOrientation.GetAsVectors_iFwd_jLeft_kUp(leftControllerFwd, leftControllerLeft, leftControllerUp);

	g_renderer->BeginRenderEvent("PlayerStart");
	g_renderer->BindShader(nullptr);
	m_playerStart->Render();
	g_renderer->EndRenderEvent("PlayerStart");

	RenderLinkLines();
	RenderParticles();

	g_renderer->EndRenderEvent("Map");
}

void Map::RenderScreen() const
{
}

void Map::UpdateParticles()
{
	for (int particleIndex = 0; particleIndex < (int)m_particles.size(); particleIndex++)
	{
		m_particles[particleIndex]->Update();
	}
}

void Map::DestroyGarbageParticles()
{
	m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [](Particle* particle){ return particle->m_isDestroyed; }), m_particles.end());
}

void Map::RenderLinkLines() const
{
	if (!m_renderLinkLines)
	{
		return;
	}
	if (m_game->m_player->m_state == PlayerState::PLAY)
	{
		return;
	}

	g_renderer->BeginRenderEvent("Link Lines");

	std::vector<Vertex_PCU> linkLinesVerts;
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		if (m_entities[entityIndex]->m_type != EntityType::BUTTON && m_entities[entityIndex]->m_type != EntityType::LEVER)
		{
			continue;
		}

		Activator* activator = (Activator*)m_entities[entityIndex];
		Entity* activatable = GetEntityFromUID(activator->m_activatableUID);
		if (activatable)
		{
			AddVertsForLineSegment3D(linkLinesVerts, activator->m_position, activatable->m_position, 0.01f, Rgba8::GRAY);
		}
	}

	g_renderer->BindShader(nullptr);
	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetModelConstants();
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(linkLinesVerts);

	g_renderer->EndRenderEvent("Link Lines");
}

void Map::RenderParticles() const
{
	g_renderer->BindShader(m_diffuseShader);
	g_renderer->SetLightConstants(Vec3::ZERO, 0.f, 1.f);
	g_renderer->BindTexture(nullptr);
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

	for (int particleIndex = 0; particleIndex < (int)m_particles.size(); particleIndex++)
	{
		m_particles[particleIndex]->Render();
	}
}

void Map::HandlePlayerPawnEntityInteractions()
{
	if (m_game->m_player->m_state != PlayerState::PLAY)
	{
		return;
	}

	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->HandlePlayerInteraction();
	}
}

void Map::HandleMovingPlatformsVsEntities()
{
	if (m_game->m_player->m_state != PlayerState::PLAY)
	{
		return;
	}

	for (int movingPlatformIndex = 0; movingPlatformIndex < (int)m_entities.size(); movingPlatformIndex++)
	{
		if (!m_entities[movingPlatformIndex])
		{
			continue;
		}
		if (m_entities[movingPlatformIndex]->m_type != EntityType::MOVING_PLATFORM)
		{
			continue;
		}

		MovingPlatform* movingPlatform = (MovingPlatform*)m_entities[movingPlatformIndex];
		OBB3 movingPlatformBounds = movingPlatform->GetBounds();
		for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
		{
			if (entityIndex == movingPlatformIndex)
			{
				continue;
			}
			if (!m_entities[entityIndex])
			{
				continue;
			}
			if  (m_entities[entityIndex]->m_type == EntityType::CRATE || m_entities[entityIndex]->m_type == EntityType::ENEMY_ORC || m_entities[entityIndex]->m_type == EntityType::COIN)
			{
				continue;
			}

			if (DoZOBB3Overlap(movingPlatformBounds, m_entities[entityIndex]->GetBounds()))
			{
				movingPlatform->m_isMoving = false;
			}
		}
	}
}

void Map::HandleCratesVsEntities()
{
	if (m_game->m_player->m_state != PlayerState::PLAY)
	{
		return;
	}

	for (int crateIndex = 0; crateIndex < (int)m_entities.size(); crateIndex++)
	{
		if (!m_entities[crateIndex])
		{
			continue;
		}

		if (m_entities[crateIndex]->m_type != EntityType::CRATE)
		{
			continue;
		}

		Crate* crate = (Crate*)m_entities[crateIndex];
		OBB3 crateBox = crate->GetBounds();

		for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
		{
			if (entityIndex == crateIndex)
			{
				continue;
			}
			if (!m_entities[entityIndex])
			{
				continue;
			}
			if (m_entities[entityIndex]->m_type == EntityType::COIN)
			{
				continue;
			}

			float cratePositionZBeforePush = crate->m_position.z;
			if (PushZOBB3OutOfFixedZOBB3(crateBox, m_entities[entityIndex]->GetBounds()))
			{
				crate->m_position = crateBox.m_center + Vec3::GROUNDWARD * crate->m_localBounds.GetDimensions().z * crate->m_scale * 0.5f;
				if (cratePositionZBeforePush < crate->m_position.z)
				{
					crate->m_velocity.z = 0.f;
					crate->m_isGrounded = true;
					if (m_entities[entityIndex]->m_type == EntityType::BUTTON)
					{
						Button* button = (Button*)m_entities[entityIndex];
						button->m_isPressed = true;
					}
				}
			}
		}
	}
}

void Map::HandleOrcsVsEntities()
{
	if (m_game->m_player->m_state != PlayerState::PLAY)
	{
		return;
	}

	for (int orcIndex = 0; orcIndex < (int)m_entities.size(); orcIndex++)
	{
		if (!m_entities[orcIndex])
		{
			continue;
		}

		if (m_entities[orcIndex]->m_type != EntityType::ENEMY_ORC)
		{
			continue;
		}

		Enemy_Orc* orc = (Enemy_Orc*)m_entities[orcIndex];
		for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
		{
			if (entityIndex == orcIndex)
			{
				continue;
			}
			if (!m_entities[entityIndex])
			{
				continue;
			}
			if (m_entities[entityIndex]->m_type == EntityType::COIN)
			{
				continue;
			}

			float orcZPositionBeforePush = orc->m_position.z;
			Vec3 orcCylinderTop = orc->m_position + Vec3::SKYWARD * Enemy_Orc::HEIGHT;
			PushZCylinderOutOfFixedZOBB3(orc->m_position, orcCylinderTop, Enemy_Orc::RADIUS, m_entities[entityIndex]->GetBounds());
			if (orc->m_position.z > orcZPositionBeforePush)
			{
				orc->m_isGrounded = true;
				orc->m_velocity.z = 0.f;
			}
		}
	}
}

void Map::RenderCustomScreens() const
{
}

void Map::UpdateShaderConstants() const
{
	ArchiLeapShaderConstants shaderConstants;

	float skyColorAsFloats[4];
	Rgba8::DEEP_SKY_BLUE.GetAsFloats(skyColorAsFloats);
	shaderConstants.m_skyColor = Vec4(skyColorAsFloats[0], skyColorAsFloats[1], skyColorAsFloats[2], skyColorAsFloats[3]);

	shaderConstants.m_fogStartDistance = 10.f;
	shaderConstants.m_fogEndDistance = 15.f;
	shaderConstants.m_fogMaxAlpha = 0.f;

	g_renderer->CopyCPUToGPU(reinterpret_cast<void*>(&shaderConstants), sizeof(shaderConstants), m_shaderCBO);
	g_renderer->BindConstantBuffer(g_archiLeapShaderConstantsSlot, m_shaderCBO);
}

Entity* Map::CreateEntityOfTypeWithUID(EntityType type, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
{
	switch (type)
	{
		case EntityType::TILE_GRASS:		return new Tile(this, uid, TileDefinition::s_definitions["Block1x1"], position, orientation, scale);
		case EntityType::TILE_DIRT:			return new Tile(this, uid, TileDefinition::s_definitions["Dirt1x1"], position, orientation, scale);
		case EntityType::LEVER:				return new Lever(this, uid, position, orientation, scale);
		case EntityType::BUTTON:			return new Button(this, uid, position, orientation, scale);
		case EntityType::DOOR:				return new Door(this, uid, position, orientation, scale);
		case EntityType::MOVING_PLATFORM:	return new MovingPlatform(this, uid, position, orientation, scale);
		case EntityType::COIN:				return new Coin(this, uid, position, orientation, scale);
		case EntityType::CRATE:				return new Crate(this, uid, position, orientation, scale);
		case EntityType::ENEMY_ORC:			return new Enemy_Orc(this, uid, position, orientation, scale);
		case EntityType::FLAG:				return new Goal(this, uid, position, orientation, scale);
	}

	return nullptr;
}

Entity* Map::CreateEntityOfType(EntityType type, Vec3 const& position, EulerAngles const& orientation, float scale)
{
	unsigned int entityIndex;

	for (entityIndex = 0; entityIndex < m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			break;
		}
	}

	EntityUID uid(entityIndex, m_entityUIDSalt);
	m_entityUIDSalt++;

	switch (type)
	{
		case EntityType::TILE_GRASS:		return new Tile(this, uid, TileDefinition::s_definitions["Block1x1"], position, orientation, scale);
		case EntityType::TILE_DIRT:			return new Tile(this, uid, TileDefinition::s_definitions["Dirt1x1"], position, orientation, scale);
		case EntityType::LEVER:				return new Lever(this, uid, position, orientation, scale);
		case EntityType::BUTTON:			return new Button(this, uid, position, orientation, scale);
		case EntityType::DOOR:				return new Door(this, uid, position, orientation, scale);
		case EntityType::MOVING_PLATFORM:	return new MovingPlatform(this, uid, position, orientation, scale);
		case EntityType::COIN:				return new Coin(this, uid, position, orientation, scale);
		case EntityType::CRATE:				return new Crate(this, uid, position, orientation, scale);
		case EntityType::ENEMY_ORC:			return new Enemy_Orc(this, uid, position, orientation, scale);
		case EntityType::FLAG:				return new Goal(this, uid, position, orientation, scale);
	}

	return nullptr;
}

Entity* Map::SpawnNewEntityOfType(EntityType type, Vec3 const& position, EulerAngles const& orientation, float scale)
{
	Entity* entity = CreateEntityOfType(type, position, orientation, scale);
	if (entity)
	{
		if (entity->m_uid.GetIndex() < m_entities.size())
		{
			m_entities[entity->m_uid.GetIndex()] = entity;
		}
		else
		{
			m_entities.push_back(entity);
		}
	}

	return entity;
}

std::string Map::GetEntityNameFromType(EntityType type)
{
	switch (type)
	{
		case EntityType::NONE:				return "Player Start";
		case EntityType::TILE_GRASS:		return "Tile (Grass)";
		case EntityType::TILE_DIRT:			return "Tile (Dirt)";
		case EntityType::LEVER:				return "Lever";
		case EntityType::BUTTON:			return "Button";
		case EntityType::DOOR:				return "Door";
		case EntityType::MOVING_PLATFORM:	return "Moving Platform";
		case EntityType::COIN:				return "Coin";
		case EntityType::CRATE:				return "Crate";
		case EntityType::ENEMY_ORC:			return "Orc";
		case EntityType::FLAG:				return "Flag";
	}
	return "None";
}

float Map::GetDefaultEntityScaleForType(EntityType type)
{
	switch (type)
	{
		case EntityType::TILE_GRASS:		return 1.f;
		case EntityType::TILE_DIRT:			return 1.f;
		case EntityType::LEVER:				return MODEL_SCALE;
		case EntityType::BUTTON:			return MODEL_SCALE;
		case EntityType::DOOR:				return MODEL_SCALE;
		case EntityType::MOVING_PLATFORM:	return MODEL_SCALE;
		case EntityType::COIN:				return 1.f;
		case EntityType::CRATE:				return 1.f;
		case EntityType::ENEMY_ORC:			return MODEL_SCALE;
		case EntityType::FLAG:				return MODEL_SCALE;
	}
	return 1.f;
}

bool Map::RemoveEntityFromMap(Entity* entity)
{
	if (!entity)
	{
		return false;
	}
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (m_entities[entityIndex] == entity)
		{
			delete m_entities[entityIndex];
			m_entities[entityIndex] = nullptr;
			return true;
		}
	}

	return false;
}

void Map::LinkEntities(Entity* entity1, Entity* entity2)
{
	if (m_isPulsingActivatables)
	{
		TogglePulseActivatables();
	}
	else if (m_isPulsingActivators)
	{
		TogglePulseActivators();
	}

	if (entity1->m_type == EntityType::BUTTON || entity1->m_type == EntityType::LEVER)
	{
		if (entity2->m_type == EntityType::DOOR || entity2->m_type == EntityType::MOVING_PLATFORM)
		{
			Activator* activator = (Activator*)entity1;
			Activatable* activatable = (Activatable*)entity2;

			Activator* prevActivator = dynamic_cast<Activator*>(GetEntityFromUID(activatable->m_activatorUID));
			if (prevActivator)
			{
				prevActivator->m_activatableUID = EntityUID::INVALID;
			}
			Activatable* prevActivatable = dynamic_cast<Activatable*>(GetEntityFromUID(activator->m_activatableUID));
			if (prevActivatable)
			{
				prevActivatable->m_activatorUID = EntityUID::INVALID;
			}

			activator->m_activatableUID = activatable->m_uid;
			activatable->m_activatorUID = activator->m_uid;
		}
	}
	else if (entity1->m_type == EntityType::DOOR || entity1->m_type == EntityType::MOVING_PLATFORM)
	{
		if (entity2->m_type == EntityType::BUTTON || entity2->m_type == EntityType::LEVER)
		{
			Activator* activator = (Activator*)entity2;
			Activatable* activatable = (Activatable*)entity1;

			Activator* prevActivator = dynamic_cast<Activator*>(GetEntityFromUID(activatable->m_activatorUID));
			if (prevActivator)
			{
				prevActivator->m_activatableUID = EntityUID::INVALID;
			}
			Activatable* prevActivatable = dynamic_cast<Activatable*>(GetEntityFromUID(activator->m_activatableUID));
			if (prevActivatable)
			{
				prevActivatable->m_activatorUID = EntityUID::INVALID;
			}

			activator->m_activatableUID = activatable->m_uid;
			activatable->m_activatorUID = activator->m_uid;
		}
	}
}

Particle* Map::SpawnParticle(Vec3 const& position, Vec3 const& velocity, EulerAngles const& orientation, float size, Rgba8 const& color, float lifetime)
{
	Particle* newParticle = new Particle(this, position, velocity, orientation, size, color, lifetime, m_cubeModel);
	m_particles.push_back(newParticle);
	return newParticle;
}

void Map::SetHoveredEntityForHand(XRHand hand, Entity* hoveredEntity)
{
	if (hand == XRHand::NONE)
	{
		SetMouseHoveredEntity(hoveredEntity);
	}
	if (hand == XRHand::LEFT)
	{
		SetLeftHoveredEntity(hoveredEntity);
	}
	else if (hand == XRHand::RIGHT)
	{
		SetRightHoveredEntity(hoveredEntity);
	}
}

void Map::SetMouseHoveredEntity(Entity* hoveredEntity)
{
	m_playerStart->SetMouseHovered(false);
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->SetMouseHovered(false);
	}

	if (hoveredEntity)
	{
		hoveredEntity->SetMouseHovered(true);
	}
}

void Map::SetRightHoveredEntity(Entity* hoveredEntity)
{
	m_playerStart->SetRightHovered(false);
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->SetRightHovered(false);
	}

	if (hoveredEntity)
	{
		hoveredEntity->SetRightHovered(true);
	}
}

void Map::SetLeftHoveredEntity(Entity* hoveredEntity)
{
	m_playerStart->SetLeftHovered(false);
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->SetLeftHovered(false);
	}

	if (hoveredEntity)
	{
		hoveredEntity->SetLeftHovered(true);
	}
}

void Map::SetSelectedEntity(Entity* selectedEntity)
{
	m_playerStart->SetSelected(false);
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}
		
		m_entities[entityIndex]->SetSelected(false);
	}

	if (selectedEntity)
	{
		selectedEntity->SetSelected(true);
	}
	m_selectedEntity = selectedEntity;
}

void Map::TogglePulseActivatables()
{
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		if (m_entities[entityIndex]->m_type == EntityType::DOOR || m_entities[entityIndex]->m_type == EntityType::MOVING_PLATFORM)
		{
			if (m_isPulsingActivatables)
			{
				m_entities[entityIndex]->m_pulseTimer.Stop();
			}
			else
			{
				m_entities[entityIndex]->m_pulseTimer.Start();
			}
		}
	}
	m_isPulsingActivatables = !m_isPulsingActivatables;
}

void Map::TogglePulseActivators()
{
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		if (m_entities[entityIndex]->m_type == EntityType::LEVER || m_entities[entityIndex]->m_type == EntityType::BUTTON)
		{
			if (m_isPulsingActivators)
			{
				m_entities[entityIndex]->m_pulseTimer.Stop();
			}
			else
			{
				m_entities[entityIndex]->m_pulseTimer.Start();
			}
		}
	}
	m_isPulsingActivators = !m_isPulsingActivators;
}

void Map::SaveAllEntityStates()
{
	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->SaveEditorState();
	}
}

void Map::ResetAllEntityStates()
{
	m_game->m_player->m_pawn->m_position = m_playerStart->m_position;
	m_game->m_player->m_pawn->m_orientation = m_playerStart->m_orientation;
	m_game->m_player->m_pawn->m_velocity = Vec3::ZERO;
	m_game->m_player->m_pawn->m_acceleration = Vec3::ZERO;
	m_game->m_player->m_pawn->m_angularVelocity = EulerAngles::ZERO;
	m_game->m_player->m_pawn->m_hasWon = false;

	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}

		m_entities[entityIndex]->ResetState();
	}
}

Entity* Map::GetEntityFromUID(EntityUID uid) const
{
	if (uid == EntityUID::INVALID)
	{
		return nullptr;
	}

	if (uid == m_playerStart->m_uid)
	{
		return m_playerStart;
	}

	unsigned int entityIndex = uid.GetIndex();

	if (entityIndex >= m_entities.size())
	{
		return nullptr;
	}

	Entity* entity = m_entities[entityIndex];
	if (!entity)
	{
		return nullptr;
	}
	if (entity->m_uid == uid)
	{
		return entity;
	}

	return nullptr;
}

Entity* Map::GetEntityFromUID(unsigned int uid) const
{
	EntityUID entityUID(uid);
	return GetEntityFromUID(entityUID);
}

ArchiLeapRaycastResult3D Map::RaycastVsEntities(Vec3 const& rayStartPos, Vec3 const& fwdNormal, float maxDistance, Entity const* entityToIgnore)
{
	ArchiLeapRaycastResult3D closestRaycastResult;
	closestRaycastResult.m_rayStartPosition = rayStartPos;
	closestRaycastResult.m_rayForwardNormal = fwdNormal;
	closestRaycastResult.m_rayMaxLength = maxDistance;
	closestRaycastResult.m_impactDistance = maxDistance;
	closestRaycastResult.m_didImpact = false;
	closestRaycastResult.m_impactPosition = rayStartPos + fwdNormal * maxDistance;
	closestRaycastResult.m_rayForwardNormal = Vec3::ZERO;
	closestRaycastResult.m_impactEntity = nullptr;

	if (m_playerStart != entityToIgnore)
	{
		ArchiLeapRaycastResult3D raycastVsPlayerStartResult = m_playerStart->Raycast(rayStartPos, fwdNormal, maxDistance);
		if (raycastVsPlayerStartResult.m_didImpact && raycastVsPlayerStartResult.m_impactDistance < closestRaycastResult.m_impactDistance)
		{
			closestRaycastResult = raycastVsPlayerStartResult;
		}
	}

	for (int entityIndex = 0; entityIndex < (int)m_entities.size(); entityIndex++)
	{
		if (!m_entities[entityIndex])
		{
			continue;
		}
		if (m_entities[entityIndex] == entityToIgnore)
		{
			continue;
		}

		ArchiLeapRaycastResult3D raycastResult = m_entities[entityIndex]->Raycast(rayStartPos, fwdNormal, maxDistance);
		if (raycastResult.m_didImpact && raycastResult.m_impactDistance < closestRaycastResult.m_impactDistance)
		{
			closestRaycastResult = raycastResult;
		}
	}

	return closestRaycastResult;
}

bool Map::Event_ToggleLinkLines(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_currentMap->m_renderLinkLines = !g_app->m_game->m_currentMap->m_renderLinkLines;
	return true;
}

bool Map::Event_ResetTransform(EventArgs& args)
{
	unsigned int entityUID = (unsigned int)args.GetValue("entity", (int)ENTITYUID_INVALID);
	Entity* entity = g_app->m_game->m_currentMap->GetEntityFromUID(entityUID);
	
	if (!entity)
	{
		return false;
	}

	entity->m_position = Vec3::ZERO;
	entity->m_orientation = EulerAngles::ZERO;
	entity->m_scale = 1.f;
	return true;
}

bool Map::Event_SaveMap(EventArgs& args)
{
	UNUSED(args);

	std::string mapName = g_app->m_game->m_mapNameInputField->m_text;
	Map* currentMap = g_app->m_game->m_currentMap;
	if (!currentMap)
	{
		return false;
	}

	std::vector<uint8_t> buffer;
	BufferWriter writer(buffer);

	writer.AppendByte(SAVEFILE_4CC_CODE[0]);
	writer.AppendByte(SAVEFILE_4CC_CODE[1]);
	writer.AppendByte(SAVEFILE_4CC_CODE[2]);
	writer.AppendByte(SAVEFILE_4CC_CODE[3]);
	writer.AppendByte(SAVEFILE_VERSION);
	writer.AppendUint32((uint32_t)currentMap->m_entities.size());

	currentMap->m_playerStart->AppendToBuffer(writer);

	for (int entityIndex = 0; entityIndex < (int)currentMap->m_entities.size(); entityIndex++)
	{
		if (!currentMap->m_entities[entityIndex])
		{
			writer.AppendByte(0xFF);
			continue;
		}

		currentMap->m_entities[entityIndex]->AppendToBuffer(writer);
	}

	FileWriteBuffer(Stringf("Saved\\%s.almap", mapName.c_str()), buffer);

	currentMap->m_isUnsaved = false;

	return true;
}

bool Map::Event_ChangeMovementDirection(EventArgs& args)
{
	EntityUID uid = EntityUID(args.GetValue("entity", (int)ENTITYUID_INVALID));
	MovingPlatform* movingPlatform = dynamic_cast<MovingPlatform*>(g_app->m_game->m_currentMap->GetEntityFromUID(uid));
	if (!movingPlatform)
	{
		return false;
	}
	MovementDirection newMovementDirection = MovementDirection(args.GetValue("direction", (int)MovementDirection::NONE));
	if (newMovementDirection == MovementDirection::NONE)
	{
		return false;
	}

	movingPlatform->m_movementDirection = newMovementDirection;
	return true;
}
