#pragma once

#include "app/types.h"
#include <map>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace edge_ai {

class EventBus {
public:
    explicit EventBus(size_t max_queue_size = 256);
    ~EventBus();

    SubscriptionId subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);

    void publish(Event event);

    void start();
    void stop();

private:
    void dispatchLoop();

    struct Subscription {
        SubscriptionId id;
        EventType type;
        EventCallback callback;
    };

    size_t max_queue_size_;
    std::atomic<bool> running_{false};
    SubscriptionId next_sub_id_{1};

    std::vector<Subscription> subscriptions_;
    std::mutex subs_mutex_;

    std::queue<Event> event_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread dispatch_thread_;
};

} // namespace edge_ai

