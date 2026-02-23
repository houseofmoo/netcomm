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
    std::unique_ptr<std::byte[]> buf;
};

inline SendLabel make_send_label(int id, size_t size, int sleep_time) {
    return SendLabel {
        id,
        size,
        sleep_time,
        std::make_unique<std::byte[]>(size)
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
    std::unique_ptr<std::byte[]> buf;
    HANDLE sem;

    void wait() {
        if (sem != nullptr) {
            ::WaitForSingleObject(sem, INFINITE);
        }
    }

    bool timed_wait() {
        if (sem != nullptr) {
            if (::WaitForSingleObject(sem, 10 * 1000) == WAIT_TIMEOUT) {
                return false;
            }
            return true;
        }
        return false;
    }
};

inline RecvLabel make_recv_label(int id, int size) {
    return RecvLabel {
        id,
        size,
        std::make_unique<std::byte[]>(size),
        ::CreateSemaphoreW(nullptr, 0, 1000, nullptr)
    };
}
#elif defined(EROIL_LINUX)
struct RecvLabel {
    int id;
    int size;
    std::unique_ptr<std::byte[]> buf;
    sem_t* sem;

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
        std::make_unique<std::byte[]>(size),
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