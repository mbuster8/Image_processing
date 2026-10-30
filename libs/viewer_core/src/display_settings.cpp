#include "viewer_core/display_settings.h"

namespace viewer_core {

DisplaySettings DisplaySettingsStore::get(const std::string& name) const {
    const auto found = m_settings.find(name);
    return found != m_settings.end() ? found->second : DisplaySettings{};
}

// Only names that differ from the default are kept, so the store grows with what the user changed.
void DisplaySettingsStore::set(const std::string& name, const DisplaySettings& settings) {
    if (settings == DisplaySettings{}) {
        m_settings.erase(name);
    } else {
        m_settings[name] = settings;
    }
}

} // namespace viewer_core
