// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_state.h"

#include <memory>
#include <mutex>
#include <new>
#include <string_view>

namespace Libraries::AvPlayer {

#ifdef __APPLE__
extern "C" void* runtime_guest_calloc(size_t count, size_t size);
extern "C" void runtime_guest_free(void* p);
#endif

class AvPlayer {
public:
#ifdef __APPLE__
    // tlg: the game keeps this object's address as its player handle; below 1 TiB on macOS.
    static void* operator new(size_t size) {
        if (void* p = runtime_guest_calloc(1, size)) {
            return p;
        }
        throw std::bad_alloc();
    }
    static void operator delete(void* p) {
        runtime_guest_free(p);
    }
#endif
    AvPlayer(const AvPlayerInitData& data);

    s32 PostInit(const AvPlayerPostInitData& data);
    s32 AddSource(std::string_view filename);
    s32 AddSourceEx(std::string_view path, AvPlayerSourceType source_type);
    s32 GetStreamCount();
    s32 GetStreamInfo(u32 stream_index, AvPlayerStreamInfo& info);
    s32 EnableStream(u32 stream_index);
    s32 Start();
    s32 Pause();
    s32 Resume();
    s32 SetAvSyncMode(AvPlayerAvSyncMode sync_mode);
    bool GetAudioData(AvPlayerFrameInfo& audio_info);
    bool GetVideoData(AvPlayerFrameInfo& video_info);
    bool GetVideoData(AvPlayerFrameInfoEx& video_info);
    bool IsActive();
    u64 CurrentTime();
    s32 Stop();
    bool SetLooping(bool is_looping);

private:
    // Memory Replacement
    static void* PS4_SYSV_ABI Allocate(void* handle, u32 alignment, u32 size);
    static void PS4_SYSV_ABI Deallocate(void* handle, void* memory);
    static void* PS4_SYSV_ABI AllocateTexture(void* handle, u32 alignment, u32 size);
    static void PS4_SYSV_ABI DeallocateTexture(void* handle, void* memory);

    // File Replacement
    static int PS4_SYSV_ABI OpenFile(void* handle, const char* filename);
    static int PS4_SYSV_ABI CloseFile(void* handle);
    static int PS4_SYSV_ABI ReadOffsetFile(void* handle, u8* buffer, u64 position, u32 length);
    static u64 PS4_SYSV_ABI SizeFile(void* handle);

    AvPlayerInitData StubInitData(const AvPlayerInitData& data);

    AvPlayerInitData m_init_data{};
    AvPlayerInitData m_init_data_original{};
    std::mutex m_file_io_mutex{};

    std::unique_ptr<AvPlayerState> m_state{};
};

} // namespace Libraries::AvPlayer
