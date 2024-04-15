#include <arpa/inet.h>
#include <sys/socket.h>
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

namespace ImGui {
	void TextUnformatted(const SubStr &s)
	{
		return TextUnformatted(s.begin, s.end);
	}
}

static void RunCommand(int pid, const Command &c);
static struct AppConsole
{
	char mWindowName[64];
	int mPid;
	bool show;
	char                  InputBuf[256];
	int mBufLen;
	ImVector<SubStr>       Items;
	SubStr InternalCommands[3] ={"help", "history", "clear"};
	ImVector<SubStr>       History;
	int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
	ImGuiTextFilter       Filter;
	bool                  AutoScroll;
	bool                  ScrollToBottom;

	AppConsole()
	{
		ClearLog();
		memset(InputBuf, 0, sizeof(InputBuf));
		HistoryPos = -1;
		AutoScroll = true;
		ScrollToBottom = false;
		AddLog("Welcome to Dear ImGui!");
	}
	~AppConsole()
	{
		ClearLog();
		for (int i = 0; i < History.Size; i++)
			History[i].Free();
	}

	// Portable helpers
	//static int   Stricmp(const char* s1, const char* s2)         { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
	//static int   Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
	//static char* Strdup(const char* s)                           { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = ImGui::MemAlloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
	//static void  Strtrim(char* s)                                { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

	void ClearLog()
	{
		for (int i = 0; i < Items.Size; i++)
			Items[i].Free();
		Items.clear();
	}

	template <size_t fmtlen, typename... Ts>
	void AddLog(const char (&fmt)[fmtlen], Ts... args ) //IM_FMTARGS(2)
	{
		char buf[512];
		int len = SBPrint(buf,fmt,args...);
		Items.push_back(SubStr(buf, len - 1).StrDup());
	}
	template <size_t fmtlen>
	void AddLog(const char (&fmt)[fmtlen])
	{
		Items.push_back(SubStr(fmt).StrDup());
	}

	void Draw()
	{
		if(!mWindowName[0])
		{
			show = false;
			return;
		}
		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(mWindowName, &show))
		{
			ImGui::End();
			return;
		}

