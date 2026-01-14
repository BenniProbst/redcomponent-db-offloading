/**
 * @file IOffloadManager.hpp
 * @brief Database Offloading Manager Interface
 * @copyright Copyright (c) 2026 BEP Venture UG. All rights reserved.
 * @license EULA - Proprietary
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>
#include <functional>
#include <cstdint>

namespace redcomponent::offloading {

/**
 * @brief Offload operation status
 */
enum class OffloadStatus {
    Idle,           ///< No offload in progress
    Preparing,      ///< Preparing offload operation
    Transferring,   ///< Data transfer in progress
    Completing,     ///< Finalizing offload
    Completed,      ///< Offload completed successfully
    Failed,         ///< Offload failed
    Cancelled,      ///< Offload was cancelled
    Paused          ///< Offload is paused
};

/**
 * @brief Convert OffloadStatus to string
 */
inline std::string to_string(OffloadStatus status) {
    switch (status) {
        case OffloadStatus::Idle:        return "Idle";
        case OffloadStatus::Preparing:   return "Preparing";
        case OffloadStatus::Transferring: return "Transferring";
        case OffloadStatus::Completing:  return "Completing";
        case OffloadStatus::Completed:   return "Completed";
        case OffloadStatus::Failed:      return "Failed";
        case OffloadStatus::Cancelled:   return "Cancelled";
        case OffloadStatus::Paused:      return "Paused";
        default:                         return "Unknown";
    }
}

/**
 * @brief Health status of a node
 */
enum class NodeHealth {
    Healthy,        ///< Node is healthy and available
    Degraded,       ///< Node is operational but degraded
    Unhealthy,      ///< Node is unhealthy
    Unknown         ///< Health status unknown
};

/**
 * @brief Target node information
 */
struct TargetNode {
    std::string node_id;                    ///< Unique node identifier
    std::string host;                       ///< Node hostname or IP
    uint16_t port = 5432;                   ///< Node port
    std::string cluster_id;                 ///< Cluster identifier
    std::string region;                     ///< Geographic region

    // Resource information
    size_t total_storage_bytes = 0;         ///< Total storage capacity
    size_t available_storage_bytes = 0;     ///< Available storage
    size_t used_storage_bytes = 0;          ///< Used storage
    double cpu_usage_percent = 0.0;         ///< CPU utilization
    double memory_usage_percent = 0.0;      ///< Memory utilization
    double network_utilization_percent = 0.0; ///< Network utilization

    // Health and availability
    NodeHealth health = NodeHealth::Unknown;
    bool accepting_offloads = true;         ///< Whether node accepts offloads
    size_t active_offload_count = 0;        ///< Current active offloads
    size_t max_concurrent_offloads = 10;    ///< Maximum concurrent offloads

    // Timestamps
    std::chrono::steady_clock::time_point last_health_check;
    std::chrono::steady_clock::time_point last_successful_offload;

    /**
     * @brief Calculate storage usage percentage
     */
    [[nodiscard]] double storage_usage_percent() const {
        if (total_storage_bytes == 0) return 0.0;
        return 100.0 * used_storage_bytes / total_storage_bytes;
    }

    /**
     * @brief Check if node can accept more offloads
     */
    [[nodiscard]] bool can_accept_offload() const {
        return accepting_offloads &&
               health == NodeHealth::Healthy &&
               active_offload_count < max_concurrent_offloads;
    }
};

/**
 * @brief Offload configuration
 */
struct OffloadConfig {
    // Thresholds
    double memory_threshold_percent = 80.0;     ///< Memory threshold to trigger auto-offload
    double storage_threshold_percent = 85.0;    ///< Storage threshold for auto-offload
    size_t min_byte_difference = 100 * 1024 * 1024; ///< Minimum bytes to offload (100MB)
    size_t max_byte_per_transfer = 1024ULL * 1024 * 1024 * 10; ///< Max bytes per transfer (10GB)

