/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <string>
#include <vector>

namespace FileDialog
{
    struct FileFilter
    {
        std::string name;        // Display name (e.g., "ROM files")
        std::string extensions;  // Extensions (e.g., "*.nds;*.gba")
    };

    std::string openFile(
        const std::string& title,
        const std::string& defaultPath = "",
        const std::vector<FileFilter>& filters = {}
    );

    std::string saveFile(
        const std::string& title,
        const std::string& defaultPath = "",
        const std::vector<FileFilter>& filters = {}
    );

    std::string openFolder(
        const std::string& title,
        const std::string& defaultPath = ""
    );

    std::vector<std::string> openFiles(
        const std::string& title,
        const std::string& defaultPath = "",
        const std::vector<FileFilter>& filters = {}
    );

    namespace Filters
    {
        extern const std::vector<FileFilter> ROM_FILES;
        extern const std::vector<FileFilter> NDS_ROM_FILES;
        extern const std::vector<FileFilter> GBA_ROM_FILES;
        extern const std::vector<FileFilter> BIOS_FILES;
        extern const std::vector<FileFilter> FIRMWARE_FILES;
        extern const std::vector<FileFilter> SAVESTATE_FILES;
        extern const std::vector<FileFilter> SAVE_FILES;
        extern const std::vector<FileFilter> CHEAT_FILES;
        extern const std::vector<FileFilter> WAV_FILES;
        extern const std::vector<FileFilter> IMAGE_FILES;
        extern const std::vector<FileFilter> ALL_FILES;
    }
}

#endif // FILEDIALOG_H 