		// As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
		// So e.g. IsItemHovered() will return true when hovering the title bar.
		// Here we create a context menu only available from the title bar.
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Close Console"))
				show = false;
			ImGui::EndPopup();
		}

		ImGui::TextWrapped(
			"This example implements a console with basic coloring, completion (TAB key) and history (Up/Down keys). A more elaborate "
			"implementation may want to store entries along with extra data such as timestamp, emitter, etc.");
		ImGui::TextWrapped("Enter 'HELP' for help.");

		// TODO: display items starting from the bottom

		if (ImGui::SmallButton("Add Debug Text"))  { AddLog("%d some text", Items.Size); AddLog("some more text"); AddLog("display very important message here!"); }
		ImGui::SameLine();
		if (ImGui::SmallButton("Add Debug Error")) { AddLog("[error] something went wrong"); }
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear"))           { ClearLog(); }
		ImGui::SameLine();
		bool copy_to_clipboard = ImGui::SmallButton("Copy");
		//static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }

		ImGui::Separator();

		// Options menu
		if (ImGui::BeginPopup("Options"))
		{
			ImGui::Checkbox("Auto-scroll", &AutoScroll);
			ImGui::EndPopup();
		}

		// Options, Filter
		if (ImGui::Button("Options"))
			ImGui::OpenPopup("Options");
		ImGui::SameLine();
		Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
		ImGui::Separator();

		// Reserve enough left-over height for 1 separator + 1 input text
		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
		{
			if (ImGui::BeginPopupContextWindow())
			{
				if (ImGui::Selectable("Clear")) ClearLog();
				ImGui::EndPopup();
			}

			// Display every line as a separate entry so we can change their color or add custom widgets.
			// If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
			// NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
			// to only process visible items. The clipper will automatically measure the height of your first item and then
			// "seek" to display only items in the visible area.
			// To use the clipper we can replace your standard loop:
			//      for (int i = 0; i < Items.Size; i++)
			//   With:
			//      ImGuiListClipper clipper;
			//      clipper.Begin(Items.Size);
			//      while (clipper.Step())
			//         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			// - That your items are evenly spaced (same height)
			// - That you have cheap random access to your elements (you can access them given their index,
			//   without processing all the ones before)
			// You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
			// We would need random-access on the post-filtered list.
			// A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
			// or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
			// and appending newly elements as they are inserted. This is left as a task to the user until we can manage
			// to improve this example code!
			// If your items are of variable height:
			// - Split them into same height items would be simpler and facilitate random-seeking into your list.
			// - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
			if (copy_to_clipboard)
				ImGui::LogToClipboard();
			for (const SubStr &item : Items)
			{
				if (!Filter.PassFilter(item.begin, item.end))
					continue;

				// Normally you would store more information in your item than just a string.
				// (e.g. make Items[] an array of structure, store color/type etc.)
				ImVec4 color;
				bool has_color = false;
				if(item.StartsWith("[error]")){ color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
				else if (item.StartsWith("# ")) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
				if (has_color)
					ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(item.begin, item.end);
				if (has_color)
					ImGui::PopStyleColor();
			}
			if (copy_to_clipboard)
				ImGui::LogFinish();

			// Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
			// Using a scrollbar or mouse-wheel will take away from the bottom edge.
			if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
				ImGui::SetScrollHereY(1.0f);
			ScrollToBottom = false;

			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
		ImGui::Separator();

		// Command-line
		bool reclaim_focus = false;
		ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit;
		if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
		{
			SubStr buf = SubStr(InputBuf, mBufLen);
			char* s = InputBuf;
			while(buf.end > buf.begin && (*(buf.end - 1) == ' ') )
					buf.end--;
			if (s[0])
				ExecCommand(buf);
			mBufLen = 0;
			InputBuf[0] = 0;
			reclaim_focus = true;
		}

		// Auto-focus on window apparition
		ImGui::SetItemDefaultFocus();
		if (reclaim_focus)
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

		ImGui::End();
	}

	void    ExecCommand(const SubStr &command_line)
	{
		AddLog("# %s\n", command_line);

		// Insert into history. First find match and delete it so it can be pushed to the back.
		// This isn't trying to be smart or optimal.
		HistoryPos = -1;
		for (int i = History.Size - 1; i >= 0; i--)
			if (History[i].Equals(command_line))
			{
				History[i].Free();
				History.erase(History.begin() + i);
				break;
			}
		History.push_back(command_line.StrDup());

		// Process command
		if (command_line.Equals("clear"))
		{
			ClearLog();
		}
		else if (command_line.Equals("help"))
		{
			AddLog("Internal Commands:");
			for (int i = 0; i < IM_ARRAYSIZE(InternalCommands); i++)
				AddLog("- %s", InternalCommands[i]);
			AddLog("Client Commands:");
			for (int i = 1; i < IM_ARRAYSIZE(gCommands); i++)
				AddLog("- %s (%s) %s", gCommands[i].name, gCommands[i].sign, gCommands[i].description);
		}
		else if (command_line.Equals("history"))
		{
			int first = History.Size - 10;
			for (int i = first > 0 ? first : 0; i < History.Size; i++)
				AddLog("%3d: %s\n", i, History[i]);
		}
		else
		{
			Command c = Command(command_line);
			if(c.ctype != EVENT_POLL_NULL)
				RunCommand(mPid, c);
			else
				AddLog("Unknown command: '%s'\n", command_line);
		}

		// On command input, we scroll to bottom even if AutoScroll==false
		ScrollToBottom = true;
	}

	// In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
	static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
	{
		AppConsole* console = (AppConsole*)data->UserData;
		return console->TextEditCallback(data);
	}

	int     TextEditCallback(ImGuiInputTextCallbackData* data)
	{
		//AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
		switch (data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
			{
				// Example of TEXT COMPLETION

				// Locate beginning of current word
				const char* word_end = data->Buf + data->CursorPos;
				const char* word_start = word_end;
				while (word_start > data->Buf)
				{
					const char c = word_start[-1];
					if (c == ' ' || c == '\t' || c == ',' || c == ';')
						break;
					word_start--;
				}

				// Build a list of candidates
				ImVector<SubStr> candidates;
				SubStr word = SubStr{word_start, word_end};
				for (int i = 1; i < IM_ARRAYSIZE(gCommands); i++)
					if(gCommands[i].name.StartsWith(word))
						candidates.push_back(gCommands[i].name);
				for (int i = 0; i < IM_ARRAYSIZE(InternalCommands); i++)
					if(InternalCommands[i].StartsWith(word))
						candidates.push_back(InternalCommands[i]);

				if (candidates.Size == 0)
				{
					// No match
					AddLog("No match for \"%s\"!\n", word);
				}
				else if (candidates.Size == 1)
				{
					// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
					data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
					data->InsertChars(data->CursorPos, candidates[0].begin, candidates[0].end);
					data->InsertChars(data->CursorPos, " ");
				}
				else
				{
					// Multiple matches. Complete as much as we can..
					// So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
					int match_len = (int)(word_end - word_start);
					for (;;)
					{
						int c = 0;
						bool all_candidates_matches = true;
						for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
							if (i == 0)
								c = candidates[i].begin[match_len];
							else if (c == 0 || c != candidates[i].begin[match_len])
								all_candidates_matches = false;
						if (!all_candidates_matches)
							break;
						match_len++;
					}

					if (match_len > 0)
					{
						data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
						data->InsertChars(data->CursorPos, candidates[0].begin, candidates[0].begin + match_len);
					}

					// List matches
					AddLog("Possible matches:\n");
					for (int i = 0; i < candidates.Size; i++)
						AddLog("- %s\n", candidates[i]);
				}

				break;
			}
		case ImGuiInputTextFlags_CallbackHistory:
			{
				// Example of HISTORY
				const int prev_history_pos = HistoryPos;
				if (data->EventKey == ImGuiKey_UpArrow)
				{
					if (HistoryPos == -1)
						HistoryPos = History.Size - 1;
					else if (HistoryPos > 0)
						HistoryPos--;
				}
				else if (data->EventKey == ImGuiKey_DownArrow)
				{
					if (HistoryPos != -1)
						if (++HistoryPos >= History.Size)
							HistoryPos = -1;
				}

				// A better implementation would preserve the data on the current input line along with cursor position.
				if (prev_history_pos != HistoryPos)
				{
					const char* history_str = (HistoryPos >= 0) ? History[HistoryPos].begin : "";
					data->DeleteChars(0, data->BufTextLen);
					data->InsertChars(0, history_str);
				}
			}
		}

		mBufLen = data->BufTextLen;
		return 0;
	}
} gConsole;


