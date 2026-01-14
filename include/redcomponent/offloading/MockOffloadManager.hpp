/**
 * @file MockOffloadManager.hpp
 * @brief Mock Offload Manager Implementation for Testing
 * @copyright Copyright (c) 2026 BEP Venture UG. All rights reserved.
 * @license EULA - Proprietary
 */

#pragma once

#include "IOffloadManager.hpp"
#include <mutex>
#include <algorithm>

namespace redcomponent::offloading {

/**
 * @brief Mock Offload Manager for Unit Testing
 *
 * Provides a fully functional mock implementation of IOffloadManager
 * with test hooks for simulating various scenarios including:
 * - Node availability
 * - Transfer progress
 * - Success/failure conditions
 * - Error conditions
 */
class MockOffloadManager : public IOffloadManager {
private:
    OffloadConfig config_;
    OffloadStatus status_ = OffloadStatus::Idle;
    OffloadProgress progress_;
    std::optional<TargetNode> current_target_;
    std::optional<OffloadResult> last_result_;
    std::vector<TargetNode> available_nodes_;
    mutable std::mutex mutex_;

    // Callbacks
    std::function<void(const OffloadProgress&)> progress_callback_;
    std::function<void(const OffloadResult&)> complete_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(OffloadStatus, OffloadStatus)> status_change_callback_;

    // Test hooks
    std::function<bool()> start_hook_;
    std::function<bool()> cancel_hook_;
    std::function<std::vector<TargetNode>()> nodes_hook_;
    std::function<bool(const std::string&)> select_node_hook_;

    // Offload data tracking
    std::vector<std::string> offload_data_ids_;

    void set_status(OffloadStatus new_status) {
        OffloadStatus old_status = status_;
        status_ = new_status;
        if (status_change_callback_ && old_status != new_status) {
            status_change_callback_(old_status, new_status);
        }
    }

    void notify_error(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
    }

    void notify_progress() {
        progress_.last_update = std::chrono::steady_clock::now();
        if (progress_.start_time.time_since_epoch().count() > 0) {
            progress_.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                progress_.last_update - progress_.start_time);
        }
        if (progress_callback_) {
            progress_callback_(progress_);
        }
    }

    void notify_complete(bool success) {
        OffloadResult result;
        result.success = success;
        result.final_progress = progress_;
        result.completed_at = std::chrono::steady_clock::now();
        if (current_target_) {
            result.target_node = *current_target_;
        }
        if (!success && progress_.error_message) {
            result.error_message = progress_.error_message;
        }
        last_result_ = result;
        if (complete_callback_) {
            complete_callback_(result);
        }
    }

public:
    MockOffloadManager() {
        // Initialize with default mock nodes
        available_nodes_ = {
            create_mock_node("node1", "192.168.1.10", 100ULL * 1024 * 1024 * 1024),
            create_mock_node("node2", "192.168.1.11", 200ULL * 1024 * 1024 * 1024),
            create_mock_node("node3", "192.168.1.12", 50ULL * 1024 * 1024 * 1024)
        };
    }

    // ─────────────────────────────────────────────────────────────────
    // IOffloadManager Implementation
    // ─────────────────────────────────────────────────────────────────

    void set_config(const OffloadConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }

