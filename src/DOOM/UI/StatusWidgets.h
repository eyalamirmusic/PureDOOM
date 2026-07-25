#pragma once

namespace Doom
{
// The widgets themselves are StatusNumber / StatusPercent / StatusMultIcon /
// StatusBinIcon in StatusWidgetTypes.h, each carrying its own init/update. What
// is left here is the one-off lump cache the number widget's minus sign needs,
// which belongs to no single widget.
void initStatusWidgets();
} // namespace Doom
