// dear imgui: Renderer Backend for OpenGL2 (legacy OpenGL, fixed pipeline)
// This needs to be used along with a Platform Backend (e.g. GLFW, SDL, Win32, custom..)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'GLuint' OpenGL texture identifier as void*/ImTextureID. Read the FAQ about ImTextureID!

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in imgui_impl_opengl3.cpp**
// This code is mostly provided as a reference to learn how ImGui integration works, because it is shorter to read.
// If your code is using GL3+ context or any semi modern OpenGL calls, using this is likely to make everything more
// complicated, will require your code to reset every single OpenGL attributes to their initial state, and might
// confuse your GPU driver.
// The GL2 code is unable to reset attributes or even call e.g. "glUseProgram(0)" because they don't exist in that API.

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2022-10-11: Using 'nullptr' instead of 'NULL' as per our switch to C++11.
//  2021-12-08: OpenGL: Fixed mishandling of the ImDrawCmd::IdxOffset field! This is an old bug but it never had an effect until some internal rendering changes in 1.86.
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-05-19: OpenGL: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)
//  2021-01-03: OpenGL: Backup, setup and restore GL_SHADE_MODEL state, disable GL_STENCIL_TEST and disable GL_NORMAL_ARRAY client state to increase compatibility with legacy OpenGL applications.
//  2020-01-23: OpenGL: Backup, setup and restore GL_TEXTURE_ENV to increase compatibility with legacy OpenGL applications.
//  2019-04-30: OpenGL: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2019-02-11: OpenGL: Projecting clipping rectangles correctly using draw_data->FramebufferScale to allow multi-viewports for retina display.
//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.
//  2018-08-03: OpenGL: Disabling/restoring GL_LIGHTING and GL_COLOR_MATERIAL to increase compatibility with legacy OpenGL applications.
//  2018-06-08: Misc: Extracted imgui_impl_opengl2.cpp/.h away from the old combined GLFW/SDL+OpenGL2 examples.
//  2018-06-08: OpenGL: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplOpenGL2_RenderDrawData() in the .h file so you can call it yourself.
//  2017-09-01: OpenGL: Save and restore current polygon mode.
//  2016-09-10: OpenGL: Uploading font texture as RGBA32 to increase compatibility with users shaders (not ideal).
//  2016-09-05: OpenGL: Fixed save and restore of current scissor rectangle.

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_opengl2.h"
#include <stdint.h>     // intptr_t

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"                      // warning: macro is not used
#pragma clang diagnostic ignored "-Wnonportable-system-include-path"
#endif

// Include OpenGL header (without an OpenGL loader) requires a bit of fiddling
#if defined(_WIN32) && !defined(APIENTRY)
#define APIENTRY __stdcall                  // It is customary to use APIENTRY for OpenGL function pointer declarations on all platforms.  Additionally, the Windows OpenGL header needs APIENTRY.
#endif
#if defined(_WIN32) && !defined(WINGDIAPI)
#define WINGDIAPI __declspec(dllimport)     // Some Windows OpenGL headers need this
#endif
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
extern void *DYN_GL_GetProcAddress(const char *proc);

