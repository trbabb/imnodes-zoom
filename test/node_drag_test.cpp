// Automated drag test for imnodes, driven by imgui_test_engine.
//
// Renders the shared 3-node graph (see test_common.h) headlessly, then
// uses the test engine to synthesize a mouse drag on node "source A" and
// verifies that the node's editor-grid position changed accordingly.
//
// Outputs two PNGs:
//   node_drag_before.png — graph just after layout settles, before the drag
//   node_drag_after.png  — graph after the test engine releases the mouse
//
// Exit status: 0 if the queued test reported success, 1 otherwise. Intended
// as a starting point for richer imnodes regression tests.

#define IMGUI_DEFINE_MATH_OPERATORS

#include <imnodes.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL2/SDL.h>
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

#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

#include "test_common.h"

#include <cstdio>
#include <cstdlib>

using imnodes_test::build_node_graph;
using imnodes_test::ids::NodeSourceA;
using imnodes_test::kImageHeight;
using imnodes_test::kImageWidth;
using imnodes_test::save_framebuffer_png;
using imnodes_test::seed_initial_node_positions;

namespace
{

// Drag delta the test will apply to "source A", in editor screen pixels. We
// verify that the node's editor-grid position increased by approximately this
// amount after the drag. Choosing both axes nonzero so a buggy axis-locked
// drag would still be caught.
constexpr float kDragDx = 200.f;
constexpr float kDragDy = 120.f;

// Minimum movement we require to consider the drag "worked". We use a slack
// margin below the requested delta to tolerate sub-pixel rounding or any
// small offset between the title-bar grab point and the recorded node origin.
constexpr float kMinDx = 100.f;
constexpr float kMinDy = 50.f;

// Render one frame of the shared graph into the bound FBO.
void render_frame(GLuint fbo, const ImVec4& clear)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    build_node_graph();
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, kImageWidth, kImageHeight);
    glClearColor(clear.x, clear.y, clear.z, clear.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace

int main(int /*argc*/, char** /*argv*/)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "imnodes_node_drag_test",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        kImageWidth,
        kImageHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(0);

    // Offscreen target for headless rendering and PNG capture.
    GLuint fbo = 0, color_tex = 0;
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)kImageWidth, (float)kImageHeight);
    io.DeltaTime   = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    // The SDL2 backend would normally manage cursor state from the event
    // loop; we don't run an event loop, so disable cursor-shape feedback.
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    seed_initial_node_positions();

    // -------------------------------------------------------------------------
    // Test engine setup
    // -------------------------------------------------------------------------
    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& te_io  = ImGuiTestEngine_GetIO(engine);
    // Fast: drive synthetic inputs as fast as possible. Cinematic is useful
    // for visual demos but we don't need it here.
    te_io.ConfigRunSpeed              = ImGuiTestRunSpeed_Fast;
    te_io.ConfigVerboseLevel          = ImGuiTestVerboseLevel_Info;
    te_io.ConfigVerboseLevelOnError   = ImGuiTestVerboseLevel_Debug;
    te_io.ConfigNoThrottle            = true;

    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());

    // Register a single test that drags "source A" by a known offset and
    // checks the node's editor-grid position moved by approximately that
    // amount. The host application provides the GUI each frame, so this
    // test has no GuiFunc — only a TestFunc.
    struct DragResult
    {
        bool   ran             = false;
        ImVec2 start_grid_pos  = ImVec2(0, 0);
        ImVec2 end_grid_pos    = ImVec2(0, 0);
    };
    DragResult drag_result;

    ImGuiTest* test = IM_REGISTER_TEST(engine, "imnodes", "drag_source_a");
    test->TestFunc  = [&drag_result](ImGuiTestContext* ctx) {
        // SetRef picks the active imgui window for subsequent operations and
        // also sets the active viewport for raw-position mouse ops, which is
        // a documented prerequisite for MouseTeleportToPos / MouseMoveToPos.
        ctx->SetRef("imnodes_test");
        ctx->Yield(2);

        const ImVec2 start_grid   = ImNodes::GetNodeGridSpacePos(NodeSourceA);
        const ImVec2 start_screen = ImNodes::GetNodeScreenSpacePos(NodeSourceA);
        // Grab a point inside the title bar: a few pixels right and down
        // from the node's top-left corner so we hit the draggable strip and
        // not a pin or empty body region.
        const ImVec2 grab(start_screen.x + 30.f, start_screen.y + 8.f);
        const ImVec2 target(grab.x + kDragDx, grab.y + kDragDy);

        ctx->MouseTeleportToPos(grab);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(target);
        ctx->MouseUp(0);
        ctx->Yield();

        const ImVec2 end_grid = ImNodes::GetNodeGridSpacePos(NodeSourceA);

        drag_result.ran            = true;
        drag_result.start_grid_pos = start_grid;
        drag_result.end_grid_pos   = end_grid;

        IM_CHECK_GT(end_grid.x - start_grid.x, kMinDx);
        IM_CHECK_GT(end_grid.y - start_grid.y, kMinDy);
    };

    const ImVec4 clear(0.45f, 0.55f, 0.60f, 1.0f);

    // Warm up so imnodes has resolved node sizes and pin positions before
    // we both capture "before" and start the test.
    for (int i = 0; i < 4; ++i)
    {
        render_frame(fbo, clear);
        ImGuiTestEngine_PostSwap(engine);
    }
    save_framebuffer_png("node_drag_before.png", kImageWidth, kImageHeight);

    ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_None);

    // Drive frames until the test queue drains. Cap with a safety bound in
    // case the engine wedges; at fast speed, this test completes in well
    // under 100 frames in practice.
    constexpr int kMaxFrames = 1000;
    int frames = 0;
    while (!ImGuiTestEngine_IsTestQueueEmpty(engine) && frames < kMaxFrames)
    {
        render_frame(fbo, clear);
        ImGuiTestEngine_PostSwap(engine);
        ++frames;
    }
    // One more frame so the post-drag state is fully rendered before capture.
    render_frame(fbo, clear);
    ImGuiTestEngine_PostSwap(engine);

    save_framebuffer_png("node_drag_after.png", kImageWidth, kImageHeight);

    ImGuiTestEngineResultSummary summary;
    ImGuiTestEngine_GetResultSummary(engine, &summary);
    std::printf(
        "test engine: %d tested, %d ok, %d failed (drag ran=%d  "
        "start=(%.1f,%.1f)  end=(%.1f,%.1f))\n",
        summary.CountTested,
        summary.CountSuccess,
        summary.CountTested - summary.CountSuccess,
        drag_result.ran ? 1 : 0,
        drag_result.start_grid_pos.x,
        drag_result.start_grid_pos.y,
        drag_result.end_grid_pos.x,
        drag_result.end_grid_pos.y);

    const bool ok = (summary.CountTested > 0)
                 && (summary.CountTested == summary.CountSuccess);

    // Teardown order matters: imgui ctx must be destroyed before the test
    // engine ctx (so .ini data persists), per the test engine docs.
    ImGuiTestEngine_Stop(engine);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &color_tex);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return ok ? 0 : 1;
}