template <typename Type, const auto &NAME, size_t nlen>
struct StructField_ : Type
{
	constexpr static const char *name = NAME;
	StructField_() = default;
	StructField_(const Type &t){ *(Type*)this = t;}
	template <typename DumperType>
	StructField_(Dumper<DumperType> &l){
		auto &vv = l.GetData(this);
		DumperType d2 = l.d;
		if( l.d.show)
		{
			d2.tname = name;
			DumpNamedStruct(d2, (Type*)&vv);
		}

	}
};

#define StructField(Type, name) \
constexpr static const char fld_name_##name[] = #name; \
	StructField_<Type, fld_name_##name, sizeof(fld_name_##name) - 1> name


#define HashMapField(Key, Value, name) \
constexpr static const char fld_name_##name[] = #name; \
	HashMapField_<Key, Value, fld_name_##name, sizeof(fld_name_##name) - 1> name

void NameForKey(char (&keyname)[32], unsigned long long key)
{
	SBPrint(keyname, "%16x", key);
}

void NameForKey(char (&keyname)[32], const SubStr &key)
{
	SBPrint(keyname, "%s", key);
}

template <typename Key, typename Value, const auto &NAME, size_t nlen>
struct HashMapField_ : HashMap<Key, Value>
{
	constexpr static const char *name = NAME;
	HashMapField_() = default;
	template <typename DumperType>
	HashMapField_(Dumper<DumperType> &l){
		auto &vv = l.GetData(this);
		DumperType d2 = l.d;
		d2.startOpen = false;

		if(l.d.show)
		{
			d2.tname = name;
			d2.Begin();
			if(d2.show)
			{
				DumperType d3 = l.d;
				HASHMAP_FOREACH(vv, node)
				{
					char keyname[32];
					NameForKey(keyname,node->k);
					d3.tname = keyname;

					DumpNamedStruct(d3, &node->v);
				}
			}
			d2.End();
		}

	}
};

