#pragma once

#include "object.h"
#include <memory>
#include <string>
#include <vector>

/**
 * Scene: container for objects (and later lights, cameras). Cleared on unload so refs drop and managers can trim.
 */
class Scene {
public:
    Scene() = default;
    explicit Scene(std::string name) : m_name(std::move(name)) {}

    const std::string& GetName() const { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    std::vector<Object>& GetObjects() { return m_objects; }
    const std::vector<Object>& GetObjects() const { return m_objects; }

    /** Drop all refs; managers can TrimUnused after this. */
    void Clear() {
        m_objects.clear();
    }

    /** Fill push data for all objects (viewProj * localTransform, color, objectIndex). Call each frame before building draw list. */
    void FillPushDataForAllObjects(const float* pViewProj_ic) {
        if (pViewProj_ic == nullptr)
            return;
        for (size_t i = 0; i < m_objects.size(); ++i)
            ObjectFillPushData(m_objects[i], pViewProj_ic, static_cast<uint32_t>(i));
    }

private:
    std::string m_name;
    std::vector<Object> m_objects;
};
