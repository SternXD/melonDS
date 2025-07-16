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

#include "FileDialog.h"
#include <algorithm>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
    #include <commdlg.h>
    #include <shlobj.h>
    #include <locale>
    #include <codecvt>
#elif defined(__APPLE__)
    #include <Cocoa/Cocoa.h>
#else
    #include <cstdlib>
    #include <iostream>
#endif

namespace FileDialog
{
    namespace
    {
        std::string wstringToString(const std::wstring& wstr)
        {
#ifdef _WIN32
            if (wstr.empty()) return std::string();
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
            std::string strTo(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
            return strTo;
#else
            return std::string(wstr.begin(), wstr.end());
#endif
        }

        std::wstring stringToWstring(const std::string& str)
        {
#ifdef _WIN32
            if (str.empty()) return std::wstring();
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
            std::wstring wstrTo(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
            return wstrTo;
#else
            return std::wstring(str.begin(), str.end());
#endif
        }

        std::string formatFilters(const std::vector<FileFilter>& filters)
        {
#ifdef _WIN32
            std::string result;
            for (const auto& filter : filters)
            {
                result += filter.name + '\0' + filter.extensions + '\0';
            }
            if (!result.empty())
            {
                result += '\0';
            }
            return result;
#else
            // For non-Windows platforms, return first extension for fallback
            if (!filters.empty() && !filters[0].extensions.empty())
            {
                std::string ext = filters[0].extensions;
                // Extract first extension (remove *.  prefix)
                size_t pos = ext.find("*.");
                if (pos != std::string::npos)
                {
                    ext = ext.substr(pos + 2);
                    pos = ext.find(';');
                    if (pos != std::string::npos)
                    {
                        ext = ext.substr(0, pos);
                    }
                    return ext;
                }
            }
            return "";
#endif
        }
    }

#ifdef _WIN32
    std::string openFile(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        OPENFILENAMEA ofn = {};
        char szFile[260] = { 0 };
        
        if (!defaultPath.empty())
        {
            strcpy_s(szFile, defaultPath.c_str());
        }

        std::string filterStr = formatFilters(filters);
        
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filterStr.empty() ? nullptr : filterStr.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn))
        {
            return std::string(szFile);
        }
        return "";
    }

    std::string saveFile(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        OPENFILENAMEA ofn = {};
        char szFile[260] = { 0 };
        
        if (!defaultPath.empty())
        {
            strcpy_s(szFile, defaultPath.c_str());
        }

        std::string filterStr = formatFilters(filters);
        
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filterStr.empty() ? nullptr : filterStr.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn))
        {
            return std::string(szFile);
        }
        return "";
    }

    std::string openFolder(const std::string& title, const std::string& defaultPath)
    {
        BROWSEINFOA bi = {};
        bi.lpszTitle = title.c_str();
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl != nullptr)
        {
            char path[MAX_PATH];
            if (SHGetPathFromIDListA(pidl, path))
            {
                CoTaskMemFree(pidl);
                return std::string(path);
            }
            CoTaskMemFree(pidl);
        }
        return "";
    }

    std::vector<std::string> openFiles(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        OPENFILENAMEA ofn = {};
        char szFile[1024] = { 0 };
        
        std::string filterStr = formatFilters(filters);
        
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filterStr.empty() ? nullptr : filterStr.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_NOCHANGEDIR;

        std::vector<std::string> result;
        if (GetOpenFileNameA(&ofn))
        {
            std::string dir = szFile;
            char* filename = szFile + dir.length() + 1;
            
            if (*filename == '\0')
            {
                result.push_back(dir);
            }
            else
            {
                while (*filename)
                {
                    result.push_back(dir + "\\" + filename);
                    filename += strlen(filename) + 1;
                }
            }
        }
        return result;
    }

