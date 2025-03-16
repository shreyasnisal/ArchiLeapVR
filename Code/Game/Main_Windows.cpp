#include "Game/App.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Core/EngineCommon.hpp"

#include <cassert>
#include <crtdbg.h>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


int WINAPI WinMain( _In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	g_app = new App();
	g_app->Startup();
	g_app->Run();
	g_app->Shutdown();
	delete g_app;
	g_app = nullptr;

	return 0;
}