struct AppActionState
{
	StructField(AppAction,mState);
	HashMapField(int, AppBinding, bindings);
};

struct AppActionSetState
{
	StructField(AppActionSet,mState);
	HashMapField(SubStr,AppActionState,mActions);
};

struct AppSessionState
{
	AppSession mState;
	HashMap<SubStr,AppActionSetState> mActionsSets;
	HashMap<SubStr,AppSource> mSources[USER_PATH_COUNT + 1];
	HashMap<unsigned long long, AppActionMap> mActionMaps[USER_PATH_COUNT];
	HashMap<SubStr, AppRPN> mExpressions;
	HashMap<int, AppCustomAction> mCustomActions;
	bool show;
	char mSessionName[32];
	void SetName(int pid, unsigned long long handle)
	{
		if(mSessionName[0])
			return;
		SBPrint(mSessionName, "Session %x (%d)", handle, pid);
		show = true;
	}
};

struct AppState
{
	AppConsole mConsole;
	int pid;
	AppReg mReg;
	HashMap<unsigned long long, AppSessionState> mSessions;
	HashMap<SubStr, AppVar> mVariables;
	char mAppName[64];
	char mVarName[64];
	bool showVars;

	void SetName(const char *name)
	{
		if(!mConsole.mWindowName[0])
		{
			SBPrint(mConsole.mWindowName, "%s (%d) - Console###CON_%d", name, pid, pid);
			mConsole.show = true;

			SBPrint(mAppName, "%s (%d)###APP_%d", name, pid, pid);
			SBPrint(mVarName, "%s (%d) - Variables###APP_%d", name, pid, pid);

		mConsole.mPid = pid;
		}
	}
};

static HashMap<int, AppState> gApps;

struct EventDumper
{
	AppConsole &console;
	const char *tname;
	void Begin(){}
	void Dump(const SubStr &name, const SubStr &val)
	{
		console.AddLog("received %s: %s %s\n", tname, name, val);
	}
	void End(){}
};


struct HandleConsole
{
	AppState *state;
	template <typename T>
	void Handle(const T &packet) const
	{
		EventDumper d{state? state->mConsole : gConsole,TypeName<T>()};
		DumpNamedStruct(d, &packet);
	}
};


struct HandleApp
{
	int pid;
	AppState &state;
	template <typename T>
	void Handle(const T &packet) const{}
	void Handle(const AppSession &session) const
	{
		state.mSessions[session.handle].mState = session;
		state.mSessions[session.handle].SetName(pid, session.handle);
	}
	void Handle(const AppActionSet &set) const
	{
		state.mSessions[set.session.val].mActionsSets[SubStrB(set.setName.val)].mState = set;
		state.mSessions[set.session.val].SetName(pid, set.session.val);
	}
	void Handle(const AppAction &act) const
	{
		state.mSessions[act.session.val].mActionsSets[SubStrB(act.setName.val)].mActions[SubStrB(act.actName.val)].mState = act;
		state.mSessions[act.session.val].SetName(pid, act.session.val);
	}
	void Handle(const AppBinding &act) const
	{
		state.mSessions[act.session.val].mActionsSets[SubStrB(act.setName.val)].mActions[SubStrB(act.actName.val)].bindings[act.index] = act;
		state.mSessions[act.session.val].SetName(pid, act.session.val);
	}
	void Handle(const AppVar &var) const
	{
		state.mVariables[SubStrB(var.name.val)] = var;
	}
	void Handle(const AppSource &src) const
	{
		state.mSessions[src.session.val].mSources[src.index][SubStrB(src.name.val)] = src;
		state.mSessions[src.session.val].SetName(pid, src.session.val);
	}
	void Handle(const AppActionMap &map) const
	{
		state.mSessions[map.session.val].mActionMaps[map.hand][map.handle] = map;
		state.mSessions[map.session.val].SetName(pid, map.session.val);
	}
	void Handle(const AppRPN &rpn) const
	{
		state.mSessions[rpn.session.val].mExpressions[SubStrB(rpn.source.val)] = rpn;
		state.mSessions[rpn.session.val].SetName(pid, rpn.session.val);
	}
	void Handle(const AppCustomAction &act) const
	{
		state.mSessions[act.session.val].mCustomActions[act.index] = act;
		state.mSessions[act.session.val].SetName(pid, act.session.val);
	}
};


