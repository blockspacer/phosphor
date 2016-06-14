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

#include "phosphor/tools/export.h"

namespace phosphor {
    namespace tools {
        JSONExport::JSONExport(const TraceBuffer& _buffer)
            : buffer(_buffer),
              it(_buffer.begin()) {
        }

        size_t JSONExport::read(char* out, size_t length) {
            std::string event_json;
            size_t cursor = 0;

            while(cursor < length && !(state == State::dead && cache.size() == 0)) {
                if(cache.size() > 0) {
                    size_t copied = cache.copy((out + cursor),
                                               length - cursor);
                    cache.erase(0, copied);
                    cursor += copied;

                    if(cursor >= length) {
                        break;
                    }
                }
                switch (state) {
                    case State::opening:
                        cache = "{\n"
                                "  \"traceEvents\": [\n";
                        state = State::first_event;
                        break;
                    case State::other_events:
                        cache += ",\n";
                    case State::first_event:
                        event_json = it->to_json();
                        ++it;
                        cache += event_json;
                        state = State::other_events;
                        if(it == buffer.end()) {
                            state = State::footer;
                        }
                        break;
                    case State::footer:
                        cache = "\n    ]\n"
                                "}\n";
                        state = State::dead;
                        break;
                    case State::dead:
                        break;
                }
            }
            return cursor;
        }

        std::string JSONExport::read(size_t length) {
            std::string out;
            out.resize(length, '\0');
            out.resize(read(&out[0], length));
            return out;
        }
    }
}