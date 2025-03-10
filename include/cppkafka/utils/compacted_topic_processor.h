/*
 * Copyright (c) 2017, Matias Fontanini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef CPPKAFKA_COMPACTED_TOPIC_PROCESSOR_H
#define CPPKAFKA_COMPACTED_TOPIC_PROCESSOR_H

#include <functional>
#include <string>
#include <map>
#include <set>
#include <optional>
#include "../buffer.h"
#include "../consumer.h"
#include "../macros.h"

namespace cppkafka {
/**
 * \brief Events generated by a CompactedTopicProcessor
 */
template <typename Key, typename Value>
class CompactedTopicEvent {
public:
    /**
     * \brief Event type enum
     */
    enum EventType {
        SET_ELEMENT,
        DELETE_ELEMENT,
        CLEAR_ELEMENTS,
        REACHED_EOF
    };

    /**
     * Constructs an instance providing only a type
     */
    CompactedTopicEvent(EventType type, std::string topic, int partition);

    /**
     * Constructs an instance providing a type and a key
     */
    CompactedTopicEvent(EventType type, std::string topic, int partition, Key key);

    /**
     * Constructs an instance providing a type, a key and a value
     */
    CompactedTopicEvent(EventType type, std::string topic, int partition, Key key, Value value);

    /**
     * Gets the event type
     */
    EventType get_type() const;

    /**
     * Gets the topic that generated this event
     */
    const std::string& get_topic() const;

    /**
     * Gets the partition that generated this event
     */
    int get_partition() const;

    /**
     * \brief Gets the event key
     *
     * Note that it's only valid to call this method if the event type is either:
     *
     * * SET_ELEMENT
     * * DELETE_ELEMENT
     */
    const Key& get_key() const;

    /**
     * \brief Gets the event value
     *
     * Note that it's only valid to call this method if the event type is SET_ELEMENT
     */
    const Value& get_value() const;
private:
    EventType type_;
    std::string topic_;
    int partition_;
    std::optional<Key> key_;
    std::optional<Value> value_;
};

template <typename Key, typename Value>
class CompactedTopicProcessor {
public:
    /**
     * The type of events generated by this processor
     */
    using Event = CompactedTopicEvent<Key, Value>;

    /**
     * Callback used for decoding key objects
     */
    using KeyDecoder = std::function<std::optional<Key>(const Buffer&)>;

    /**
     * Callback used for decoding value objects
     */
    using ValueDecoder = std::function<std::optional<Value>(const Key& key, const Buffer&)>;

    /**
     * Callback used for event handling
     */
    using EventHandler = std::function<void(Event)>;

    /**
     * Callback used for error handling
     */
    using ErrorHandler = std::function<void(Message)>;

    /**
     * \brief Constructs an instance
     */
    CompactedTopicProcessor(Consumer& consumer);
    ~CompactedTopicProcessor();

    CompactedTopicProcessor(const CompactedTopicProcessor&) = delete;
    CompactedTopicProcessor(CompactedTopicProcessor&&) = delete;
    CompactedTopicProcessor& operator=(const CompactedTopicProcessor&) = delete;
    CompactedTopicProcessor& operator=(CompactedTopicProcessor&&) = delete;

    /**
     * \brief Sets the key decoder callback
     */
    void set_key_decoder(KeyDecoder callback);

    /**
     * \brief Sets the value decoder callback
     */
    void set_value_decoder(ValueDecoder callback);

    /**
     * \brief Sets the event handler callback
     */
    void set_event_handler(EventHandler callback);

    /**
     * \brief Sets the error handler callback
     */
    void set_error_handler(ErrorHandler callback);

    /** 
     * \brief Processes the next event
     */
    void process_event();
private:
    void on_assignment(TopicPartitionList& topic_partitions);

    Consumer& consumer_;
    KeyDecoder key_decoder_;
    ValueDecoder value_decoder_;
    EventHandler event_handler_;
    ErrorHandler error_handler_;
    std::map<TopicPartition, int64_t> partition_offsets_;
    Consumer::AssignmentCallback original_assignment_callback_;
};

// CompactedTopicEvent

template <typename K, typename V>
CompactedTopicEvent<K, V>::CompactedTopicEvent(EventType type, std::string topic, int partition)
: type_(type), topic_(std::move(topic)), partition_(partition) {

}

template <typename K, typename V>
CompactedTopicEvent<K, V>::CompactedTopicEvent(EventType type, std::string topic, int partition,
                                               K key)
: type_(type), topic_(std::move(topic)), partition_(partition), key_(std::move(key)) {

}

template <typename K, typename V>
CompactedTopicEvent<K, V>::CompactedTopicEvent(EventType type, std::string topic, int partition,
                                               K key, V value)