    // Transfer settings
    size_t segment_size = 1 * 1024 * 1024;      ///< Transfer segment size (1MB)
    size_t max_concurrent_transfers = 4;        ///< Maximum parallel transfers
    size_t transfer_buffer_size = 64 * 1024;    ///< Transfer buffer size (64KB)

    // Timeouts
    std::chrono::seconds connect_timeout{30};   ///< Connection timeout
    std::chrono::seconds transfer_timeout{300}; ///< Transfer timeout
    std::chrono::seconds health_check_interval{10}; ///< Health check interval

    // Retry settings
    size_t max_retries = 3;                     ///< Maximum retry attempts
    std::chrono::milliseconds retry_delay{1000}; ///< Initial retry delay
    double retry_backoff_multiplier = 2.0;      ///< Exponential backoff multiplier

    // Behavior
    bool auto_offload = true;                   ///< Enable automatic offloading
    bool compress_transfers = true;             ///< Compress data during transfer
    bool verify_integrity = true;               ///< Verify data integrity after transfer
    bool prefer_local_region = true;            ///< Prefer nodes in same region

    // Node selection
    size_t min_available_storage_bytes = 1024ULL * 1024 * 1024; ///< Minimum available storage on target
    double max_target_cpu_usage = 80.0;         ///< Maximum CPU usage on target node
    double max_target_memory_usage = 85.0;      ///< Maximum memory usage on target node
};

/**
 * @brief Offload progress information
 */
struct OffloadProgress {
    // Byte progress
    size_t total_bytes = 0;                     ///< Total bytes to transfer
    size_t transferred_bytes = 0;               ///< Bytes already transferred
    size_t pending_bytes = 0;                   ///< Bytes pending transfer

    // Segment progress
    size_t segments_total = 0;                  ///< Total segments
    size_t segments_completed = 0;              ///< Completed segments
    size_t segments_failed = 0;                 ///< Failed segments
    size_t segments_pending = 0;                ///< Pending segments

    // Timing
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
    std::chrono::microseconds elapsed{0};

    // Transfer rate
    double bytes_per_second = 0.0;              ///< Current transfer rate
    double average_bytes_per_second = 0.0;      ///< Average transfer rate

    // Status
    std::optional<std::string> error_message;
    std::optional<std::string> current_segment_id;

    /**
     * @brief Calculate progress percentage
     */
    [[nodiscard]] double progress_percent() const {
        if (total_bytes == 0) return 0.0;
        return 100.0 * transferred_bytes / total_bytes;
    }

    /**
     * @brief Calculate estimated time remaining
     */
    [[nodiscard]] std::chrono::seconds estimated_time_remaining() const {
        if (average_bytes_per_second <= 0 || pending_bytes == 0) {
            return std::chrono::seconds{0};
        }
        return std::chrono::seconds{
            static_cast<int64_t>(pending_bytes / average_bytes_per_second)
        };
    }

    /**
     * @brief Check if offload completed successfully
     */
    [[nodiscard]] bool completed_successfully() const {
        return segments_completed == segments_total &&
               segments_total > 0 &&
               !error_message.has_value();
    }
};

/**
 * @brief Offload operation result
 */
struct OffloadResult {
    bool success = false;                       ///< Operation success
    std::optional<std::string> error_message;   ///< Error message if failed
    OffloadProgress final_progress;             ///< Final progress state
    TargetNode target_node;                     ///< Target node used
    std::chrono::steady_clock::time_point completed_at;

    /**
     * @brief Get duration of offload operation
     */
    [[nodiscard]] std::chrono::microseconds duration() const {
        return final_progress.elapsed;
    }
};

/**
 * @brief Offload Manager Interface
 *
 * Abstract interface for database offloading operations.
 * Handles data migration between nodes in a distributed database cluster.
 */
class IOffloadManager {
public:
    virtual ~IOffloadManager() = default;

    // ─────────────────────────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────────────────────────

    /**
     * @brief Set offload configuration
     * @param config OffloadConfig structure
     */
    virtual void set_config(const OffloadConfig& config) = 0;

