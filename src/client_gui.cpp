#include "imgui_impl_opengl2.h"
#include <GL/gl.h>
#include "layer_events.h"
#if USE_SDL == 2
#include "SDL.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
static struct
{
	bool useGL;
	SDL_Renderer *renderer;
	SDL_Window *window;
	SDL_GLContext context;
	void (APIENTRY*pglClear)(GLbitfield mask);
	void (APIENTRY*pglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	void (APIENTRY*pglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
	void (APIENTRY*pglDisable)(GLenum cap);
	void (APIENTRY*pglEnable)(GLenum cap);
} gPlatformState;
void glDisable(GLenum cap);

void *DYN_GL_GetProcAddress(const char *proc)
{
	return SDL_GL_GetProcAddress(proc);
}

bool Platform_Init( const char *title, int width, int height )
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return false;
	}
	SDL_Init(SDL_INIT_TIMER);

#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	// Setup window
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	gPlatformState.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
	if (gPlatformState.window == nullptr)
	{
		Log("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
		return false;
	}
	gPlatformState.renderer = nullptr;//SDL_CreateRenderer(gPlatformState.window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	if (gPlatformState.renderer == nullptr)
	{
		Log("Error creating SDL_Renderer!");
		gPlatformState.useGL = true;
		SDL_DestroyWindow(gPlatformState.window);
		window_flags = (SDL_WindowFlags) (window_flags | SDL_WINDOW_OPENGL);
		gPlatformState.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
		gPlatformState.context = SDL_GL_CreateContext(gPlatformState.window);
		SDL_GL_MakeCurrent(gPlatformState.window, gPlatformState.context);
		SDL_GL_SetSwapInterval(1); // Enable vsync
		ImGui_ImplSDL2_InitForOpenGL(gPlatformState.window, gPlatformState.context);
		ImGui_ImplOpenGL2_Init();
		*(void**)&gPlatformState.pglClear = DYN_GL_GetProcAddress("glClear");
		*(void**)&gPlatformState.pglClearColor = DYN_GL_GetProcAddress("glClearColor");
		*(void**)&gPlatformState.pglViewport = DYN_GL_GetProcAddress("glViewport");
		*(void**)&gPlatformState.pglDisable = DYN_GL_GetProcAddress("glDisable");
		*(void**)&gPlatformState.pglEnable = DYN_GL_GetProcAddress("glEnable");
	}
	else
	{
		ImGui_ImplSDL2_InitForSDLRenderer(gPlatformState.window, gPlatformState.renderer);
		ImGui_ImplSDLRenderer2_Init(gPlatformState.renderer);
	}
	return true;
}

void Platform_NewFrame()
{
	if(gPlatformState.useGL)
		ImGui_ImplOpenGL2_NewFrame();
	else
		ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
}

bool Platform_ProcessEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		if (event.type == SDL_QUIT)
			return false;
		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(gPlatformState.window))
			return false;
	}
	return true;
}

void Platform_Present(ImGuiIO& io)
{
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	if(gPlatformState.useGL)
	{
		gPlatformState.pglDisable(GL_SCISSOR_TEST);
		gPlatformState.pglViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		gPlatformState.pglClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		gPlatformState.pglClear(GL_COLOR_BUFFER_BIT);
		gPlatformState.pglEnable(GL_SCISSOR_TEST);
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(gPlatformState.window);
	}
	else
	{
		SDL_RenderSetScale(gPlatformState.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
		SDL_SetRenderDrawColor(gPlatformState.renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(gPlatformState.renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(gPlatformState.renderer);
	}
}
void Platform_Shutdown()
{
	if(gPlatformState.useGL)
	{
		ImGui_ImplOpenGL2_Shutdown();
		SDL_GL_DeleteContext(gPlatformState.context);
	}
	else
	{
		ImGui_ImplSDLRenderer2_Shutdown();
		SDL_DestroyRenderer(gPlatformState.renderer);
	}
	ImGui_ImplSDL2_Shutdown();
	SDL_DestroyWindow(gPlatformState.window);
	SDL_Quit();
}

#else
#error "Not implemented yet"
#endif

int main(int argc, char **argv)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	if(!Platform_Init("Layer GUI", 1024, 768))
		return 1;
	bool done = false;
	bool show_demo_window = true;
	while(!done)
	{

		done = !Platform_ProcessEvents();
		Platform_NewFrame();
		ImGui::NewFrame();
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);
		// Rendering
		ImGui::Render();
		Platform_Present(io);
	}
	Platform_Shutdown();
}
