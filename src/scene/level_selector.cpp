/*
 * LevelSelector - Implementation
 */
#include "level_selector.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

void LevelSelector::ScanLevels(const std::string& levelsBasePath) {
    m_levels.clear();
    
    // Try to find levels directory
    fs::path basePath(levelsBasePath);
    
    // Also check relative to PROJECT_SOURCE_DIR if defined
#ifdef PROJECT_SOURCE_DIR
    if (!fs::exists(basePath)) {
        basePath = fs::path(PROJECT_SOURCE_DIR) / levelsBasePath;
    }
#endif
    
    if (fs::exists(basePath) && fs::is_directory(basePath)) {
        // Scan for directories containing level.json
        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (entry.is_directory()) {
                fs::path levelJsonPath = entry.path() / "level.json";
                if (fs::exists(levelJsonPath)) {
                    LevelInfo info;
                    info.path = levelJsonPath.string();
                    info.name = entry.path().filename().string();
                    
                    // Try to read name and description from JSON
                    try {
                        std::ifstream file(levelJsonPath);
                        if (file.is_open()) {
                            nlohmann::json j;
                            file >> j;
                            
                            if (j.contains("name") && j["name"].is_string()) {
                                info.name = j["name"].get<std::string>();
                            }
                            if (j.contains("description") && j["description"].is_string()) {
                                info.description = j["description"].get<std::string>();
                            }
                        }
                    } catch (...) {
                        // Use directory name as fallback
                    }
                    
                    m_levels.push_back(std::move(info));
                }
            }
        }
    }
    
    // Sort levels alphabetically by name
    std::sort(m_levels.begin(), m_levels.end(), [](const LevelInfo& a, const LevelInfo& b) {
        return a.name < b.name;
    });
    
    // Add special levels (stress tests)
    AddSpecialLevels();
    
    // Select first level if available
    if (!m_levels.empty() && m_selectedIndex < 0) {
        m_selectedIndex = 0;
    }
}

void LevelSelector::AddSpecialLevels() {
    // Separator
    LevelInfo separator;
    separator.name = "--- Stress Tests ---";
    separator.isSpecial = true;
    separator.specialId = 0;  // Not loadable
    m_levels.push_back(separator);
    
    // Stress test presets
    const char* presets[] = {
        "Stress: Light (~1.3K)",
        "Stress: Medium (~12K)",
        "Stress: Heavy (~58K)",
        "Stress: Extreme (~117K)"
    };
    const char* descriptions[] = {
        "1,000 static + 100 semi + 50 dynamic + 200 procedural",
        "10,000 static + 500 semi + 200 dynamic + 1,000 procedural",
        "50,000 static + 2,000 semi + 1,000 dynamic + 5,000 procedural",
        "100,000 static + 5,000 semi + 2,000 dynamic + 10,000 procedural"
    };
    
    for (int i = 0; i < 4; ++i) {
        LevelInfo stress;
        stress.name = presets[i];
        stress.description = descriptions[i];
        stress.isSpecial = true;
        stress.specialId = i + 1;  // 1-4 for stress test presets
        m_levels.push_back(stress);
    }
    
    // Custom stress test with sliders (specialId = 5)
    LevelInfo custom;
    custom.name = "Stress: Custom";
    custom.description = "Configure each tier with sliders";
    custom.isSpecial = true;
    custom.specialId = 5;
    m_levels.push_back(custom);
}

void LevelSelector::SetSelectedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(m_levels.size())) {
        // Don't allow selecting separators
        if (m_levels[index].isSpecial && m_levels[index].specialId == 0) {
            return;
        }
        m_selectedIndex = index;
    }
}

const LevelInfo* LevelSelector::GetSelectedLevel() const {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_levels.size())) {
        return &m_levels[m_selectedIndex];
    }
    return nullptr;
}

bool LevelSelector::ConsumeLoadRequest() {
    bool requested = m_loadRequested;
    m_loadRequested = false;
    return requested;
}
