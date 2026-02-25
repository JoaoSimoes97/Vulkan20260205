/*
 * LevelSelector - Discovers and manages level selection.
 * 
 * Scans the levels folder for available levels (directories containing level.json)
 * and provides a UI for selection. Also includes special entries like stress tests.
 */
#pragma once

#include "stress_test_generator.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * Information about a discoverable level.
 */
struct LevelInfo {
    std::string name;           // Display name (from JSON or directory name)
    std::string path;           // Full path to level.json
    std::string description;    // Optional description from level.json
    bool isSpecial = false;     // True for stress tests and other generated levels
    int specialId = 0;          // For special levels: stress test preset ID (1-4)
};

/**
 * LevelSelector - Discovers levels and tracks selection.
 */
class LevelSelector {
public:
    LevelSelector() = default;
    
    /**
     * Scan the levels directory for available levels.
     * @param levelsBasePath Base path to levels folder (e.g., "levels")
     */
    void ScanLevels(const std::string& levelsBasePath);
    
    /**
     * Get list of all discovered levels.
     */
    const std::vector<LevelInfo>& GetLevels() const { return m_levels; }
    
    /**
     * Get currently selected level index (-1 = none).
     */
    int GetSelectedIndex() const { return m_selectedIndex; }
    
    /**
     * Set selected level by index.
     */
    void SetSelectedIndex(int index);
    
    /**
     * Get info about selected level (nullptr if none).
     */
    const LevelInfo* GetSelectedLevel() const;
    
    /**
     * Check if a level load was requested (and clear the flag).
     */
    bool ConsumeLoadRequest();
    
    /**
     * Request to load the currently selected level.
     */
    void RequestLoad() { m_loadRequested = true; }
    
    /**
     * Get the currently loaded level path (empty if none).
     */
    const std::string& GetCurrentLevelPath() const { return m_currentLevelPath; }
    
    /**
     * Set the currently loaded level path.
     */
    void SetCurrentLevelPath(const std::string& path) { m_currentLevelPath = path; }
    
    /**
     * Get custom stress test parameters (editable via sliders).
     */
    StressTestParams& GetCustomParams() { return m_customParams; }
    const StressTestParams& GetCustomParams() const { return m_customParams; }
    
private:
    void AddSpecialLevels();
    
    std::vector<LevelInfo> m_levels;
    int m_selectedIndex = -1;
    bool m_loadRequested = false;
    std::string m_currentLevelPath;
    StressTestParams m_customParams;  // Custom stress test parameters
};