    /**
     * @brief Get current configuration
     * @return Current OffloadConfig
     */
    [[nodiscard]] virtual OffloadConfig get_config() const = 0;

    // ─────────────────────────────────────────────────────────────────
    // Node Management
    // ─────────────────────────────────────────────────────────────────

    /**
     * @brief Get list of available target nodes
     * @return Vector of available TargetNode
     */
    [[nodiscard]] virtual std::vector<TargetNode> get_available_nodes() const = 0;

    /**
     * @brief Refresh node list from cluster
     * @return true if refresh successful
     */
    virtual bool refresh_nodes() = 0;

    /**
     * @brief Select target node for offload
     * @param node_id Node identifier
     * @return true if node selected successfully
     */
    virtual bool select_target_node(const std::string& node_id) = 0;

    /**
     * @brief Auto-select best target node
     * @return true if a suitable node was found and selected
     */
    virtual bool auto_select_target_node() = 0;

    /**
     * @brief Get currently selected target node
     * @return Optional TargetNode if one is selected
     */
    [[nodiscard]] virtual std::optional<TargetNode> get_current_target() const = 0;

    /**
     * @brief Clear target node selection
     */
    virtual void clear_target_selection() = 0;

    // ─────────────────────────────────────────────────────────────────
    // Offload Operations
    // ─────────────────────────────────────────────────────────────────

    /**
     * @brief Start offload operation
     * @return true if offload started successfully
     */
    virtual bool start_offload() = 0;

    /**
     * @brief Start offload with specific data
     * @param data_ids Vector of data identifiers to offload
     * @return true if offload started successfully
     */
    virtual bool start_offload(const std::vector<std::string>& data_ids) = 0;

    /**
     * @brief Cancel ongoing offload
     * @return true if cancellation successful
     */
    virtual bool cancel_offload() = 0;

    /**
     * @brief Pause ongoing offload
     * @return true if pause successful
     */
    virtual bool pause_offload() = 0;

    /**
     * @brief Resume paused offload
     * @return true if resume successful
     */
    virtual bool resume_offload() = 0;

    // ─────────────────────────────────────────────────────────────────
    // Status
    // ─────────────────────────────────────────────────────────────────

    /**
     * @brief Get current offload status
     * @return Current OffloadStatus
     */
    [[nodiscard]] virtual OffloadStatus get_status() const = 0;

    /**
     * @brief Get current progress
     * @return Current OffloadProgress
     */
    [[nodiscard]] virtual OffloadProgress get_progress() const = 0;

    /**
     * @brief Check if offload is active
     * @return true if offload is in progress
     */
    [[nodiscard]] virtual bool is_active() const = 0;

    /**
     * @brief Get last offload result
     * @return Optional last OffloadResult
     */
    [[nodiscard]] virtual std::optional<OffloadResult> get_last_result() const = 0;

    // ─────────────────────────────────────────────────────────────────
    // Callbacks
    // ─────────────────────────────────────────────────────────────────

    /**
     * @brief Set progress callback
     * @param callback Function called on progress update
     */
    virtual void on_progress(
        std::function<void(const OffloadProgress&)> callback) = 0;

    /**
     * @brief Set completion callback
     * @param callback Function called on completion (success or failure)
     */
    virtual void on_complete(
        std::function<void(const OffloadResult&)> callback) = 0;

    /**
     * @brief Set error callback
     * @param callback Function called on error
     */
    virtual void on_error(
        std::function<void(const std::string&)> callback) = 0;

    /**
     * @brief Set status change callback
     * @param callback Function called on status change
     */
    virtual void on_status_change(
        std::function<void(OffloadStatus, OffloadStatus)> callback) = 0;
};

/**
 * @brief Unique pointer type alias for IOffloadManager
 */
using OffloadManagerPtr = std::unique_ptr<IOffloadManager>;

/**
 * @brief Shared pointer type alias for IOffloadManager
 */
using OffloadManagerSharedPtr = std::shared_ptr<IOffloadManager>;

} // namespace redcomponent::offloading