struct ImGuiTreeDumper
{
	const char *tname;
	bool show;
	void Begin()
	{
		show = ImGui::TreeNode(tname);
	}
	bool Dump(const SubStr &name, const SubStr &val)
	{
		if(!show)
			return false;
		ImGui::TextUnformatted(name);
		ImGui::SameLine();
		ImGui::TextUnformatted(val);
		return true;
	}
	void End()
	{
		if(show)
		ImGui::TreePop();
	}
};

struct ImGuiTableTreeDumper
{
	const char *tname;
	bool show;
	bool startOpen = false;
	void Begin()
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted("<object>");
		ImGui::TableSetColumnIndex(0);
		if(startOpen)
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		show = ImGui::TreeNode(tname);
	}
	bool Dump(const SubStr &name, const SubStr &val)
	{
		if(!show)
			return false;
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TreeNodeEx(name.begin, ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted(val);
		return true;
	}
	void End()
	{
		if(show)
			ImGui::TreePop();
	}
};

static unsigned int gRequestFrames = 3;
static struct Client
{
	int fd;
	bool Start(int port)
	{
		fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = htons(port);
		connect(fd, (sockaddr*)&addr, sizeof(addr));
		ClientReg reg = {true};
		Send(reg);
		return true;
	}
	template <typename T>
	int Send(const T &data, unsigned char target = 0xF, pid_t targetPid = 0)
	{
		EventPacket p;
		p.head.sourcePid = getpid();
		SubStr("client_simple").CopyTo(p.head.displayName);
		p.head.target = target;
		p.head.targetPid = targetPid;
		p.head.type = T::type;
		memcpy(&p.data, &data, sizeof(data));
		return send(fd,&p, ((char*)&p.data - (char*)&p) + sizeof(data), 0);
	}
	void RunFrame(unsigned int time)
	{
		EventPacket p;
		struct timeval tv;
		fd_set rfds;
		tv.tv_sec = 0;
		tv.tv_usec = time;
		FD_ZERO( &rfds );
		FD_SET(fd, &rfds);
		if( select( fd + 1, &rfds, NULL, NULL,&tv ) > 0)
		{
			if(!(FD_ISSET(fd,&rfds) && (recv(fd, &p, sizeof(p), MSG_DONTWAIT) >= 0)))
				return;
			gRequestFrames = 4;
			if(p.head.type == EVENT_APP_REGISTER)
			{
				AppState &s = gApps[p.head.sourcePid];
				s.pid = p.head.sourcePid;
				s.SetName(p.head.displayName);
				if(!s.mReg.name.val[0] && p.data.appReg.name.val[0])
					s.mReg = p.data.appReg;
			}
			AppState *s = gApps.GetPtr(p.head.sourcePid);
			HandleConsole h{s};
			HandlePacket(h, p);
			if(s)
			{
				HandleApp a{p.head.sourcePid,*s};
				HandlePacket(a, p);
			}
		}
	}
} gClient;

static void RunCommand(int pid, const Command &c)
{
	gClient.Send(c,TARGET_APP, pid);
}
constexpr const char *gUserDevNames[] =
{
	"Left",
	"Right",
	"Head",
	"Gamepad",
	"Auto",
	"External"
};

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

void FrameControl()
{
	static unsigned long long lastTime;
	unsigned long long time = GetTimeU64();
	long long timeDiff = sleepTimes[gRequestFrames] - (time - lastTime);
	if(timeDiff <= 5000000)
		timeDiff = 0;
	gClient.RunFrame(timeDiff / 1000);
	lastTime = time;
}

