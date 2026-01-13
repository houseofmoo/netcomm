#pragma once

#include <memory>
#include "windows_hdr.h"

constexpr size_t KILOBYTE = 1024;

struct SendLabel {
    int id;
    int size;
    std::unique_ptr<std::uint8_t[]> buf;

    std::uint8_t* get_buf() { return buf.get(); }
};

inline SendLabel make_send_label(int id, int size) {
    return SendLabel {
        id,
        size,
        std::make_unique<std::uint8_t[]>(size)
    };
}

struct RecvLabel {
    int id;
    int size;
    std::unique_ptr<std::uint8_t[]> buf;
    HANDLE sem;

    std::uint8_t* get_buf() { return buf.get(); }
    void wait() {
        if (sem != nullptr) {
            ::WaitForSingleObject(sem, INFINITE);
        }
    }
};

inline RecvLabel make_recv_label(int id, int size) {
    return RecvLabel {
        id,
        size,
        std::make_unique<std::uint8_t[]>(size),
        ::CreateSemaphoreW(nullptr, 0, 1000, nullptr)
    };
}