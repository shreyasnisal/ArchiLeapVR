#pragma once

#include "Game/GameCommon.hpp"

#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/Shader.hpp"


class EntityUID;
class Game;
class Particle;


class Map
{
public:
	~Map();
	Map() = default;
	explicit Map(Game* game);
	explicit Map(Game* game, std::string mapFileName, MapMode mode);

	void LoadAssets();
	void InitializeTiles();
	void LoadFromFile(std::string filename);

	void Update();
	void Render() const;
	void RenderScreen() const;

	void UpdateParticles();
	void DestroyGarbageParticles();

	void RenderLinkLines() const;
	void RenderParticles() const;

	void HandlePlayerPawnEntityInteractions();
	void HandleMovingPlatformsVsEntities();
	void HandleCratesVsEntities();
	void HandleOrcsVsEntities();

	void RenderCustomScreens() const;

	void UpdateShaderConstants() const;

	Entity* CreateEntityOfTypeWithUID(EntityType type, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);
	Entity* CreateEntityOfType(EntityType type, Vec3 const& position, EulerAngles const& orientation, float scale);
	Entity* SpawnNewEntityOfType(EntityType type, Vec3 const& position, EulerAngles const& orientation, float scale);
	std::string GetEntityNameFromType(EntityType type);
	float GetDefaultEntityScaleForType(EntityType type);
	bool RemoveEntityFromMap(Entity* entity);
	void LinkEntities(Entity* entity1, Entity* entity2);

	Particle* SpawnParticle(Vec3 const& position, Vec3 const& velocity, EulerAngles const& orientation, float size, Rgba8 const& color, float lifetime);

	void SetHoveredEntityForHand(XRHand hand, Entity* hoveredEntity);
	void SetMouseHoveredEntity(Entity* hoveredEntity);
	void SetRightHoveredEntity(Entity* hoveredEntity);
	void SetLeftHoveredEntity(Entity* hoveredEntity);
	void SetSelectedEntity(Entity* selectedEntity);

	void TogglePulseActivatables();
	void TogglePulseActivators();

	void SaveAllEntityStates();
	void ResetAllEntityStates();

	Entity* GetEntityFromUID(EntityUID uid) const;
	Entity* GetEntityFromUID(unsigned int uid) const;

	ArchiLeapRaycastResult3D RaycastVsEntities(Vec3 const& rayStartPos, Vec3 const& fwdNormal, float maxDistance, Entity const* entityToIgnore = nullptr);

	static bool Event_ToggleLinkLines(EventArgs& args);
	static bool Event_ResetTransform(EventArgs& args);
	static bool Event_SaveMap(EventArgs& args);
	static bool Event_ChangeMovementDirection(EventArgs& args);

public:
	static constexpr int NEW_MAP_HALF_DIMENSIONS = 5;

	MapMode m_mode = MapMode::NONE;
	std::vector<Entity*> m_entities;
	Game* m_game = nullptr;
	Shader* m_diffuseShader = nullptr;
	ConstantBuffer* m_shaderCBO = nullptr;
	Entity* m_playerStart = nullptr;
	unsigned int m_entityUIDSalt = 0x0;
	bool m_renderLinkLines = true;
	int m_coinsCollected = 0;
	Entity* m_selectedEntity = nullptr;
	bool m_isUnsaved = false;
	std::vector<Particle*> m_particles;
	bool m_isPulsingActivatables = false;
	bool m_isPulsingActivators = false;

private:
	Model* m_cubeModel = nullptr;
};