: type_(type), topic_(std::move(topic)), partition_(partition), key_(std::move(key)),
  value_(std::move(value)) {

}

template <typename K, typename V>
typename CompactedTopicEvent<K, V>::EventType CompactedTopicEvent<K, V>::get_type() const {
    return type_;
}

template <typename K, typename V>
const std::string& CompactedTopicEvent<K, V>::get_topic() const {
    return topic_;
}

template <typename K, typename V>
int CompactedTopicEvent<K, V>::get_partition() const {
    return partition_;
}

template <typename K, typename V>
const K& CompactedTopicEvent<K, V>::get_key() const {
    return *key_;
}

template <typename K, typename V>
const V& CompactedTopicEvent<K, V>::get_value() const {
    return *value_;
}

// CompactedTopicProcessor

template <typename K, typename V>
CompactedTopicProcessor<K, V>::CompactedTopicProcessor(Consumer& consumer) 
: consumer_(consumer) {
    // Save the current assignment callback and assign ours
    original_assignment_callback_ = consumer_.get_assignment_callback();
    consumer_.set_assignment_callback([&](TopicPartitionList& topic_partitions) {
        on_assignment(topic_partitions);
    });
}

template <typename K, typename V>
CompactedTopicProcessor<K, V>::~CompactedTopicProcessor() {
    // Restore previous assignment callback
    consumer_.set_assignment_callback(original_assignment_callback_);
}

template <typename K, typename V>
void CompactedTopicProcessor<K, V>::set_key_decoder(KeyDecoder callback) {
    key_decoder_ = std::move(callback);
}

template <typename K, typename V>
void CompactedTopicProcessor<K, V>::set_value_decoder(ValueDecoder callback) {
    value_decoder_ = std::move(callback);
}

template <typename K, typename V>
void CompactedTopicProcessor<K, V>::set_event_handler(EventHandler callback) {
    event_handler_ = std::move(callback);
}

template <typename K, typename V>
void CompactedTopicProcessor<K, V>::set_error_handler(ErrorHandler callback) {
    error_handler_ = std::move(callback);
}

template <typename Key, typename Value>
void CompactedTopicProcessor<Key, Value>::process_event() {
    Message message = consumer_.poll();
    if (message) {
        if (!message.get_error()) {
            std::optional<Key> key = key_decoder_(message.get_key());
            if (key) {
                if (message.get_payload()) {
                    std::optional<Value> value = value_decoder_(*key, message.get_payload());
                    if (value) {
                        // If there's a payload and we managed to parse the value, generate a
                        // SET_ELEMENT event
                        event_handler_({ Event::SET_ELEMENT, message.get_topic(),
                                         message.get_partition(), *key, std::move(*value) });
                    }
                }
                else {
                    // No payload, generate a DELETE_ELEMENT event
                    event_handler_({ Event::DELETE_ELEMENT, message.get_topic(),
                                     message.get_partition(), *key });
                }
            }
            // Store the offset for this topic/partition
            TopicPartition topic_partition(message.get_topic(), message.get_partition());
            partition_offsets_[topic_partition] = message.get_offset();
        }
        else {
            if (message.is_eof()) {
                event_handler_({ Event::REACHED_EOF, message.get_topic(),
                                 message.get_partition() });
            }
            else if (error_handler_) {
                error_handler_(std::move(message));
            }
        }
    }
}

template <typename K, typename V>
void CompactedTopicProcessor<K, V>::on_assignment(TopicPartitionList& topic_partitions) {
    if (original_assignment_callback_) {
        original_assignment_callback_(topic_partitions);
    }
    std::set<TopicPartition> partitions_found;
    // See if we already had an assignment for any of these topic/partitions. If we do,
    // then restore the offset following the last one we saw
    for (TopicPartition& topic_partition : topic_partitions) {
        auto iter = partition_offsets_.find(topic_partition);
        if (iter != partition_offsets_.end()) {
            topic_partition.set_offset(iter->second);
        }
        // Populate this set
        partitions_found.insert(topic_partition);
    }
    // Clear our cache: remove any entries for topic/partitions that aren't assigned to us now.
    // Emit a CLEAR_ELEMENTS event for each topic/partition that is gone
    auto iter = partition_offsets_.begin();
    while (iter != partition_offsets_.end()) {
        const TopicPartition& topic_partition = iter->first;
        if (partitions_found.count(topic_partition) == 0) {
            event_handler_({ Event::CLEAR_ELEMENTS, topic_partition.get_topic(),
                             topic_partition.get_partition() });
            iter = partition_offsets_.erase(iter);
        }
        else {
            ++iter;
        }
    }
}

} // cppkafka

#endif // CPPKAFKA_COMPACTED_TOPIC_PROCESSOR_H
