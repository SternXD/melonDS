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

#ifndef IMGUIMULTIINSTANCE_H
#define IMGUIMULTIINSTANCE_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>

class ImGuiEmuInstance;

extern const int kMaxEmuInstances;
extern ImGuiEmuInstance* emuInstances[];

extern std::chrono::steady_clock::time_point sysTimerStart;

bool createEmuInstance();
void deleteEmuInstance(int id);
void deleteAllEmuInstances(int first = 0);
int numEmuInstances();

enum InstanceCommand
{
    InstCmd_Pause,
    InstCmd_Unpause,
    InstCmd_UpdateRecentFiles,
};

void broadcastInstanceCommand(int cmd, int sourceinst);

std::string getEmuDirectory();
void setEmuDirectory(const std::string& dir);

#endif // IMGUIMULTIINSTANCE_H 