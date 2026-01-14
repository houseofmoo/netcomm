#pragma once

#include <memory>
#include "windows_hdr.h"

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

#if defined(EROIL_WIN32)
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
#endif

#if defined(EROIL_LINUX)
#include <semaphore.h>
struct RecvLabel {
    int id;
    int size;
    std::unique_ptr<std::uint8_t[]> buf;
    sem_t* sem;

    std::uint8_t* get_buf() { return buf.get(); }
    void wait() {
        if (sem != nullptr) {
            sem_wait(sem);
        }
    }
};

inline RecvLabel make_recv_label(int id, int size) {
    sem_t* sem = new sem_t();
    sem_init(sem, 0, 0);
    
    return RecvLabel {
        id,
        size,
        std::make_unique<std::uint8_t[]>(size),
        sem
    };
}
#endif