void DrawActionSetTab(AppSessionState &s)
{
	if(ImGui::Button("Get Application sets"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_APP_BINDINGS;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}
	ImGui::SameLine();
	if(ImGui::Button("Get Layer sets"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_LAYER_BINDINGS;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}
	HASHMAP_FOREACH(s.mActionsSets, as)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if(ImGui::CollapsingHeader(as->k.begin))
		{
			ImGui::PushID(as->k.begin);
			if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter| ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("Object");
				ImGui::TableSetupColumn("Contents");
				ImGui::TableHeadersRow();
				ImGuiTableTreeDumper d{"Action set info"};
				d.startOpen = true;
				DumpNamedStruct(d, &as->v);
				ImGui::EndTable();
			}
			ImGui::PopID();
		}
	}
}

void DrawSourcesTab(AppSessionState &s)
{
	if(ImGui::Button("Update"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_SOURCES;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}
	//ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	//if(ImGui::CollapsingHeader("Sources dump"))
	{
		if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter| ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Object");
			ImGui::TableSetupColumn("Contents");
			ImGui::TableHeadersRow();

			for(int i = 0; i < USER_PATH_COUNT + 1; i++)
			{
				ImGuiTableTreeDumper d{gUserDevNames[i]};
				if(s.mSources[i].Begin().n == nullptr)
					continue;
				d.startOpen = true;
				d.Begin();
				if(d.show)
				HASHMAP_FOREACH(s.mSources[i], src)
				{
					char key[32];
					ImGuiTableTreeDumper d2{key};
					NameForKey(key, src->k);

					DumpNamedStruct(d2, &src->v);
				}
				d.End();
			}

			ImGui::EndTable();
		}
	}
}

void DrawActionMapsTab(AppSessionState &s)
{
	if(ImGui::Button("Update"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_ACTION_MAPS;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}
	//ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	//if(ImGui::CollapsingHeader("Action maps dump"))
	{
		if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter| ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Object");
			ImGui::TableSetupColumn("Contents");
			ImGui::TableHeadersRow();

			for(int i = 0; i < USER_PATH_COUNT; i++)
			{
				ImGuiTableTreeDumper d{gUserDevNames[i]};
				d.startOpen = true;
				d.Begin();
				if(d.show)
				HASHMAP_FOREACH(s.mActionMaps[i], src)
				{
					char key[64];
					ImGuiTableTreeDumper d2{key};
					SBPrint(key, "%s##%x", src->v.actName.val, src->k);
					d2.startOpen = src->v.override;
					DumpNamedStruct(d2, &src->v);
				}
				d.End();
			}

			ImGui::EndTable();
		}
	}
}


void DrawExpressionsTab(AppSessionState &s)
{
	if(ImGui::Button("Update"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_EXPRESSIONS;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}

	if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter| ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Object");
		ImGui::TableSetupColumn("Contents");
		ImGui::TableHeadersRow();


		HASHMAP_FOREACH(s.mExpressions, src)
		{
			ImGuiTableTreeDumper d2{src->k.begin};
			d2.startOpen = true;
			DumpNamedStruct(d2, &src->v);
		}

		ImGui::EndTable();
	}
}

void DrawCustomActionsTab(AppSessionState &s)
{
	if(ImGui::Button("Update"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_CUSTOM_ACTIONS;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}

	if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter| ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Object");
		ImGui::TableSetupColumn("Contents");
		ImGui::TableHeadersRow();


		HASHMAP_FOREACH(s.mCustomActions, src)
		{
			char key[32];
			NameForKey(key, src->k);
			ImGuiTableTreeDumper d2{key};
			d2.startOpen = true;
			DumpNamedStruct(d2, &src->v);
		}

		ImGui::EndTable();
	}
}

