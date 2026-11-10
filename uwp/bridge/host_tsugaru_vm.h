// Tsugaru UWP - VM-thread operations for the host bridge (see host_tsugaru_vm.cpp).  All block until done.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

bool tsvm_serialize(std::vector<uint8_t> &out);
bool tsvm_unserialize(const uint8_t *data, size_t size);
bool tsvm_save_state(const std::string &path);
bool tsvm_load_state(const std::string &path);
bool tsvm_load_cd(const std::string &path);
bool tsvm_eject_cd(void);
bool tsvm_reset(void);
