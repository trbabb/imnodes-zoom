// Shared helpers for the imnodes test programs (render_test, node_drag_test).
//
// These are inline definitions in a header rather than a separate translation
// unit because there are only two consumers, the bodies are short, and keeping
// them inline avoids another CMake target. Both consumers must already have
// included <imgui.h>, <imnodes.h>, an OpenGL header that provides
// GL_PACK_ALIGNMENT and glReadPixels, and <png.h> — this header does not
// re-include them so it can stay agnostic of the platform-specific GL include.

#pragma once

#include <cstdio>
#include <vector>

namespace imnodes_test
{

// Target image dimensions used by both headless render paths. Kept modest so
// PNGs are quick to write and easy to eyeball in a terminal image viewer.
inline constexpr int kImageWidth  = 1024;
inline constexpr int kImageHeight = 640;

// Node + link IDs for the shared 3-node graph. Centralized so tests can
// reference nodes by symbolic name.
namespace ids
{
inline constexpr int NodeSourceA = 1;
inline constexpr int NodeSourceB = 2;
inline constexpr int NodeSink    = 3;

inline constexpr int PinSourceAOut = 11;
inline constexpr int PinSourceBOut = 21;
inline constexpr int PinSinkA      = 31;
inline constexpr int PinSinkB      = 32;
inline constexpr int PinSinkResult = 33;

inline constexpr int LinkAtoSink = 100;
inline constexpr int LinkBtoSink = 101;
} // namespace ids

// Build the shared node graph: two source nodes feeding one sink. Sized to
// fill the current display so it tracks live window resizes (interactive
// render_test) and the FBO size (headless paths).
inline void build_node_graph()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin(
        "imnodes_test",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    ImNodes::BeginNodeEditor();

    ImNodes::BeginNode(ids::NodeSourceA);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted("source A");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginOutputAttribute(ids::PinSourceAOut);
    ImGui::Indent(60);
    ImGui::Text("out");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    ImNodes::BeginNode(ids::NodeSourceB);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted("source B");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginOutputAttribute(ids::PinSourceBOut);
    ImGui::Indent(60);
    ImGui::Text("out");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    ImNodes::BeginNode(ids::NodeSink);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted("sink");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginInputAttribute(ids::PinSinkA);
    ImGui::Text("a");
    ImNodes::EndInputAttribute();
    ImNodes::BeginInputAttribute(ids::PinSinkB);
    ImGui::Text("b");
    ImNodes::EndInputAttribute();
    ImNodes::BeginOutputAttribute(ids::PinSinkResult);
    ImGui::Indent(60);
    ImGui::Text("result");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    ImNodes::Link(ids::LinkAtoSink, ids::PinSourceAOut, ids::PinSinkA);
    ImNodes::Link(ids::LinkBtoSink, ids::PinSourceBOut, ids::PinSinkB);

    ImNodes::EndNodeEditor();
    ImGui::End();
}

// Apply the initial node positions used by every test program. Must be called
// after ImNodes::CreateContext() so the imnodes context exists.
inline void seed_initial_node_positions()
{
    ImNodes::SetNodeGridSpacePos(ids::NodeSourceA, ImVec2(60.f, 100.f));
    ImNodes::SetNodeGridSpacePos(ids::NodeSourceB, ImVec2(60.f, 260.f));
    ImNodes::SetNodeGridSpacePos(ids::NodeSink, ImVec2(480.f, 180.f));
}

// Write an RGBA8 buffer to a PNG file using libpng. Returns true on success.
// `pixels` is in OpenGL's bottom-up row order; PNG expects top-down, so we
// hand libpng row pointers in reverse order rather than copying the buffer.
inline bool write_png_rgba(
    const char* path, const unsigned char* pixels, int width, int height)
{
    FILE* fp = std::fopen(path, "wb");
    if (!fp)
    {
        std::fprintf(stderr, "fopen('%s') failed\n", path);
        return false;
    }
    png_structp png =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png)
    {
        std::fclose(fp);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(fp);
        return false;
    }
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        std::fclose(fp);
        return false;
    }
    png_init_io(png, fp);
    png_set_IHDR(
        png,
        info,
        (png_uint_32)width,
        (png_uint_32)height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    std::vector<png_bytep> rows((size_t)height);
    const size_t stride = (size_t)width * 4;
    for (int y = 0; y < height; ++y)
    {
        rows[(size_t)y] =
            const_cast<png_bytep>(pixels + (size_t)(height - 1 - y) * stride);
    }
    png_set_rows(png, info, rows.data());
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
    return true;
}

// Convenience: read pixels from the currently bound framebuffer and write
// them as a PNG. Returns true on success.
inline bool save_framebuffer_png(const char* path, int width, int height)
{
    std::vector<unsigned char> pixels((size_t)width * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (!write_png_rgba(path, pixels.data(), width, height))
    {
        std::fprintf(stderr, "write_png failed: %s\n", path);
        return false;
    }
    std::printf("wrote %s (%dx%d)\n", path, width, height);
    return true;
}

} // namespace imnodes_test
