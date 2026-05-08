#include <ExoEngine/Renderer/MeshBuffer.h>

#include <ExoEngine/Core/Logger.h>

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Exo {

namespace {

struct GpuMeshVertex {
    Vec3 position {};
    Vec2 texCoord {};
    Vec3 normal {};
    Vec2 flags {};
};

std::string sourceNameFor(const MeshAsset& mesh) {
    if (!mesh.sourcePath.empty()) {
        const std::filesystem::path filename = mesh.sourcePath.filename();
        return filename.empty() ? mesh.sourcePath.string() : filename.string();
    }

    if (!mesh.name.empty()) {
        return mesh.name;
    }

    return "<unnamed mesh>";
}

bool validateIndices(const MeshAsset& mesh, const std::string& sourceName) {
    const std::size_t vertexCount = mesh.vertices.size();

    for (const std::uint32_t index : mesh.indices) {
        if (static_cast<std::size_t>(index) >= vertexCount) {
            Logger::error("Mesh upload failed for " + sourceName + ": index buffer references a missing vertex");
            return false;
        }
    }

    return true;
}

bool fitsGlSize(std::size_t byteCount) {
    return byteCount <= static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max());
}

} // namespace

MeshBuffer::~MeshBuffer() {
    destroy();
}

MeshBuffer::MeshBuffer(MeshBuffer&& other) noexcept {
    *this = std::move(other);
}

MeshBuffer& MeshBuffer::operator=(MeshBuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();
    vao_ = std::exchange(other.vao_, 0);
    vbo_ = std::exchange(other.vbo_, 0);
    ebo_ = std::exchange(other.ebo_, 0);
    indexCount_ = std::exchange(other.indexCount_, 0);
    bounds_ = std::exchange(other.bounds_, MeshBounds {});
    sourceName_ = std::move(other.sourceName_);
    other.sourceName_.clear();
    return *this;
}

bool MeshBuffer::upload(const MeshAsset& mesh) {
    destroy();

    const std::string sourceName = sourceNameFor(mesh);
    if (mesh.empty()) {
        Logger::warn("Mesh upload skipped for " + sourceName + ": mesh has no renderable geometry");
        return false;
    }

    if (mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        Logger::error("Mesh upload failed for " + sourceName + ": index count does not fit MeshBuffer metadata");
        return false;
    }

    if (mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        Logger::error("Mesh upload failed for " + sourceName + ": index count exceeds OpenGL draw call limits");
        return false;
    }

    if (!validateIndices(mesh, sourceName)) {
        return false;
    }

    std::vector<GpuMeshVertex> vertices;
    vertices.reserve(mesh.vertices.size());

    bool missingNormals = false;
    bool missingTexCoords = false;
    for (const MeshVertex& vertex : mesh.vertices) {
        missingNormals = missingNormals || !vertex.hasNormal;
        missingTexCoords = missingTexCoords || !vertex.hasTexCoord;

        vertices.push_back({
            vertex.position,
            vertex.hasTexCoord ? vertex.texCoord : Vec2 {},
            vertex.hasNormal ? vertex.normal : Vec3 {},
            {
                vertex.hasNormal ? 1.0f : 0.0f,
                vertex.hasTexCoord ? 1.0f : 0.0f,
            },
        });
    }

    const std::size_t vertexBytes = vertices.size() * sizeof(GpuMeshVertex);
    const std::size_t indexBytes = mesh.indices.size() * sizeof(std::uint32_t);
    if (!fitsGlSize(vertexBytes) || !fitsGlSize(indexBytes)) {
        Logger::error("Mesh upload failed for " + sourceName + ": GPU buffer is too large for this OpenGL driver");
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertexBytes),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indexBytes),
        mesh.indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GpuMeshVertex),
        reinterpret_cast<void*>(offsetof(GpuMeshVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GpuMeshVertex),
        reinterpret_cast<void*>(offsetof(GpuMeshVertex, texCoord))
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GpuMeshVertex),
        reinterpret_cast<void*>(offsetof(GpuMeshVertex, normal))
    );

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GpuMeshVertex),
        reinterpret_cast<void*>(offsetof(GpuMeshVertex, flags))
    );

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    indexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
    bounds_ = mesh.bounds;
    sourceName_ = sourceName;

    if (missingNormals) {
        Logger::warn("Mesh uploaded with missing vertex normals: " + sourceName);
    }

    if (missingTexCoords) {
        Logger::warn("Mesh uploaded with missing vertex UVs: " + sourceName);
    }

    Logger::info("Mesh uploaded: " + sourceName);
    return true;
}

void MeshBuffer::destroy() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }

    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }

    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    indexCount_ = 0;
    bounds_ = {};
    sourceName_.clear();
}

void MeshBuffer::draw() const {
    if (!valid()) {
        return;
    }

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void MeshBuffer::drawRange(std::uint32_t indexOffset, std::uint32_t indexCount) const {
    if (!valid() || indexCount == 0 || indexOffset >= indexCount_) {
        return;
    }

    const std::uint32_t clampedCount = std::min(indexCount, indexCount_ - indexOffset);
    const auto byteOffset = static_cast<std::uintptr_t>(indexOffset) * sizeof(std::uint32_t);

    glBindVertexArray(vao_);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(clampedCount),
        GL_UNSIGNED_INT,
        reinterpret_cast<void*>(byteOffset)
    );
    glBindVertexArray(0);
}

bool MeshBuffer::valid() const {
    return vao_ != 0 && vbo_ != 0 && ebo_ != 0 && indexCount_ != 0;
}

std::uint32_t MeshBuffer::indexCount() const {
    return indexCount_;
}

const MeshBounds& MeshBuffer::bounds() const {
    return bounds_;
}

const std::string& MeshBuffer::sourceName() const {
    return sourceName_;
}

} // namespace Exo