void DrawSessionWindow(AppSessionState &s)
{
	ImGui::Begin( s.mSessionName, &s.show );
	if(ImGui::BeginTabBar("##tabs"))
	{
		if(ImGui::BeginTabItem("Action sets"))
		{
			DrawActionSetTab(s);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Sources"))
		{
			DrawSourcesTab(s);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Action maps"))
		{
			DrawActionMapsTab(s);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Expressions"))
		{
			DrawExpressionsTab(s);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Custom actions"))
		{
			DrawCustomActionsTab(s);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

void DrawVariablesWindow(AppState &s)
{

	ImGui::Begin( s.mVarName, &s.showVars );
	if(ImGui::Button("Update"))
	{
		Command cmd;
		cmd.ctype = EVENT_POLL_DUMP_VARIABLES;
		cmd.datasize = 0;
		gClient.Send(cmd);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
	if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter| ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Object");
		ImGui::TableSetupColumn("Contents");
		ImGui::TableHeadersRow();

		HASHMAP_FOREACH(s.mVariables, src)
		{
			ImGuiTableTreeDumper d{src->k.begin};
			d.startOpen = true;
			DumpNamedStruct(d, &src->v);
		}

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();
	ImGui::End();
}


int main(int argc, char **argv)
{
	if(argc < 2)
		return 0;
	gClient.Start(atoi(argv[1]));
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
	bool show_demo_window = false;
	bool frameSkipped = false;
	SubStr("Console").CopyTo(gConsole.mWindowName);

	while(!done)
	{
		bool hasEvents;
		done = !Platform_ProcessEvents(hasEvents);
		if(hasEvents)
			gRequestFrames = 3;
		if(gRequestFrames < 2)
		{
			FrameControl();
			if(gRequestFrames)
				gRequestFrames--;
			frameSkipped = true;
			continue;
		}
		Platform_NewFrame();
		ImGui::NewFrame();
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);
		ImGui::SetNextWindowSize(ImVec2(400,200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Client");
		ImGui::Checkbox("Demo", &show_demo_window);
		ImGui::Checkbox("Global console", &gConsole.show);
		//bool showApps = true;
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if(ImGui::CollapsingHeader("Apps"))
		{
			HASHMAP_FOREACH(gApps, node)
			{
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				if(ImGui::TreeNode(node->v.mAppName))
				{
					ImGui::Checkbox("Console", &node->v.mConsole.show);
					ImGui::TextUnformatted("Request dump:");
					if(ImGui::Button("Session"))
					{
						Command cmd;
						cmd.ctype = EVENT_POLL_DUMP_SESSION;
						cmd.datasize = 0;
						gClient.Send(cmd);
					}
					ImGui::SameLine();
					if(ImGui::Button("Variables"))
					{
						Command cmd;
						cmd.ctype = EVENT_POLL_DUMP_VARIABLES;
						cmd.datasize = 0;
						gClient.Send(cmd);
						node->v.showVars = true;
					}
					if(ImGui::Button("Reload config"))
					{
						Command cmd;
						cmd.ctype = EVENT_POLL_RELOAD_CONFIG;
						cmd.datasize = 0;
						gClient.Send(cmd);
					}
					if(ImGui::Button("Trigger reload bindings"))
					{
						Command cmd;
						cmd.ctype = EVENT_POLL_TRIGGER_INTERACTION_PROFILE_CHANGED;
						cmd.datasize = 0;
						gClient.Send(cmd);
					}
					ImGuiTreeDumper d{"Registration info"};
					DumpNamedStruct(d, &node->v.mReg);
					HASHMAP_FOREACH(node->v.mSessions, ses)
					{

						ImGui::SetNextItemOpen(true, ImGuiCond_Once);
						if(ImGui::TreeNode(ses->v.mSessionName))
						{
							d.tname = "Session state";
							DumpNamedStruct(d, &ses->v.mState);
							ImGui::Checkbox("Show session window", &ses->v.show);
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
		if(gConsole.show)
			gConsole.Draw();
		HASHMAP_FOREACH(gApps, node)
		{
			if(node->v.mConsole.show)
				node->v.mConsole.Draw();
			HASHMAP_FOREACH(node->v.mSessions, ses)
				if(ses->v.show)
					DrawSessionWindow(ses->v);
			if(node->v.showVars)
				DrawVariablesWindow(node->v);
		}

		ImGui::Render();
		if(!frameSkipped)
		{
			Platform_Present(io);
			FrameControl();
			gRequestFrames--;
		}

		frameSkipped = false;
	}
	Platform_Shutdown();
}
