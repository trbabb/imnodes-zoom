// Render test for imnodes.
//
// Renders a small node graph either headlessly (saved as a PNG) or in an
// interactive SDL window. Intended for agents to iterate on imnodes changes
// and inspect the visual result directly, without needing a display when
// running headlessly.
//
// Usage:
//   render_test [output.png]       # headless: render and write PNG, then exit
//   render_test -i | --interactive # show a window, run an event loop
//                                  # press 's' to save a snapshot PNG,
//                                  # 'q' or Esc to quit
//
// Default output path: imnodes_test.png in the current working directory.
//
// Implementation notes:
// - SDL2 + OpenGL 3.2 Core (matches the existing examples in example/main.cpp)
//   so that the imgui_impl_opengl3 backend can be reused unchanged.
// - The SDL window is created hidden. We render into a user FBO and call
//   glReadPixels to recover RGBA pixels. The window is only needed to obtain
//   a valid GL context; the default framebuffer is never presented.
// - ImGui needs an explicit display size and a non-zero delta-time before its
//   first NewFrame, since we are not driving it from the SDL event loop. We
//   set DisplaySize from the target image size and fix DeltaTime to 1/60.
// - We pump several frames before reading pixels. imnodes lazily initializes
//   editor / node sizes on the first layout pass, and links route between
//   pin positions that are not known until after at least one node has been
//   submitted. Two frames are enough in practice; we use four to be safe.

#include <imnodes.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL2/SDL.h>
// FBOs / glReadPixels into a user texture require the GL 3.x core API. The
// platform headers used by the existing examples (<SDL2/SDL_opengl.h>) pull in
// the legacy desktop GL headers, which on macOS do not expose FBO entry points
// at all. Include the core-profile headers directly here.
#if defined(__APPLE__)
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#elif defined(_WIN32)
#  include <SDL2/SDL_opengl.h>
#else
#  define GL_GLEXT_PROTOTYPES
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include <png.h>

#include "test_common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using imnodes_test::kImageHeight;
using imnodes_test::kImageWidth;
using imnodes_test::build_node_graph;
using imnodes_test::save_framebuffer_png;
using imnodes_test::seed_initial_node_positions;

namespace
{
// Number of frames to pump before reading pixels. imnodes resolves node /
// pin geometry lazily, so the first frame's link routes are stale. Two is
// usually enough; four for headroom.
constexpr int kWarmupFrames = 4;
} // namespace

int main(int argc, char** argv)
{
    // Argument parsing. Interactive mode takes precedence; in headless mode the
    // first positional arg overrides the default PNG path.
    bool interactive = false;
    const char* out_path = "imnodes_test.png";
    for (int i = 1; i < argc; ++i)
    {
        const char* a = argv[i];
        if (std::strcmp(a, "-i") == 0 || std::strcmp(a, "--interactive") == 0)
            interactive = true;
        else if (a[0] != '-')
            out_path = a;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // GL 3.2 Core, matching example/main.cpp so imgui_impl_opengl3 is happy.
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (interactive)
        window_flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    else
        window_flags |= SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow(
        "imnodes_render_test",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        kImageWidth,
        kImageHeight,
        window_flags);
    if (!window)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx)
    {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(interactive ? 1 : 0);

    // Headless mode renders into an FBO so the (hidden) default framebuffer's
    // presentation state doesn't matter. Interactive mode renders straight to
    // the visible default framebuffer.
    GLuint fbo = 0, color_tex = 0;
    if (!interactive)
    {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &color_tex);
        glBindTexture(GL_TEXTURE_2D, color_tex);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, kImageWidth, kImageHeight, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::fprintf(stderr, "FBO incomplete\n");
            return 1;
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    if (!interactive)
    {
        // Headless: no event loop, so we must seed display size and dt before
        // the first NewFrame, and disable any IO that depends on cursor state.
        io.DisplaySize = ImVec2((float)kImageWidth, (float)kImageHeight);
        io.DeltaTime   = 1.0f / 60.0f;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initial node positions, in editor (grid) space. Must happen after the
    // imnodes context is created.
    seed_initial_node_positions();

    // The clear color matches example/main.cpp (a muted slate) so that visual
    // diffs against screenshots of the existing interactive examples are
    // meaningful.
    const ImVec4 clear_color(0.45f, 0.55f, 0.60f, 1.0f);

    auto render_one_frame = [&](GLuint target_fbo, int vp_w, int vp_h) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        build_node_graph();
        ImGui::Render();
        glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
        glViewport(0, 0, vp_w, vp_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };

    if (interactive)
    {
        std::printf(
            "interactive mode: drag nodes; 's' = save PNG (%s); 'q' or Esc = quit\n",
            out_path);
        bool done = false;
        while (!done)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT)
                    done = true;
                if (event.type == SDL_WINDOWEVENT
                    && event.window.event == SDL_WINDOWEVENT_CLOSE
                    && event.window.windowID == SDL_GetWindowID(window))
                    done = true;
                if (event.type == SDL_KEYDOWN)
                {
                    SDL_Keycode k = event.key.keysym.sym;
                    if (k == SDLK_ESCAPE || k == SDLK_q)
                        done = true;
                    else if (k == SDLK_s)
                    {
                        int w = 0, h = 0;
                        SDL_GL_GetDrawableSize(window, &w, &h);
                        save_framebuffer_png(out_path, w, h);
                    }
                }
            }
            int draw_w = 0, draw_h = 0;
            SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);
            render_one_frame(0, draw_w, draw_h);
            SDL_GL_SwapWindow(window);
        }
    }
    else
    {
        // Headless: pump a few frames to let imnodes resolve pin positions and
        // link geometry, then read pixels from the FBO and save.
        for (int frame = 0; frame < kWarmupFrames; ++frame)
            render_one_frame(fbo, kImageWidth, kImageHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        if (!save_framebuffer_png(out_path, kImageWidth, kImageHeight))
            return 1;
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();

    if (!interactive)
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &color_tex);
    }
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
