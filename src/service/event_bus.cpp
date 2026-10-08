#include "service/event_bus.h"

namespace edge_ai {

EventBus::EventBus(size_t max_queue_size)
    : max_queue_size_(max_queue_size) {
}

EventBus::~EventBus() {
    stop();
}

void EventBus::start() {
    if (running_) return;
    running_ = true;
    dispatch_thread_ = std::thread(&EventBus::dispatchLoop, this);
}

void EventBus::stop() {
    if (!running_) return;
    running_ = false;
    queue_cv_.notify_all();

    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }
}

SubscriptionId EventBus::subscribe(EventType type, EventCallback callback) {
    std::lock_guard<std::mutex> lock(subs_mutex_);
    SubscriptionId id = next_sub_id_++;
    subscriptions_.push_back({id, type, std::move(callback)});
    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(subs_mutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [id](const Subscription& s) { return s.id == id; }),
        subscriptions_.end()
    );
}

void EventBus::publish(Event event) {
    if (!running_) return;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (event_queue_.size() >= max_queue_size_) {
            // Ring buffer policy: Drop oldest event to avoid unbounded memory growth
            event_queue_.pop();
        }
        event_queue_.push(std::move(event));
    }
    queue_cv_.notify_one();
}

void EventBus::dispatchLoop() {
    while (running_) {
        Event event;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !event_queue_.empty() || !running_;
            });

            if (!running_ && event_queue_.empty()) {
                break;
            }

            event = std::move(event_queue_.front());
            event_queue_.pop();
        }

        // Invoke subscribers
        std::vector<EventCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(subs_mutex_);
            for (const auto& sub : subscriptions_) {
                if (sub.type == event.type) {
                    callbacks.push_back(sub.callback);
                }
            }
        }

        for (const auto& cb : callbacks) {
            try {
                cb(event);
            } catch (...) {
                // Safeguard against buggy subscriber callback exceptions
            }
        }
    }
}

} // namespace edge_ai

