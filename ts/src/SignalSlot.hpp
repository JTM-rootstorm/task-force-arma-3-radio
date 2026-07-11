#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

class ScopedSignalConnection {
public:
    ScopedSignalConnection() = default;
    explicit ScopedSignalConnection(std::function<void()> disconnect) : disconnect_(std::move(disconnect)) {}
    ScopedSignalConnection(const ScopedSignalConnection&) = delete;
    ScopedSignalConnection& operator=(const ScopedSignalConnection&) = delete;
    ScopedSignalConnection(ScopedSignalConnection&& other) noexcept : disconnect_(std::move(other.disconnect_)) {}
    ScopedSignalConnection& operator=(ScopedSignalConnection&& other) noexcept {
        if (this != &other) { disconnect(); disconnect_ = std::move(other.disconnect_); }
        return *this;
    }
    ~ScopedSignalConnection() { disconnect(); }
    void disconnect() { if (disconnect_) { auto callback = std::move(disconnect_); callback(); } }
private:
    std::function<void()> disconnect_;
};

template<class Sig> class Signal;

template<class ReturnType, class... Args>
class Signal<ReturnType(Args...)> {
    using Slot = std::function<ReturnType(Args...)>;
    struct Entry { std::uint64_t id; Slot slot; };
    struct State { std::mutex mutex; std::vector<Entry> slots; std::uint64_t nextId = 1; };
public:
    std::uint64_t connect(Slot slot) { return add(std::move(slot)); }
    ScopedSignalConnection connectScoped(Slot slot) {
        const auto id = add(std::move(slot));
        std::weak_ptr<State> state = state_;
        return ScopedSignalConnection([state, id]() { if (auto locked = state.lock()) remove(*locked, id); });
    }
    std::vector<ReturnType> operator()(Args... args) const { return emit(args...); }
    std::vector<ReturnType> emit(Args... args) const {
        const auto slots = snapshot();
        std::vector<ReturnType> results;
        results.reserve(slots.size());
        for (const auto& slot : slots) results.push_back(slot(args...));
        return results;
    }
    void removeAllSlots() { std::lock_guard<std::mutex> lock(state_->mutex); state_->slots.clear(); }
private:
    std::uint64_t add(Slot slot) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        const auto id = state_->nextId++;
        state_->slots.push_back({ id, std::move(slot) });
        return id;
    }
    static void remove(State& state, std::uint64_t id) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.slots.erase(std::remove_if(state.slots.begin(), state.slots.end(), [id](const Entry& entry) { return entry.id == id; }), state.slots.end());
    }
    std::vector<Slot> snapshot() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        std::vector<Slot> result;
        result.reserve(state_->slots.size());
        for (const auto& entry : state_->slots) result.push_back(entry.slot);
        return result;
    }
    std::shared_ptr<State> state_ = std::make_shared<State>();
};

template<class... Args>
class Signal<void(Args...)> {
    using Slot = std::function<void(Args...)>;
    struct Entry { std::uint64_t id; Slot slot; };
    struct State { std::mutex mutex; std::vector<Entry> slots; std::uint64_t nextId = 1; };
public:
    std::uint64_t connect(Slot slot) { return add(std::move(slot)); }
    ScopedSignalConnection connectScoped(Slot slot) {
        const auto id = add(std::move(slot));
        std::weak_ptr<State> state = state_;
        return ScopedSignalConnection([state, id]() { if (auto locked = state.lock()) remove(*locked, id); });
    }
    void operator()(Args... args) const { emit(args...); }
    void emit(Args... args) const { for (const auto& slot : snapshot()) slot(args...); }
    void removeAllSlots() { std::lock_guard<std::mutex> lock(state_->mutex); state_->slots.clear(); }
private:
    std::uint64_t add(Slot slot) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        const auto id = state_->nextId++;
        state_->slots.push_back({ id, std::move(slot) });
        return id;
    }
    static void remove(State& state, std::uint64_t id) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.slots.erase(std::remove_if(state.slots.begin(), state.slots.end(), [id](const Entry& entry) { return entry.id == id; }), state.slots.end());
    }
    std::vector<Slot> snapshot() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        std::vector<Slot> result;
        result.reserve(state_->slots.size());
        for (const auto& entry : state_->slots) result.push_back(entry.slot);
        return result;
    }
    std::shared_ptr<State> state_ = std::make_shared<State>();
};
