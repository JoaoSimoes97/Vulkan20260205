#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * General drawable object for debugging and future scene/render list.
 * Holds pipeline key, shape tag, arbitrary push data, and draw params.
 * Will be extended (mesh id, material id, descriptor sets, etc.) per docs.
 */
enum class Shape {
    Triangle,
    Circle,
    Rectangle,
    Cube,
};

struct Object {
    std::string              pipelineKey   = "main";
    Shape                    shape         = Shape::Triangle;
    /** Local transform (column-major mat4). Used with projection to fill pushData each frame. */
    alignas(16) float        localTransform[16] = {
        1.f, 0.f, 0.f, 0.f,  0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,  0.f, 0.f, 0.f, 1.f
    };
    /** Per-object color (RGBA). Passed to fragment shader via push constants. */
    float                    color[4]      = { 1.f, 1.f, 1.f, 1.f };
    /** Arbitrary data pushed to the GPU (e.g. mat4 + color). Filled each frame from projection * localTransform + color. */
    std::vector<uint8_t>     pushData;
    uint32_t                 pushDataSize  = 0u;
    uint32_t                 vertexCount   = 3u;
    uint32_t                 instanceCount = 1u;
    uint32_t                 firstVertex   = 0u;
    uint32_t                 firstInstance = 0u;
};

/** Identity matrix in column-major order (16 floats). */
inline void ObjectSetIdentity(float* out16) {
    for (int i = 0; i < 16; ++i) out16[i] = (i % 5 == 0) ? 1.f : 0.f;
}

/** Translation (tx, ty, tz) into column-major mat4. */
inline void ObjectSetTranslation(float* out16, float tx, float ty, float tz) {
    ObjectSetIdentity(out16);
    out16[12] = tx;
    out16[13] = ty;
    out16[14] = tz;
}

/** Column-major mat4 multiply: out = proj * model. out16 must have 16 floats. */
inline void ObjectMat4Multiply(float* out16, const float* proj16, const float* model16) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float v = 0.f;
            for (int k = 0; k < 4; ++k)
                v += proj16[row + k * 4] * model16[k + col * 4];
            out16[row + col * 4] = v;
        }
    }
}

/** Push layout: mat4 (64) + vec4 color (16) = 80 bytes. */
constexpr uint32_t kObjectPushConstantSize = 80u;

inline Object MakeTriangle() {
    Object o;
    o.shape = Shape::Triangle;
    ObjectSetIdentity(o.localTransform);
    o.color[0] = 1.f; o.color[1] = 0.f; o.color[2] = 0.f; o.color[3] = 1.f;  /* red */
    o.pushData.resize(kObjectPushConstantSize);
    o.pushDataSize = kObjectPushConstantSize;
    o.vertexCount = 3u;
    return o;
}

inline Object MakeCircle() {
    Object o;
    o.shape = Shape::Circle;
    ObjectSetTranslation(o.localTransform, -0.4f, 0.f, 0.f);
    o.color[0] = 0.f; o.color[1] = 1.f; o.color[2] = 0.f; o.color[3] = 1.f;  /* green */
    o.pushData.resize(kObjectPushConstantSize);
    o.pushDataSize = kObjectPushConstantSize;
    o.vertexCount = 3u;  /* placeholder until mesh */
    return o;
}

inline Object MakeRectangle() {
    Object o;
    o.shape = Shape::Rectangle;
    ObjectSetTranslation(o.localTransform, 0.4f, 0.f, 0.f);
    o.color[0] = 0.f; o.color[1] = 0.f; o.color[2] = 1.f; o.color[3] = 1.f;  /* blue */
    o.pushData.resize(kObjectPushConstantSize);
    o.pushDataSize = kObjectPushConstantSize;
    o.vertexCount = 3u;  /* placeholder until mesh */
    return o;
}

inline Object MakeCube() {
    Object o;
    o.shape = Shape::Cube;
    ObjectSetTranslation(o.localTransform, 0.f, 0.3f, 0.f);
    o.color[0] = 1.f; o.color[1] = 1.f; o.color[2] = 0.f; o.color[3] = 1.f;  /* yellow */
    o.pushData.resize(kObjectPushConstantSize);
    o.pushDataSize = kObjectPushConstantSize;
    o.vertexCount = 3u;  /* placeholder until mesh */
    return o;
}