void (APIENTRY*pglMatrixMode)(GLenum mode);
void (APIENTRY*pglLoadIdentity)();
void (APIENTRY*pglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (APIENTRY*pglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (APIENTRY*pglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (APIENTRY*pglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
void (APIENTRY*pglBindTexture)(GLenum target, GLuint texture);
void (APIENTRY*pglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (APIENTRY*pglTexParameteri)(GLenum target, GLenum pname, GLint param);
void (APIENTRY*pglPixelStorei)(GLenum pname, GLint param);
void (APIENTRY*pglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY*pglBlendFunc)(GLenum sfactor, GLenum dfactor);
void (APIENTRY*pglDisable)(GLenum cap);
void (APIENTRY*pglEnable)(GLenum cap);
void (APIENTRY*pglPolygonMode)(GLenum face, GLenum mode);
void (APIENTRY*pglShadeModel)(GLenum mode);
void (APIENTRY*pglTexEnvi)(GLenum target, GLenum pname, GLint param);
void (APIENTRY*pglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
void (APIENTRY*pglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (APIENTRY*pglGenTextures)(GLsizei n, GLuint *textures);
void (APIENTRY*pglEnableClientState)(GLenum cap);
void (APIENTRY*pglDisableClientState)(GLenum cap);
void (APIENTRY*pglDeleteTextures)(GLsizei n, const GLuint *textures);

#define LOAD_FUNC(x) *(void**)&p##x = DYN_GL_GetProcAddress(#x);
struct ImGui_ImplOpenGL2_Data
{
	GLuint       FontTexture;

	ImGui_ImplOpenGL2_Data() { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplOpenGL2_Data* ImGui_ImplOpenGL2_GetBackendData()
{
	return ImGui::GetCurrentContext() ? (ImGui_ImplOpenGL2_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Functions
bool    ImGui_ImplOpenGL2_Init()
{
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

	// Setup backend capabilities flags
	ImGui_ImplOpenGL2_Data* bd = IM_NEW(ImGui_ImplOpenGL2_Data)();
	io.BackendRendererUserData = (void*)bd;
	io.BackendRendererName = "imgui_impl_opengl2";
	LOAD_FUNC(glMatrixMode);
	LOAD_FUNC(glLoadIdentity);
	LOAD_FUNC(glOrtho);
	LOAD_FUNC(glTexCoordPointer);
	LOAD_FUNC(glColorPointer);
	LOAD_FUNC(glScissor);
	LOAD_FUNC(glBindTexture);
	LOAD_FUNC(glDrawElements);
	LOAD_FUNC(glTexParameteri);
	LOAD_FUNC(glPixelStorei);
	LOAD_FUNC(glTexImage2D);
	LOAD_FUNC(glBlendFunc);
	LOAD_FUNC(glDisable);
	LOAD_FUNC(glEnable);
	LOAD_FUNC(glPolygonMode);
	LOAD_FUNC(glShadeModel);
	LOAD_FUNC(glTexEnvi);
	LOAD_FUNC(glViewport);
	LOAD_FUNC(glVertexPointer);
	LOAD_FUNC(glGenTextures);
	LOAD_FUNC(glEnableClientState);
	LOAD_FUNC(glDisableClientState);
	LOAD_FUNC(glDeleteTextures);
	return true;
}

void    ImGui_ImplOpenGL2_Shutdown()
{
	ImGui_ImplOpenGL2_Data* bd = ImGui_ImplOpenGL2_GetBackendData();
	IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplOpenGL2_DestroyDeviceObjects();
	io.BackendRendererName = nullptr;
	io.BackendRendererUserData = nullptr;
	IM_DELETE(bd);
}

void    ImGui_ImplOpenGL2_NewFrame()
{
	ImGui_ImplOpenGL2_Data* bd = ImGui_ImplOpenGL2_GetBackendData();
	IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplOpenGL2_Init()?");

	if (!bd->FontTexture)
		ImGui_ImplOpenGL2_CreateDeviceObjects();
}

static void ImGui_ImplOpenGL2_SetupRenderState(ImDrawData* draw_data, int fb_width, int fb_height)
{
	pglViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();
	pglOrtho(draw_data->DisplayPos.x, draw_data->DisplayPos.x + draw_data->DisplaySize.x, draw_data->DisplayPos.y + draw_data->DisplaySize.y, draw_data->DisplayPos.y, -1.0f, +1.0f);
}

// OpenGL2 Render function.
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state explicitly.
// This is in order to be able to run within an OpenGL engine that doesn't do so.
void ImGui_ImplOpenGL2_RenderDrawData(ImDrawData* draw_data)
{
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width == 0 || fb_height == 0)
		return;


	// Setup desired GL state
	ImGui_ImplOpenGL2_SetupRenderState(draw_data, fb_width, fb_height);

	// Will project scissor/clipping rectangles into framebuffer space
	ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
	ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

	// Render command lists
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
		const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
		pglVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + offsetof(ImDrawVert, pos)));
		pglTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + offsetof(ImDrawVert, uv)));
		pglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + offsetof(ImDrawVert, col)));

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					ImGui_ImplOpenGL2_SetupRenderState(draw_data, fb_width, fb_height);
				else
					pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
				ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
				if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
					continue;

				// Apply scissor/clipping rectangle (Y is inverted in OpenGL)
				pglScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));

				// Bind texture, Draw
				pglBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->GetTexID());
				pglDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer + pcmd->IdxOffset);
			}
		}
	}
}

bool ImGui_ImplOpenGL2_CreateFontsTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplOpenGL2_Data* bd = ImGui_ImplOpenGL2_GetBackendData();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

	// Upload texture to graphics system
	// (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)

	pglGenTextures(1, &bd->FontTexture);
	pglBindTexture(GL_TEXTURE_2D, bd->FontTexture);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// Store our identifier
	io.Fonts->SetTexID((ImTextureID)(intptr_t)bd->FontTexture);

	return true;
}

void ImGui_ImplOpenGL2_DestroyFontsTexture()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplOpenGL2_Data* bd = ImGui_ImplOpenGL2_GetBackendData();
	if (bd->FontTexture)
	{
		pglDeleteTextures(1, &bd->FontTexture);
		io.Fonts->SetTexID(0);
		bd->FontTexture = 0;
	}
}

bool    ImGui_ImplOpenGL2_CreateDeviceObjects()
{
	// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, vertex/texcoord/color pointers, polygon fill.
	pglEnable(GL_BLEND);
	pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // In order to composite our output buffer we need to preserve alpha
	pglDisable(GL_CULL_FACE);
	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_STENCIL_TEST);
	pglDisable(GL_LIGHTING);
	pglDisable(GL_COLOR_MATERIAL);
	pglEnable(GL_SCISSOR_TEST);
	pglEnableClientState(GL_VERTEX_ARRAY);
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglEnableClientState(GL_COLOR_ARRAY);
	pglDisableClientState(GL_NORMAL_ARRAY);
	pglEnable(GL_TEXTURE_2D);
	pglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	pglShadeModel(GL_SMOOTH);
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
	// you may need to backup/reset/restore other state, e.g. for current shader using the commented lines below.
	// (DO NOT MODIFY THIS FILE! Add the code in your calling function)
	//   GLint last_program;
	//   glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	//   glUseProgram(0);
	//   ImGui_ImplOpenGL2_RenderDrawData(...);
	//   glUseProgram(last_program)
	// There are potentially many more states you could need to clear/setup that we can't access from default headers.
	// e.g. glBindBuffer(GL_ARRAY_BUFFER, 0), glDisable(GL_TEXTURE_CUBE_MAP).

	// Setup viewport, orthographic projection matrix
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();
	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity();
	return ImGui_ImplOpenGL2_CreateFontsTexture();
}

void    ImGui_ImplOpenGL2_DestroyDeviceObjects()
{
	ImGui_ImplOpenGL2_DestroyFontsTexture();
}

//-----------------------------------------------------------------------------

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // #ifndef IMGUI_DISABLE