#elif defined(__APPLE__)
    std::string openFile(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:NO];
            
            if ([panel runModal] == NSModalResponseOK)
            {
                NSURL* url = [[panel URLs] objectAtIndex:0];
                return std::string([[url path] UTF8String]);
            }
        }
        return "";
    }

    std::string saveFile(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        @autoreleasepool {
            NSSavePanel* panel = [NSSavePanel savePanel];
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
            
            if ([panel runModal] == NSModalResponseOK)
            {
                NSURL* url = [panel URL];
                return std::string([[url path] UTF8String]);
            }
        }
        return "";
    }

    std::string openFolder(const std::string& title, const std::string& defaultPath)
    {
        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
            [panel setCanChooseFiles:NO];
            [panel setCanChooseDirectories:YES];
            [panel setAllowsMultipleSelection:NO];
            
            if ([panel runModal] == NSModalResponseOK)
            {
                NSURL* url = [[panel URLs] objectAtIndex:0];
                return std::string([[url path] UTF8String]);
            }
        }
        return "";
    }

    std::vector<std::string> openFiles(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:YES];
            
            std::vector<std::string> result;
            if ([panel runModal] == NSModalResponseOK)
            {
                NSArray* urls = [panel URLs];
                for (NSURL* url in urls)
                {
                    result.push_back(std::string([[url path] UTF8String]));
                }
            }
            return result;
        }
    }

#else
    std::string openFile(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        std::string command = "zenity --file-selection --title=\"" + title + "\"";
        
        if (!defaultPath.empty())
        {
            command += " --filename=\"" + defaultPath + "\"";
        }
        
        std::string filterExt = formatFilters(filters);
        if (!filterExt.empty())
        {
            command += " --file-filter=\"*." + filterExt + "\"";
        }
        
        command += " 2>/dev/null";
        
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";
        
        char buffer[1024];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result = buffer;
            if (!result.empty() && result.back() == '\n')
            {
                result.pop_back();
            }
        }
        pclose(pipe);
        return result;
    }

    std::string saveFile(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        std::string command = "zenity --file-selection --save --title=\"" + title + "\"";
        
        if (!defaultPath.empty())
        {
            command += " --filename=\"" + defaultPath + "\"";
        }
        
        command += " 2>/dev/null";
        
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";
        
        char buffer[1024];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result = buffer;
            if (!result.empty() && result.back() == '\n')
            {
                result.pop_back();
            }
        }
        pclose(pipe);
        return result;
    }

    std::string openFolder(const std::string& title, const std::string& defaultPath)
    {
        std::string command = "zenity --file-selection --directory --title=\"" + title + "\"";
        
        if (!defaultPath.empty())
        {
            command += " --filename=\"" + defaultPath + "\"";
        }
        
        command += " 2>/dev/null";
        
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";
        
        char buffer[1024];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result = buffer;
            if (!result.empty() && result.back() == '\n')
            {
                result.pop_back();
            }
        }
        pclose(pipe);
        return result;
    }

    std::vector<std::string> openFiles(const std::string& title, const std::string& defaultPath, const std::vector<FileFilter>& filters)
    {
        std::string command = "zenity --file-selection --multiple --separator='|' --title=\"" + title + "\"";
        
        if (!defaultPath.empty())
        {
            command += " --filename=\"" + defaultPath + "\"";
        }
        
        command += " 2>/dev/null";
        
        FILE* pipe = popen(command.c_str(), "r");
        std::vector<std::string> result;
        if (!pipe) return result;
        
        char buffer[2048];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            std::string output = buffer;
            if (!output.empty() && output.back() == '\n')
            {
                output.pop_back();
            }
            
            std::stringstream ss(output);
            std::string item;
            while (std::getline(ss, item, '|'))
            {
                result.push_back(item);
            }
        }
        pclose(pipe);
        return result;
    }
#endif

    namespace Filters
    {
        const std::vector<FileFilter> ROM_FILES = {
            {"DS ROM files", "*.nds;*.srl;*.ids"},
            {"GBA ROM files", "*.gba;*.agb;*.mb"},
            {"All supported files", "*.nds;*.srl;*.ids;*.gba;*.agb;*.mb"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> NDS_ROM_FILES = {
            {"DS ROM files", "*.nds;*.srl;*.ids"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> GBA_ROM_FILES = {
            {"GBA ROM files", "*.gba;*.agb;*.mb"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> BIOS_FILES = {
            {"BIOS files", "*.bin;*.rom"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> FIRMWARE_FILES = {
            {"Firmware files", "*.bin;*.rom"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> SAVESTATE_FILES = {
            {"melonDS savestates", "*.ml*"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> SAVE_FILES = {
            {"Save files", "*.sav;*.bin;*.dsv"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> CHEAT_FILES = {
            {"Cheat files", "*.mch"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> WAV_FILES = {
            {"WAV files", "*.wav"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> IMAGE_FILES = {
            {"Image files", "*.bin;*.img;*.rom;*.sd;*.dmg"},
            {"All files", "*.*"}
        };

        const std::vector<FileFilter> ALL_FILES = {
            {"All files", "*.*"}
        };
    }
} 