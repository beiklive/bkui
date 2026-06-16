#include <bkui/backend/opengl/OpenGLDevice.hpp>

#include <SDL3/SDL_opengl.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bk
{
namespace
{
using GLcharPtr = const GLchar*;

struct GLFunctions
{
    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC DeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC GenBuffers = nullptr;
    PFNGLBINDBUFFERPROC BindBuffer = nullptr;
    PFNGLBUFFERDATAPROC BufferData = nullptr;
    PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
};

struct GLBuffer
{
    GLuint id = 0;
    GLenum target = GL_ARRAY_BUFFER;
};

struct GLShader
{
    GLuint id = 0;
};

struct GLTexture
{
    GLuint id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct GLPipeline
{
    GLuint program = 0;
    GLuint vao = 0;
    VertexLayoutDesc vertexLayout;
    std::vector<VertexAttributeDesc> attributes;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

struct GLCommandBuffer
{
    PipelineHandle pipeline;
    BufferHandle vertexBuffer;
    TextureHandle texture;
    std::uint32_t vertexCount = 0;
    std::uint32_t firstVertex = 0;
    bool recording = false;
};

template <typename T>
bool LoadProc(Platform& platform, T& target, const char* name)
{
    target = reinterpret_cast<T>(platform.GetOpenGLProcAddress(name));
    if (target == nullptr)
    {
        std::fprintf(stderr, "OpenGL procedure not found: %s\n", name);
        return false;
    }
    return true;
}

bool LoadGL(Platform& platform, GLFunctions& gl)
{
    return LoadProc(platform, gl.CreateShader, "glCreateShader") &&
        LoadProc(platform, gl.ShaderSource, "glShaderSource") &&
        LoadProc(platform, gl.CompileShader, "glCompileShader") &&
        LoadProc(platform, gl.GetShaderiv, "glGetShaderiv") &&
        LoadProc(platform, gl.GetShaderInfoLog, "glGetShaderInfoLog") &&
        LoadProc(platform, gl.DeleteShader, "glDeleteShader") &&
        LoadProc(platform, gl.CreateProgram, "glCreateProgram") &&
        LoadProc(platform, gl.AttachShader, "glAttachShader") &&
        LoadProc(platform, gl.LinkProgram, "glLinkProgram") &&
        LoadProc(platform, gl.GetProgramiv, "glGetProgramiv") &&
        LoadProc(platform, gl.GetProgramInfoLog, "glGetProgramInfoLog") &&
        LoadProc(platform, gl.DeleteProgram, "glDeleteProgram") &&
        LoadProc(platform, gl.UseProgram, "glUseProgram") &&
        LoadProc(platform, gl.GenVertexArrays, "glGenVertexArrays") &&
        LoadProc(platform, gl.BindVertexArray, "glBindVertexArray") &&
        LoadProc(platform, gl.DeleteVertexArrays, "glDeleteVertexArrays") &&
        LoadProc(platform, gl.GenBuffers, "glGenBuffers") &&
        LoadProc(platform, gl.BindBuffer, "glBindBuffer") &&
        LoadProc(platform, gl.BufferData, "glBufferData") &&
        LoadProc(platform, gl.DeleteBuffers, "glDeleteBuffers") &&
        LoadProc(platform, gl.GetUniformLocation, "glGetUniformLocation") &&
        LoadProc(platform, gl.Uniform1i, "glUniform1i") &&
        LoadProc(platform, gl.ActiveTexture, "glActiveTexture") &&
        LoadProc(platform, gl.EnableVertexAttribArray, "glEnableVertexAttribArray") &&
        LoadProc(platform, gl.VertexAttribPointer, "glVertexAttribPointer");
}

GLenum ToGLBufferTarget(BufferUsage usage)
{
    switch (usage)
    {
    case BufferUsage::Vertex:
        return GL_ARRAY_BUFFER;
    case BufferUsage::Index:
        return GL_ELEMENT_ARRAY_BUFFER;
    case BufferUsage::Uniform:
        return GL_UNIFORM_BUFFER;
    }

    return GL_ARRAY_BUFFER;
}

GLenum ToGLShaderStage(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    }

    return GL_VERTEX_SHADER;
}

GLint VertexFormatComponents(VertexFormat format)
{
    switch (format)
    {
    case VertexFormat::Float2:
        return 2;
    case VertexFormat::Float3:
        return 3;
    case VertexFormat::Float4:
        return 4;
    }

    return 2;
}

GLenum ToGLTopology(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    }

    return GL_TRIANGLES;
}

template <typename T>
T* Lookup(std::vector<T>& items, std::uint32_t id)
{
    if (id == 0 || id > items.size())
    {
        return nullptr;
    }

    T& item = items[id - 1];
    return item.id == 0 ? nullptr : &item;
}

GLShader* LookupShader(std::vector<GLShader>& items, std::uint32_t id)
{
    if (id == 0 || id > items.size())
    {
        return nullptr;
    }

    GLShader& item = items[id - 1];
    return item.id == 0 ? nullptr : &item;
}

GLPipeline* LookupPipeline(std::vector<GLPipeline>& items, std::uint32_t id)
{
    if (id == 0 || id > items.size())
    {
        return nullptr;
    }

    GLPipeline& item = items[id - 1];
    return item.program == 0 ? nullptr : &item;
}

GLCommandBuffer* LookupCommandBuffer(std::vector<GLCommandBuffer>& items, std::uint32_t id)
{
    if (id == 0 || id > items.size())
    {
        return nullptr;
    }

    return &items[id - 1];
}

GLuint CompileShader(const GLFunctions& gl, const ShaderDesc& desc)
{
    const GLuint shader = gl.CreateShader(ToGLShaderStage(desc.stage));
    const GLcharPtr sources[] = {desc.source.c_str()};
    gl.ShaderSource(shader, 1, sources, nullptr);
    gl.CompileShader(shader);

    GLint ok = GL_FALSE;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        std::array<GLchar, 1024> log{};
        gl.GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Shader compile failed: %s\n", log.data());
        gl.DeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint LinkProgram(const GLFunctions& gl, GLuint vertexShader, GLuint fragmentShader)
{
    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vertexShader);
    gl.AttachShader(program, fragmentShader);
    gl.LinkProgram(program);

    GLint ok = GL_FALSE;
    gl.GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        std::array<GLchar, 1024> log{};
        gl.GetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Program link failed: %s\n", log.data());
        gl.DeleteProgram(program);
        return 0;
    }

    return program;
}

class OpenGLDevice final : public Device
{
public:
    explicit OpenGLDevice(Platform& platform)
        : platform_(platform)
    {
    }

    bool Init() override
    {
        if (!platform_.CreateOpenGLContext(OpenGLContextDesc{}))
        {
            return false;
        }

        platform_.MakeOpenGLContextCurrent();
        if (!LoadGL(platform_, gl_))
        {
            return false;
        }

        Resize(platform_.GetWindowSize());
        return true;
    }

    void Shutdown() override
    {
        for (std::uint32_t i = 1; i <= pipelines_.size(); ++i)
        {
            DestroyPipeline(PipelineHandle{i});
        }

        for (std::uint32_t i = 1; i <= shaders_.size(); ++i)
        {
            DestroyShader(ShaderHandle{i});
        }

        for (std::uint32_t i = 1; i <= buffers_.size(); ++i)
        {
            DestroyBuffer(BufferHandle{i});
        }

        for (std::uint32_t i = 1; i <= textures_.size(); ++i)
        {
            DestroyTexture(TextureHandle{i});
        }

        commandBuffers_.clear();
        platform_.DestroyOpenGLContext();
    }

    void Resize(Vector2 size) override
    {
        if (size.x > 0.0F && size.y > 0.0F)
        {
            glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
        }
    }

    SwapchainHandle GetMainSwapchain() const override
    {
        return SwapchainHandle{1};
    }

    BufferHandle CreateBuffer(const BufferDesc& desc) override
    {
        if (desc.size == 0)
        {
            return {};
        }

        GLBuffer buffer;
        buffer.target = ToGLBufferTarget(desc.usage);
        gl_.GenBuffers(1, &buffer.id);
        gl_.BindBuffer(buffer.target, buffer.id);
        gl_.BufferData(buffer.target, static_cast<GLsizeiptr>(desc.size), desc.data, GL_STATIC_DRAW);
        gl_.BindBuffer(buffer.target, 0);

        buffers_.push_back(buffer);
        return BufferHandle{static_cast<std::uint32_t>(buffers_.size())};
    }

    TextureHandle CreateTexture(const TextureDesc& desc) override
    {
        if (desc.width == 0 || desc.height == 0 || desc.rgba == nullptr)
        {
            return {};
        }

        GLTexture texture;
        texture.width = desc.width;
        texture.height = desc.height;
        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            desc.rgba
        );
        glBindTexture(GL_TEXTURE_2D, 0);

        textures_.push_back(texture);
        return TextureHandle{static_cast<std::uint32_t>(textures_.size())};
    }

    ShaderHandle CreateShader(const ShaderDesc& desc) override
    {
        if (desc.source.empty())
        {
            return {};
        }

        GLShader shader;
        shader.id = CompileShader(gl_, desc);
        if (shader.id == 0)
        {
            return {};
        }

        shaders_.push_back(shader);
        return ShaderHandle{static_cast<std::uint32_t>(shaders_.size())};
    }

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override
    {
        GLShader* vertexShader = LookupShader(shaders_, desc.vertexShader.id);
        GLShader* fragmentShader = LookupShader(shaders_, desc.fragmentShader.id);
        if (vertexShader == nullptr || fragmentShader == nullptr || desc.vertexLayout.stride == 0)
        {
            return {};
        }

        GLPipeline pipeline;
        pipeline.program = LinkProgram(gl_, vertexShader->id, fragmentShader->id);
        if (pipeline.program == 0)
        {
            return {};
        }

        pipeline.topology = desc.topology;
        pipeline.attributes.assign(
            desc.vertexLayout.attributes,
            desc.vertexLayout.attributes + desc.vertexLayout.attributeCount
        );
        pipeline.vertexLayout = desc.vertexLayout;
        pipeline.vertexLayout.attributes = pipeline.attributes.data();

        gl_.GenVertexArrays(1, &pipeline.vao);

        pipelines_.push_back(std::move(pipeline));
        return PipelineHandle{static_cast<std::uint32_t>(pipelines_.size())};
    }

    CommandBufferHandle CreateCommandBuffer() override
    {
        commandBuffers_.push_back(GLCommandBuffer{});
        return CommandBufferHandle{static_cast<std::uint32_t>(commandBuffers_.size())};
    }

    void DestroyBuffer(BufferHandle handle) override
    {
        GLBuffer* buffer = Lookup(buffers_, handle.id);
        if (buffer == nullptr)
        {
            return;
        }

        gl_.DeleteBuffers(1, &buffer->id);
        buffer->id = 0;
    }

    void DestroyTexture(TextureHandle handle) override
    {
        if (handle.id == 0 || handle.id > textures_.size())
        {
            return;
        }

        GLTexture& texture = textures_[handle.id - 1];
        if (texture.id != 0)
        {
            glDeleteTextures(1, &texture.id);
            texture.id = 0;
        }
    }

    void DestroyShader(ShaderHandle handle) override
    {
        GLShader* shader = LookupShader(shaders_, handle.id);
        if (shader == nullptr)
        {
            return;
        }

        gl_.DeleteShader(shader->id);
        shader->id = 0;
    }

    void DestroyPipeline(PipelineHandle handle) override
    {
        GLPipeline* pipeline = LookupPipeline(pipelines_, handle.id);
        if (pipeline == nullptr)
        {
            return;
        }

        if (pipeline->vao != 0)
        {
            gl_.DeleteVertexArrays(1, &pipeline->vao);
            pipeline->vao = 0;
        }

        if (pipeline->program != 0)
        {
            gl_.DeleteProgram(pipeline->program);
            pipeline->program = 0;
        }
    }

    void DestroyCommandBuffer(CommandBufferHandle handle) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, handle.id);
        if (commandBuffer != nullptr)
        {
            *commandBuffer = {};
        }
    }

    void BeginFrame(SwapchainHandle swapchain, const RenderPassDesc& renderPass) override
    {
        if (!IsValid(swapchain))
        {
            return;
        }

        Resize(platform_.GetWindowSize());
        glClearColor(renderPass.clearColor.r, renderPass.clearColor.g, renderPass.clearColor.b, renderPass.clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void BeginCommandBuffer(CommandBufferHandle commandBufferHandle) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer == nullptr)
        {
            return;
        }

        commandBuffer->pipeline = {};
        commandBuffer->vertexBuffer = {};
        commandBuffer->texture = {};
        commandBuffer->vertexCount = 0;
        commandBuffer->firstVertex = 0;
        commandBuffer->recording = true;
    }

    void BindPipeline(CommandBufferHandle commandBufferHandle, PipelineHandle pipeline) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer != nullptr && commandBuffer->recording)
        {
            commandBuffer->pipeline = pipeline;
        }
    }

    void BindVertexBuffer(CommandBufferHandle commandBufferHandle, BufferHandle buffer) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer != nullptr && commandBuffer->recording)
        {
            commandBuffer->vertexBuffer = buffer;
        }
    }

    void BindTexture(CommandBufferHandle commandBufferHandle, TextureHandle texture) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer != nullptr && commandBuffer->recording)
        {
            commandBuffer->texture = texture;
        }
    }

    void Draw(CommandBufferHandle commandBufferHandle, std::uint32_t vertexCount, std::uint32_t firstVertex) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer != nullptr && commandBuffer->recording)
        {
            commandBuffer->vertexCount = vertexCount;
            commandBuffer->firstVertex = firstVertex;
        }
    }

    void EndCommandBuffer(CommandBufferHandle commandBufferHandle) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer != nullptr)
        {
            commandBuffer->recording = false;
        }
    }

    void Submit(CommandBufferHandle commandBufferHandle) override
    {
        GLCommandBuffer* commandBuffer = LookupCommandBuffer(commandBuffers_, commandBufferHandle.id);
        if (commandBuffer == nullptr || commandBuffer->vertexCount == 0)
        {
            return;
        }

        GLPipeline* pipeline = LookupPipeline(pipelines_, commandBuffer->pipeline.id);
        GLBuffer* vertexBuffer = Lookup(buffers_, commandBuffer->vertexBuffer.id);
        if (pipeline == nullptr || vertexBuffer == nullptr)
        {
            return;
        }

        gl_.UseProgram(pipeline->program);
        if (commandBuffer->texture.id != 0 && commandBuffer->texture.id <= textures_.size())
        {
            GLTexture& texture = textures_[commandBuffer->texture.id - 1];
            if (texture.id != 0)
            {
                gl_.ActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture.id);
                const GLint location = gl_.GetUniformLocation(pipeline->program, "uTexture");
                if (location >= 0)
                {
                    gl_.Uniform1i(location, 0);
                }
            }
        }
        gl_.BindVertexArray(pipeline->vao);
        gl_.BindBuffer(GL_ARRAY_BUFFER, vertexBuffer->id);

        for (const VertexAttributeDesc& attribute : pipeline->attributes)
        {
            gl_.EnableVertexAttribArray(attribute.location);
            gl_.VertexAttribPointer(
                attribute.location,
                VertexFormatComponents(attribute.format),
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(pipeline->vertexLayout.stride),
                reinterpret_cast<void*>(attribute.offset)
            );
        }

        glDrawArrays(
            ToGLTopology(pipeline->topology),
            static_cast<GLint>(commandBuffer->firstVertex),
            static_cast<GLsizei>(commandBuffer->vertexCount)
        );

        gl_.BindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        gl_.BindVertexArray(0);
        gl_.UseProgram(0);
    }

    void EndFrame(SwapchainHandle swapchain) override
    {
        if (IsValid(swapchain))
        {
            platform_.SwapOpenGLBuffers();
        }
    }

    const char* BackendName() const override
    {
        return "OpenGL";
    }

private:
    Platform& platform_;
    GLFunctions gl_;
    std::vector<GLBuffer> buffers_;
    std::vector<GLTexture> textures_;
    std::vector<GLShader> shaders_;
    std::vector<GLPipeline> pipelines_;
    std::vector<GLCommandBuffer> commandBuffers_;
};
}

std::unique_ptr<Device> CreateOpenGLDevice(Platform& platform)
{
    return std::make_unique<OpenGLDevice>(platform);
}

}
