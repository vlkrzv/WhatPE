#pragma once

#include "../PeBinaryInfoLib/PeBinaryInfo.h"

// Shows the multi-tab PE info dialog, one tab per peinfo::PeFileFormattedInfoCategory. Blocks
// until the dialog is closed.
void ShowPeInfoDialog(HINSTANCE hInstance, const std::wstring& filePath, const peinfo::PeFileFormattedInfo& peInfo);
