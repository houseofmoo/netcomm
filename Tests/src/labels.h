#pragma once

#include <memory>
#include "scenario/scenario.h"
#if defined(EROIL_WIN32)
#include "windows_hdr.h"
#elif defined(EROIL_LINUX)
#include <semaphore.h>
#endif


constexpr size_t KILOBYTE = 1024;

struct SendLabel {
    int id;
    size_t size;
    int sleep_time;
    std::unique_ptr<std::uint8_t[]> buf;

    std::uint8_t* get_buf() { return buf.get(); }
};

inline SendLabel make_send_label(int id, size_t size, int sleep_time) {
    return SendLabel {
        id,
        size,
        sleep_time,
        std::make_unique<std::uint8_t[]>(size)
    };
}

inline std::vector<std::shared_ptr<SendLabel>> make_send_list(const std::vector<ScenarioSendLabel>& send_labels) {
    std::vector<std::shared_ptr<SendLabel>> labels;
    for(const auto& r : send_labels) {
        labels.push_back(
            std::make_shared<SendLabel>(make_send_label(r.id, r.size, r.send_rate_ms))
        );
    }   
    return labels;
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
#elif defined(EROIL_LINUX)
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

inline std::vector<std::shared_ptr<RecvLabel>> make_recv_list(const std::vector<ScenarioRecvLabel>& recv_labels) {
    std::vector<std::shared_ptr<RecvLabel>> labels;
    for(const auto& r : recv_labels) {
        labels.push_back(
            std::make_shared<RecvLabel>(make_recv_label(r.id, r.size))
        );
    }   
    return labels;
}