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

bool Platform_Init( const char *title, int width, int height, bool preferSoftware )
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

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	gPlatformState.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
	if (gPlatformState.window == nullptr)
	{
		Log("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
		return false;
	}
	static bool bErrored = false;
	bool bIsX11 = !strcmp(SDL_GetCurrentVideoDriver(), "x11");
	// SDL does not handle X11 errors, so app crashes if GL renderer unavailiable
	if(!strcmp(SDL_GetCurrentVideoDriver(), "x11"))
	{
		// anyway, 2d rendering in software and uploading to GL is slower than direct x11 render
		SDL_SetHint("SDL_FRAMEBUFFER_ACCELERATION", "software");
		void (*x_error_handler)(void *d, void *e) = [](void *d, void *e) -> void
		{
			Log("X11 Error!\n");
			bErrored = true;
		};
		void *ptr = SDL_LoadObject("libX11.so");
		if(ptr)
		{
			int (*pXSetErrorHandler)(void (*handler)(void *d, void *e));
			*(void**)&pXSetErrorHandler = SDL_LoadFunction(ptr, "XSetErrorHandler");
			if(pXSetErrorHandler)
				pXSetErrorHandler(x_error_handler);
		}
	}
	int renderer_index = -1;
	unsigned int flags = SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED;

	// OpenGL implementations are bloated
	// Even with LIBGL_ALWAYS_INDIRECT or something it loads llvm, libdrm_* for all known and unknown gpus, etc..
	// try set SDL not even try to load glX, EGL
	// todo: SDL3 has vulkan renderer, which may get much less overhead
	if(preferSoftware)
	{
		// force only software renderer.
		// if it's unavailiable, will fallback to gl1 anyway
		// todo: what about GLES-only systems? Local GL implementation may work on ES1
		// GLES2 SDL_Rendererer is known to be the slowest
		SDL_SetHint("SDL_FRAMEBUFFER_ACCELERATION", "software");
		SDL_SetHint("SDL_RENDER_DRIVER", "software");
		int count = SDL_GetNumRenderDrivers();
		for(int i = 0; i < count; i++)
		{
			SDL_RendererInfo info;
			SDL_GetRenderDriverInfo(i, &info);
			if(info.flags & SDL_RENDERER_SOFTWARE)
			{
				renderer_index = i;
				flags &= ~SDL_RENDERER_ACCELERATED;
				flags |= SDL_RENDERER_SOFTWARE;
				break;
			}
		}
	}

	gPlatformState.renderer = SDL_CreateRenderer(gPlatformState.window, renderer_index, flags);

	if (gPlatformState.renderer == nullptr)
	{
		Log("Error creating SDL_Renderer!");
		gPlatformState.useGL = true;
		SDL_DestroyWindow(gPlatformState.window);
		window_flags = (SDL_WindowFlags) (window_flags | SDL_WINDOW_OPENGL);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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
		if(!preferSoftware && bErrored)
			Log("Warning: SDL_Renderer errored\n"
				"Try setting SDL_RENDER_DRIVER=software and SDL_FRAMEBUFFER_ACCELERATION=software if have some rendering issues!\n");

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

bool Platform_ProcessEvents(bool &hasEvents)
{
	SDL_Event event;
	hasEvents = false;
	while (SDL_PollEvent(&event))
	{
		hasEvents = true;
		ImGui_ImplSDL2_ProcessEvent(&event);
		if (event.type == SDL_QUIT)
			return false;
		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(gPlatformState.window))
			return false;
		hasEvents = true;
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
#define SLEEP_ACTIVE 16666666
#define SLEEP_SUBACTIVE 2500000
#define SLEEP_IDLE  100000000
constexpr static long long sleepTimes[4]
{
	50000000,
	16666666,
	16666666,
	10000000,
};
void FrameControl(unsigned int requestFrames)
{
	static unsigned long long lastTime;
	unsigned long long time = GetTimeU64();
	long long timeDiff = sleepTimes[requestFrames] - (time - lastTime);
	if(timeDiff > 5000000)
		usleep(timeDiff / 1000);
	lastTime = time;
}

int main(int argc, char **argv)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	if(!Platform_Init("Layer GUI", 1024, 768, false))
		return 1;
	bool done = false;
	bool show_demo_window = true;
	unsigned int requestFrames = 3;
	bool frameSkipped = false;
	while(!done)
	{
		bool hasEvents;
		done = !Platform_ProcessEvents(hasEvents);
		if(hasEvents)
			requestFrames = 3;
		if(requestFrames < 2)
		{
			FrameControl(requestFrames);
			if(requestFrames)
				requestFrames--;
			frameSkipped = true;
			continue;
		}
		Platform_NewFrame();
		ImGui::NewFrame();
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		ImGui::Render();
		if(!frameSkipped)
		{
			Platform_Present(io);
			FrameControl(requestFrames);
			requestFrames--;
		}

		frameSkipped = false;
	}
	Platform_Shutdown();
}
