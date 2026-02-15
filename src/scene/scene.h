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

    /** Fill push data for all objects (viewProj * localTransform, color). Call each frame before building draw list. */
    void FillPushDataForAllObjects(const float* viewProj) {
        if (!viewProj) return;
        for (auto& obj : m_objects)
            ObjectFillPushData(obj, viewProj);
    }

private:
    std::string m_name;
    std::vector<Object> m_objects;
};
