/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2016 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "phosphor/trace_log.h"
#include "phosphor/trace_buffer.h"

using namespace phosphor;

TEST(TraceConfigTest, defaultConstructor) {
    TraceConfig config;
}

TEST(TraceConfigTest, createFixed) {
    TraceConfig config(BufferMode::fixed, 1337);

    /* Check that we get a fixed buffer factory */
    auto bufferA = make_fixed_buffer(0, 0);
    auto& bufferARef = *bufferA;
    auto bufferB = config.getBufferFactory()(0, 0);
    auto& bufferBRef = *bufferB;

    EXPECT_EQ(typeid(bufferARef),
              typeid(bufferBRef));

    EXPECT_EQ(1337, config.getBufferSize());
    EXPECT_EQ(BufferMode::fixed, config.getBufferMode());
}

TEST(TraceConfigTest, createCustom) {
    TraceConfig config(make_fixed_buffer, 1337);

    /* Check that we get a fixed buffer factory */
    auto bufferA = make_fixed_buffer(0, 0);
    auto& bufferARef = *bufferA;
    auto bufferB = config.getBufferFactory()(0, 0);
    auto& bufferBRef = *bufferB;

    EXPECT_EQ(typeid(bufferARef),
              typeid(bufferBRef));

    EXPECT_EQ(BufferMode::custom, config.getBufferMode());
}

TEST(TraceConfigTest, createModeErrors) {
    EXPECT_THROW(TraceConfig(BufferMode::ring, 1337), std::invalid_argument);
    EXPECT_THROW(TraceConfig(BufferMode::custom, 1337), std::invalid_argument);
    EXPECT_THROW(TraceConfig(static_cast<BufferMode>(0xFF), 1337),
                 std::invalid_argument);
}

class MockTraceLog : public TraceLog {
    friend class TraceLogTest;
};

class TraceLogTest : public testing::Test {
public:
    static const int min_buffer_size = sizeof(TraceChunk);

    TraceLogTest() {

    }
    virtual ~TraceLogTest() = default;

    void start_basic() {
        trace_log.start(TraceConfig(BufferMode::fixed,
                                    min_buffer_size));
    }

    void log_event() {
        trace_log.logEvent("category", "name", TraceEvent::Type::Instant,
                           0, 0, 0);
    }
protected:
    MockTraceLog trace_log;
};

TEST_F(TraceLogTest, smallBufferThrow) {
    EXPECT_THROW(trace_log.start(TraceConfig(BufferMode::fixed, 0)),
                 std::invalid_argument);
    EXPECT_NO_THROW(trace_log.start(TraceConfig(BufferMode::fixed,
                                                min_buffer_size)));
}

TEST_F(TraceLogTest, isEnabled) {
    EXPECT_FALSE(trace_log.isEnabled());
    start_basic();
    EXPECT_TRUE(trace_log.isEnabled());
    trace_log.stop();
    EXPECT_FALSE(trace_log.isEnabled());
}

TEST_F(TraceLogTest, EnabledBufferGetThrow) {
    EXPECT_EQ(nullptr, trace_log.getBuffer().get());
    start_basic();
    EXPECT_THROW(trace_log.getBuffer(), std::logic_error);
    trace_log.stop();
    EXPECT_NE(nullptr, trace_log.getBuffer().get());
}

TEST_F(TraceLogTest, bufferGenerationCheck) {
    start_basic();
    trace_log.stop();
    buffer_ptr buffer = trace_log.getBuffer();
    EXPECT_EQ(0, buffer->getGeneration());
    start_basic();
    trace_log.stop();
    buffer = trace_log.getBuffer();
    EXPECT_EQ(1, buffer->getGeneration());
}

TEST_F(TraceLogTest, logTillFullAndEvenThen) {
    trace_log.start(TraceConfig(BufferMode::fixed,
                                min_buffer_size * 4));

    while(trace_log.isEnabled()) {
        log_event();
    }
    log_event();
}

TEST_F(TraceLogTest, logTillFullAndEvenThenButReload) {
    trace_log.start(TraceConfig(BufferMode::fixed,
                                min_buffer_size * 4));

    while(trace_log.isEnabled()) {
        log_event();
    }
    trace_log.start(TraceConfig(BufferMode::fixed,
                                min_buffer_size * 4));

    while(trace_log.isEnabled()) {
        log_event();
    }
}

TEST_F(TraceLogTest, logTillFullThreaded) {
    const int thread_count = 8;
    std::vector<std::thread> threads;
    trace_log.start(TraceConfig(BufferMode::fixed,
                                min_buffer_size * thread_count * 4));

    for(int i = 0; i < thread_count; ++i) {
        threads.emplace_back([this]() {
            while (trace_log.isEnabled()) {
                log_event();
            }
        });
    }
    for(std::thread& thread : threads) {
        thread.join();
    };
}


TEST(TraceLogStaticTest, getInstance) {
    EXPECT_EQ(&TraceLog::getInstance(), &TraceLog::getInstance());
}

/*
 * It's slightly awkward to test if this actually does anything,
 * so just run the code to check for memory leaks etc.
 */
TEST(TraceLogStaticTest, registerDeRegister) {
    TraceLog trace_log;
    trace_log.start(TraceConfig(BufferMode::fixed, sizeof(TraceChunk)));

    EXPECT_THROW(TraceLog::deregisterThread(trace_log), std::logic_error);
    TraceLog::registerThread();
    EXPECT_NO_THROW(TraceLog::deregisterThread(trace_log));
}

TEST(TraceLogStaticTest, registerDeRegisterWithChunk) {
    TraceLog trace_log;
    trace_log.start(TraceConfig(BufferMode::fixed, sizeof(TraceChunk)));
    TraceLog::registerThread();
    trace_log.logEvent("category", "name", TraceEvent::Type::Instant, 0, 0, 0);
    EXPECT_NO_THROW(TraceLog::deregisterThread(trace_log));
}