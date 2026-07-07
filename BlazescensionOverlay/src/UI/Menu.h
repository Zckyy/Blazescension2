#pragma once

#include "Core/AppConfig.h"
#include "Core/Types.h"

namespace UI {

void applyBlazeStyle();
void drawMenu(Core::AppConfig& config, const Core::GameSnapshot& snapshot);
void drawStatusPanel(const Core::AppConfig& config, const Core::GameSnapshot& snapshot);

} // namespace UI

