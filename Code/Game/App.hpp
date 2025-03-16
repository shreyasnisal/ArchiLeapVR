#pragma once

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/Models/Model.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/Camera.hpp"


class Game;

class App
{
public:
						App							();
						~App						();

	void				Startup						();
	void				Shutdown					();
	void				Run							();
	void				RunFrame					();

	bool				IsQuitting					() const		{ return m_isQuitting; }
	bool				HandleQuitRequested			();

	XREye				GetCurrentEye				() const;
	Camera const		GetCurrentCamera			() const;

	static bool			HandleQuitRequested			(EventArgs& args);
	static bool			ShowControls				(EventArgs& args);

public:
	Camera				m_worldCamera;
	Camera				m_leftWorldCamera;
	Camera				m_rightWorldCamera;
	Camera				m_screenCamera;
	Camera				m_leftEyeCamera;
	Camera				m_rightEyeCamera;

	Texture*			m_screenRTVTexture = nullptr;

	Game* m_game = nullptr;

private:
	void				InitializeCameras			();

	void				BeginFrame					();
	void				Update						();
	void				Render						() const;
	void				EndFrame					();

	void				RenderScreen				() const;
	void				RenderCustomScreens			() const;
	void				HandleDevInput				();

private:
	bool				m_isQuitting				= false;

	XREye				m_currentEye				= XREye::NONE;
	XREye				m_currentEyeForScreen		= XREye::NONE;
};
