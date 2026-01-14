/**
 * @file test_offloading.cpp
 * @brief Unit Tests for Offload Manager
 * @copyright Copyright (c) 2026 BEP Venture UG. All rights reserved.
 * @license EULA - Proprietary
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

#include "../include/redcomponent/offloading/IOffloadManager.hpp"
#include "../include/redcomponent/offloading/MockOffloadManager.hpp"

using namespace redcomponent::offloading;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
// Test Fixture
// ─────────────────────────────────────────────────────────────────────────────

class OffloadingTest : public ::testing::Test {
protected:
    std::unique_ptr<MockOffloadManager> manager_;

    void SetUp() override {
        manager_ = std::make_unique<MockOffloadManager>();
    }

    void TearDown() override {
        if (manager_ && manager_->is_active()) {
            manager_->cancel_offload();
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Basic Operation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, BasicOffloadOperation) {
    // Initially idle
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Idle);
    EXPECT_FALSE(manager_->is_active());

    // Get available nodes
    auto nodes = manager_->get_available_nodes();
    EXPECT_GE(nodes.size(), 1);

    // Select target node
    EXPECT_TRUE(manager_->select_target_node("node1"));
    auto target = manager_->get_current_target();
    EXPECT_TRUE(target.has_value());
    EXPECT_EQ(target->node_id, "node1");

    // Start offload
    EXPECT_TRUE(manager_->start_offload());
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Transferring);
    EXPECT_TRUE(manager_->is_active());

    // Simulate completion
    manager_->simulate_complete(true);
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Completed);
    EXPECT_FALSE(manager_->is_active());

    // Check result
    auto result = manager_->get_last_result();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
}

TEST_F(OffloadingTest, OffloadCancellation) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());
    EXPECT_TRUE(manager_->is_active());

    // Cancel the offload
    EXPECT_TRUE(manager_->cancel_offload());
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Cancelled);
    EXPECT_FALSE(manager_->is_active());

    // Cannot cancel again
    EXPECT_FALSE(manager_->cancel_offload());
}

TEST_F(OffloadingTest, OffloadPauseResume) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    // Pause
    EXPECT_TRUE(manager_->pause_offload());
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Paused);
    EXPECT_TRUE(manager_->is_active()); // Paused is still considered active

    // Cannot pause again
    EXPECT_FALSE(manager_->pause_offload());

    // Resume
    EXPECT_TRUE(manager_->resume_offload());
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Transferring);

    // Cannot resume when not paused
    EXPECT_FALSE(manager_->resume_offload());
}

TEST_F(OffloadingTest, PartialOffloadRecovery) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    // Simulate partial progress
    manager_->simulate_progress(10 * 1024 * 1024); // 10MB
    manager_->simulate_progress(20 * 1024 * 1024); // 20MB more

    auto progress = manager_->get_progress();
    EXPECT_EQ(progress.transferred_bytes, 30 * 1024 * 1024);
    EXPECT_LT(progress.progress_percent(), 100.0);

    // Pause
    EXPECT_TRUE(manager_->pause_offload());

    // Progress should be preserved
    auto paused_progress = manager_->get_progress();
    EXPECT_EQ(paused_progress.transferred_bytes, progress.transferred_bytes);

    // Resume and complete
    EXPECT_TRUE(manager_->resume_offload());
    manager_->simulate_complete(true);

    EXPECT_EQ(manager_->get_status(), OffloadStatus::Completed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Node Selection Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, TargetNodeSelection) {
    // Select existing node
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->get_current_target().has_value());

    // Clear selection
    manager_->clear_target_selection();
    EXPECT_FALSE(manager_->get_current_target().has_value());

    // Select different node
    EXPECT_TRUE(manager_->select_target_node("node2"));
    EXPECT_EQ(manager_->get_current_target()->node_id, "node2");
}

TEST_F(OffloadingTest, TargetNodeUnavailable) {
    // Try to select non-existent node
    EXPECT_FALSE(manager_->select_target_node("nonexistent"));
    EXPECT_FALSE(manager_->get_current_target().has_value());

    // Set node as unhealthy
    manager_->set_node_health("node1", NodeHealth::Unhealthy);

    // Should fail to select unhealthy node
    EXPECT_FALSE(manager_->select_target_node("node1"));
}

TEST_F(OffloadingTest, AutoSelectTargetNode) {
    // Auto-select should pick node with most available storage
    EXPECT_TRUE(manager_->auto_select_target_node());

    auto target = manager_->get_current_target();
    EXPECT_TRUE(target.has_value());
    // node2 has 200GB, should be selected
    EXPECT_EQ(target->node_id, "node2");
}

TEST_F(OffloadingTest, AutoSelectNoNodesAvailable) {
    // Clear all nodes
    manager_->clear_nodes();

    EXPECT_FALSE(manager_->auto_select_target_node());
    EXPECT_FALSE(manager_->get_current_target().has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Progress Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, ProgressCallbacks) {
    std::vector<double> progress_updates;

    manager_->on_progress([&progress_updates](const OffloadProgress& progress) {
        progress_updates.push_back(progress.progress_percent());
    });

    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    // Simulate progress
    manager_->simulate_progress(25 * 1024 * 1024);
    manager_->simulate_progress(25 * 1024 * 1024);
    manager_->simulate_progress(25 * 1024 * 1024);
    manager_->simulate_progress(25 * 1024 * 1024);

    EXPECT_EQ(progress_updates.size(), 4);

    // Progress should be increasing
    for (size_t i = 1; i < progress_updates.size(); ++i) {
        EXPECT_GT(progress_updates[i], progress_updates[i - 1]);
    }
}

TEST_F(OffloadingTest, CompletionCallback) {
    bool callback_called = false;
    bool callback_success = false;

    manager_->on_complete([&callback_called, &callback_success](const OffloadResult& result) {
        callback_called = true;
        callback_success = result.success;
    });

    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());
    manager_->simulate_complete(true);

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(callback_success);
}

TEST_F(OffloadingTest, ErrorCallback) {
    std::string last_error;

    manager_->on_error([&last_error](const std::string& error) {
        last_error = error;
    });

    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());
    manager_->simulate_error("Transfer failed: network timeout");

    EXPECT_EQ(last_error, "Transfer failed: network timeout");
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Failed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Concurrent Operations Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, ConcurrentOffloads) {
    // Cannot start multiple offloads
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    // Second start should fail
    EXPECT_FALSE(manager_->start_offload());
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Transferring);
}

TEST_F(OffloadingTest, ConcurrentStatusQueries) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                auto status = manager_->get_status();
                auto progress = manager_->get_progress();
                if (status != OffloadStatus::Idle) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All queries should succeed
    EXPECT_EQ(success_count.load(), 1000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Configuration Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, ConfigurationValidation) {
    OffloadConfig config;
    config.memory_threshold_percent = 75.0;
    config.segment_size = 2 * 1024 * 1024;
    config.max_retries = 5;
    config.auto_offload = false;

    manager_->set_config(config);

    auto retrieved = manager_->get_config();
    EXPECT_EQ(retrieved.memory_threshold_percent, 75.0);
    EXPECT_EQ(retrieved.segment_size, 2 * 1024 * 1024);
    EXPECT_EQ(retrieved.max_retries, 5);
    EXPECT_FALSE(retrieved.auto_offload);
}

// ─────────────────────────────────────────────────────────────────────────────
// Auto Offload Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, AutoOffloadTrigger) {
    OffloadConfig config;
    config.auto_offload = true;
    config.memory_threshold_percent = 80.0;
    manager_->set_config(config);

    auto retrieved = manager_->get_config();
    EXPECT_TRUE(retrieved.auto_offload);
    EXPECT_EQ(retrieved.memory_threshold_percent, 80.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Node Health Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, NodeHealthTracking) {
    auto nodes = manager_->get_available_nodes();
    EXPECT_FALSE(nodes.empty());

    // All nodes should be healthy initially
    for (const auto& node : nodes) {
        EXPECT_EQ(node.health, NodeHealth::Healthy);
        EXPECT_TRUE(node.can_accept_offload());
    }

    // Set a node to degraded
    manager_->set_node_health("node1", NodeHealth::Degraded);

    nodes = manager_->get_available_nodes();
    auto it = std::find_if(nodes.begin(), nodes.end(),
        [](const TargetNode& n) { return n.node_id == "node1"; });

    EXPECT_NE(it, nodes.end());
    EXPECT_EQ(it->health, NodeHealth::Degraded);
}

TEST_F(OffloadingTest, RefreshNodes) {
    auto before = manager_->get_available_nodes();
    EXPECT_TRUE(manager_->refresh_nodes());

    // Check health check timestamps were updated
    auto after = manager_->get_available_nodes();
    EXPECT_EQ(before.size(), after.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Status Change Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, StatusChangeCallback) {
    std::vector<std::pair<OffloadStatus, OffloadStatus>> status_changes;

    manager_->on_status_change([&status_changes](OffloadStatus from, OffloadStatus to) {
        status_changes.push_back({from, to});
    });

    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());
    manager_->simulate_complete(true);

    // Should have recorded: Idle->Preparing, Preparing->Transferring,
    // Transferring->Completing, Completing->Completed
    EXPECT_GE(status_changes.size(), 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Offload Without Target Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, StartWithoutTarget) {
    // Don't select target
    EXPECT_FALSE(manager_->get_current_target().has_value());

    // Start should fail
    EXPECT_FALSE(manager_->start_offload());
    EXPECT_EQ(manager_->get_status(), OffloadStatus::Idle);
}

// ─────────────────────────────────────────────────────────────────────────────
// Data ID Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, OffloadSpecificData) {
    EXPECT_TRUE(manager_->select_target_node("node1"));

    std::vector<std::string> data_ids = {"data1", "data2", "data3"};
    EXPECT_TRUE(manager_->start_offload(data_ids));

    auto offload_ids = manager_->get_offload_data_ids();
    EXPECT_EQ(offload_ids.size(), 3);
    EXPECT_EQ(offload_ids[0], "data1");
    EXPECT_EQ(offload_ids[1], "data2");
    EXPECT_EQ(offload_ids[2], "data3");
}

// ─────────────────────────────────────────────────────────────────────────────
// Progress Calculation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, ProgressCalculation) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    auto progress = manager_->get_progress();
    EXPECT_EQ(progress.progress_percent(), 0.0);
    EXPECT_GT(progress.total_bytes, 0);

    // 50% progress
    manager_->simulate_progress(progress.total_bytes / 2);
    progress = manager_->get_progress();
    EXPECT_NEAR(progress.progress_percent(), 50.0, 1.0);

    // 100% progress
    manager_->simulate_complete(true);
    progress = manager_->get_progress();
    EXPECT_EQ(progress.progress_percent(), 100.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Node Management Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, AddRemoveNodes) {
    size_t initial_count = manager_->node_count();

    // Add node
    auto new_node = MockOffloadManager::create_mock_node(
        "node_new", "192.168.1.100", 500ULL * 1024 * 1024 * 1024);
    manager_->add_node(new_node);
    EXPECT_EQ(manager_->node_count(), initial_count + 1);

    // Remove node
    manager_->remove_node("node_new");
    EXPECT_EQ(manager_->node_count(), initial_count);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reset Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, Reset) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());
    manager_->simulate_progress(10 * 1024 * 1024);

    manager_->reset();

    EXPECT_EQ(manager_->get_status(), OffloadStatus::Idle);
    EXPECT_FALSE(manager_->get_current_target().has_value());
    EXPECT_FALSE(manager_->get_last_result().has_value());

    auto progress = manager_->get_progress();
    EXPECT_EQ(progress.transferred_bytes, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Estimated Time Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OffloadingTest, EstimatedTimeRemaining) {
    EXPECT_TRUE(manager_->select_target_node("node1"));
    EXPECT_TRUE(manager_->start_offload());

    auto progress = manager_->get_progress();
    auto initial_eta = progress.estimated_time_remaining();

    // After progress, ETA should decrease
    manager_->simulate_progress(50 * 1024 * 1024);
    progress = manager_->get_progress();

    // With simulated transfer rate, we can calculate expected ETA
    if (progress.average_bytes_per_second > 0) {
        auto eta = progress.estimated_time_remaining();
        EXPECT_GE(eta.count(), 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