    [[nodiscard]] OffloadConfig get_config() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    [[nodiscard]] std::vector<TargetNode> get_available_nodes() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nodes_hook_) {
            return nodes_hook_();
        }
        return available_nodes_;
    }

    bool refresh_nodes() override {
        std::lock_guard<std::mutex> lock(mutex_);
        // Mock: just update health check timestamps
        auto now = std::chrono::steady_clock::now();
        for (auto& node : available_nodes_) {
            node.last_health_check = now;
        }
        return true;
    }

    bool select_target_node(const std::string& node_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (select_node_hook_) {
            return select_node_hook_(node_id);
        }

        for (const auto& node : available_nodes_) {
            if (node.node_id == node_id) {
                if (!node.can_accept_offload()) {
                    notify_error("Node " + node_id + " cannot accept offloads");
                    return false;
                }
                current_target_ = node;
                return true;
            }
        }
        notify_error("Node not found: " + node_id);
        return false;
    }

    bool auto_select_target_node() override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find best node (most available storage, healthy, can accept)
        TargetNode* best = nullptr;
        for (auto& node : available_nodes_) {
            if (node.can_accept_offload()) {
                if (!best || node.available_storage_bytes > best->available_storage_bytes) {
                    best = &node;
                }
            }
        }

        if (best) {
            current_target_ = *best;
            return true;
        }

        notify_error("No suitable target node available");
        return false;
    }

    [[nodiscard]] std::optional<TargetNode> get_current_target() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_target_;
    }

    void clear_target_selection() override {
        std::lock_guard<std::mutex> lock(mutex_);
        current_target_.reset();
    }

    bool start_offload() override {
        return start_offload({});
    }

    bool start_offload(const std::vector<std::string>& data_ids) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (start_hook_) {
            bool result = start_hook_();
            if (result) {
                set_status(OffloadStatus::Transferring);
            }
            return result;
        }

        if (!current_target_.has_value()) {
            notify_error("No target node selected");
            return false;
        }

        if (status_ != OffloadStatus::Idle && status_ != OffloadStatus::Paused) {
            notify_error("Offload already in progress or not in valid state");
            return false;
        }

        // Initialize progress
        offload_data_ids_ = data_ids;
        progress_ = OffloadProgress{};
        progress_.start_time = std::chrono::steady_clock::now();
        progress_.total_bytes = 100 * 1024 * 1024; // Mock: 100MB
        progress_.pending_bytes = progress_.total_bytes;
        progress_.segments_total = 100;
        progress_.segments_pending = 100;

        set_status(OffloadStatus::Preparing);
        set_status(OffloadStatus::Transferring);

        return true;
    }

    bool cancel_offload() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (cancel_hook_) {
            return cancel_hook_();
        }

        if (status_ == OffloadStatus::Idle ||
            status_ == OffloadStatus::Completed ||
            status_ == OffloadStatus::Failed ||
            status_ == OffloadStatus::Cancelled) {
            notify_error("No active offload to cancel");
            return false;
        }

        set_status(OffloadStatus::Cancelled);
        notify_complete(false);
        return true;
    }

    bool pause_offload() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (status_ != OffloadStatus::Transferring) {
            notify_error("Cannot pause: not transferring");
            return false;
        }

        set_status(OffloadStatus::Paused);
        return true;
    }

    bool resume_offload() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (status_ != OffloadStatus::Paused) {
            notify_error("Cannot resume: not paused");
            return false;
        }

        set_status(OffloadStatus::Transferring);
        return true;
    }

    [[nodiscard]] OffloadStatus get_status() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    [[nodiscard]] OffloadProgress get_progress() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return progress_;
    }

    [[nodiscard]] bool is_active() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_ == OffloadStatus::Preparing ||
               status_ == OffloadStatus::Transferring ||
               status_ == OffloadStatus::Completing ||
               status_ == OffloadStatus::Paused;
    }

    [[nodiscard]] std::optional<OffloadResult> get_last_result() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_result_;
    }

    void on_progress(std::function<void(const OffloadProgress&)> callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_callback_ = std::move(callback);
    }

    void on_complete(std::function<void(const OffloadResult&)> callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        complete_callback_ = std::move(callback);
    }

    void on_error(std::function<void(const std::string&)> callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        error_callback_ = std::move(callback);
    }

    void on_status_change(
        std::function<void(OffloadStatus, OffloadStatus)> callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_change_callback_ = std::move(callback);
    }

    // ─────────────────────────────────────────────────────────────────
    // Test Helper Methods
    // ─────────────────────────────────────────────────────────────────

    /**
     * @brief Create a mock target node
     */
    static TargetNode create_mock_node(
        const std::string& id,
        const std::string& host,
        size_t available_storage,
        double cpu = 30.0,
        double memory = 40.0) {
        TargetNode node;
        node.node_id = id;
        node.host = host;
        node.port = 5432;
        node.cluster_id = "test-cluster";
        node.region = "us-east-1";
        node.total_storage_bytes = available_storage * 2;
        node.available_storage_bytes = available_storage;
        node.used_storage_bytes = available_storage;
        node.cpu_usage_percent = cpu;
        node.memory_usage_percent = memory;
        node.health = NodeHealth::Healthy;
        node.accepting_offloads = true;
        node.active_offload_count = 0;
        node.max_concurrent_offloads = 10;
        node.last_health_check = std::chrono::steady_clock::now();
        return node;
    }

    /**
     * @brief Set custom start offload behavior
     */
    void set_start_hook(std::function<bool()> hook) {
        std::lock_guard<std::mutex> lock(mutex_);
        start_hook_ = std::move(hook);
    }

    /**
     * @brief Set custom cancel behavior
     */
    void set_cancel_hook(std::function<bool()> hook) {
        std::lock_guard<std::mutex> lock(mutex_);
        cancel_hook_ = std::move(hook);
    }

    /**
     * @brief Set custom nodes list behavior
     */
    void set_nodes_hook(std::function<std::vector<TargetNode>()> hook) {
        std::lock_guard<std::mutex> lock(mutex_);
        nodes_hook_ = std::move(hook);
    }

    /**
     * @brief Set custom node selection behavior
     */
    void set_select_node_hook(std::function<bool(const std::string&)> hook) {
        std::lock_guard<std::mutex> lock(mutex_);
        select_node_hook_ = std::move(hook);
    }

    /**
     * @brief Force a specific status
     */
    void force_status(OffloadStatus status) {
        std::lock_guard<std::mutex> lock(mutex_);
        set_status(status);
    }

    /**
     * @brief Simulate progress update
     * @param bytes Number of bytes transferred
     */
    void simulate_progress(size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);

        progress_.transferred_bytes += bytes;
        progress_.pending_bytes = progress_.total_bytes - progress_.transferred_bytes;
        progress_.segments_completed++;
        progress_.segments_pending = progress_.segments_total - progress_.segments_completed;

        // Calculate transfer rate
        auto now = std::chrono::steady_clock::now();
        if (progress_.start_time.time_since_epoch().count() > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - progress_.start_time).count();
            if (elapsed > 0) {
                progress_.average_bytes_per_second =
                    static_cast<double>(progress_.transferred_bytes) / elapsed;
            }
        }
        progress_.bytes_per_second = static_cast<double>(bytes);

        notify_progress();
    }

    /**
     * @brief Simulate offload completion
     * @param success Whether offload succeeded
     */
    void simulate_complete(bool success) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (success) {
            progress_.transferred_bytes = progress_.total_bytes;
            progress_.pending_bytes = 0;
            progress_.segments_completed = progress_.segments_total;
            progress_.segments_pending = 0;
            set_status(OffloadStatus::Completing);
            set_status(OffloadStatus::Completed);
        } else {
            set_status(OffloadStatus::Failed);
        }

        notify_complete(success);
    }

    /**
     * @brief Simulate an error
     * @param error Error message
     */
    void simulate_error(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);

        progress_.error_message = error;
        set_status(OffloadStatus::Failed);
        notify_error(error);
        notify_complete(false);
    }

    /**
     * @brief Set available nodes list
     */
    void set_available_nodes(const std::vector<TargetNode>& nodes) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_nodes_ = nodes;
    }

    /**
     * @brief Add a node to available list
     */
    void add_node(const TargetNode& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_nodes_.push_back(node);
    }

    /**
     * @brief Remove a node from available list
     */
    void remove_node(const std::string& node_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_nodes_.erase(
            std::remove_if(available_nodes_.begin(), available_nodes_.end(),
                [&node_id](const TargetNode& n) { return n.node_id == node_id; }),
            available_nodes_.end());
    }

    /**
     * @brief Clear all nodes
     */
    void clear_nodes() {
        std::lock_guard<std::mutex> lock(mutex_);
        available_nodes_.clear();
    }

    /**
     * @brief Set node health
     */
    void set_node_health(const std::string& node_id, NodeHealth health) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& node : available_nodes_) {
            if (node.node_id == node_id) {
                node.health = health;
                break;
            }
        }
    }

    /**
     * @brief Reset mock to initial state
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);

        status_ = OffloadStatus::Idle;
        progress_ = OffloadProgress{};
        current_target_.reset();
        last_result_.reset();
        offload_data_ids_.clear();

        // Clear hooks
        start_hook_ = nullptr;
        cancel_hook_ = nullptr;
        nodes_hook_ = nullptr;
        select_node_hook_ = nullptr;

        // Reset nodes to default
        available_nodes_ = {
            create_mock_node("node1", "192.168.1.10", 100ULL * 1024 * 1024 * 1024),
            create_mock_node("node2", "192.168.1.11", 200ULL * 1024 * 1024 * 1024),
            create_mock_node("node3", "192.168.1.12", 50ULL * 1024 * 1024 * 1024)
        };
    }

    /**
     * @brief Get offload data IDs (for verification)
     */
    [[nodiscard]] std::vector<std::string> get_offload_data_ids() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return offload_data_ids_;
    }

    /**
     * @brief Get number of available nodes
     */
    [[nodiscard]] size_t node_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_nodes_.size();
    }
};

} // namespace redcomponent::offloading
