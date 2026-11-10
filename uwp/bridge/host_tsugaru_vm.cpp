// Tsugaru UWP - the host calls that must run on the VM thread, with the emulator's own headers (kept out of
// host_tsugaru.cpp, whose frontend headers clash with them).  Each function blocks until the VM ran it, or fails
// when no VM is running.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <functional>
#include <string>
#include <vector>

#include "towns.h"
#include "townsthread.h"
#include "cdrom.h"

#include "host_tsugaru_vm.h"

bool TsugaruUwpRunOnVM(std::function<void(TownsThread &, FMTownsCommon &)> fn, int timeoutMs);

namespace
{
constexpr int kTimeoutMs = 3000;
}

bool tsvm_serialize(std::vector<uint8_t> &out)
{
    bool ok = false;
    const bool ran = TsugaruUwpRunOnVM(
        [&](TownsThread &, FMTownsCommon &towns) {
            out = towns.SaveStateMem();
            ok = !out.empty();
        },
        kTimeoutMs);
    return ran && ok;
}

bool tsvm_unserialize(const uint8_t *data, size_t size)
{
    bool ok = false;
    std::vector<uint8_t> copy(data, data + size);
    const bool ran = TsugaruUwpRunOnVM(
        [&](TownsThread &, FMTownsCommon &towns) { ok = towns.LoadStateMem(copy); }, kTimeoutMs);
    return ran && ok;
}

bool tsvm_save_state(const std::string &path)
{
    bool ok = false;
    const bool ran = TsugaruUwpRunOnVM(
        [&](TownsThread &, FMTownsCommon &towns) { ok = towns.SaveState(path); }, kTimeoutMs * 4);
    return ran && ok;
}

bool tsvm_load_state(const std::string &path)
{
    bool ok = false;
    const bool ran = TsugaruUwpRunOnVM(
        [&](TownsThread &, FMTownsCommon &towns) { ok = towns.LoadState(path); }, kTimeoutMs * 4);
    return ran && ok;
}

bool tsvm_load_cd(const std::string &path)
{
    unsigned int err = ~0u;
    const bool ran = TsugaruUwpRunOnVM(
        [&](TownsThread &, FMTownsCommon &towns) { err = towns.cdrom.LoadDiscImage(path); }, kTimeoutMs * 4);
    return ran && err == DiscImage::ERROR_NOERROR;
}

bool tsvm_eject_cd(void)
{
    return TsugaruUwpRunOnVM([&](TownsThread &, FMTownsCommon &towns) { towns.cdrom.Eject(); }, kTimeoutMs);
}

bool tsvm_reset(void)
{
    return TsugaruUwpRunOnVM([&](TownsThread &, FMTownsCommon &towns) { towns.Reset(); }, kTimeoutMs);
}
