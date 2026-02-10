/*
================================================================================
SyncBuffer - High-Performance Multi-Sensor Temporal Synchronization

Author: Pablo Bustos
Date: April 8th, 2025

Description:
------------
SyncBuffer is a C++23 class template designed for real-time synchronization
of multiple asynchronous sensors (e.g., cameras, lidars, radars).
It pairs samples from different sensors based on their timestamps,
within flexible user-defined temporal constraints.

Technologies used:
-------------------
- C++23 features (concepts, structured bindings, constexpr lambdas, tuple_apply)
- Lock-free circular buffers for real-time safe data storage
- Zero-copy read access and move-efficient input handling
- Compile-time type safety checks
- STL-only, no external dependencies

Key Features:
-------------
- Variadic template: handles any number and type of sensor streams
- Dynamic control over synchronization constraints:
    * Maximum spread between synchronized timestamps
    * Maximum tolerated delay (relaxation factor)
    * Maximum candidate staleness
- Runtime-configurable relaxation policy: graceful degradation under delay
- Hard real-time behavior: deterministic and efficient
- Extensible and lightweight

--------------------------------------------------------------------------------

Constructor:
------------
SyncBuffer(size_t buffer_capacity,
           double max_allowed_spread_usec,
           double timeout_usec,
           double relaxation_factor = 1.33,
           double max_relaxed_spread_usec = 100000.0,
           double max_candidate_age_usec = 100000.0);

Arguments:
- buffer_capacity: Capacity of each internal circular buffer
- max_allowed_spread_usec: Strict maximum timestamp spread (µs)
- timeout_usec: Reserved for future timeout-based features
- relaxation_factor: Spread growth slope when candidate gets old
- max_relaxed_spread_usec: Maximum spread allowed under relaxation
- max_candidate_age_usec: Maximum age of acceptable samples (relative to newest)

--------------------------------------------------------------------------------

Example Usage:
--------------

using LidarImage = RoboCompLidar3D::TDataImage;
using CameraImage = RoboCompCamera360RGB::TImage;

SyncBuffer<std::pair<LidarImage, LidarImage>,
           std::pair<CameraImage, CameraImage>>
    sync_buffer(
        30,          // buffer capacity per sensor
        10000.0,     // initial max spread (10 ms)
        100000.0,    // timeout (future)
        1.5,         // relaxation factor
        50000.0,     // max relaxed spread (50 ms)
        100000.0     // max candidate age (100 ms)
    );

// Push sensor data
sync_buffer.push<0>(lidar_sample, [](const LidarImage& in) { return in; });
sync_buffer.push<1>(camera_sample, [](const CameraImage& in) { return in; });

// Read synchronized tuple
if (auto synced = sync_buffer.read())
{
    auto [lidar, camera] = *synced;
    // Use synchronized data...
}

--------------------------------------------------------------------------------

Advanced Tips:
--------------
- **Small buffer** ➔ lower latency, risk of missing old samples
- **Large buffer** ➔ smoother matching, slightly more memory
- **Tune max spread**:
  * Hardware-triggered sensors ➔ 1000-5000 µs
  * Free-running sensors ➔ 5000-10000 µs
- **Relaxation** allows smooth degradation under delays
- **Time units**: input timestamps expected in **milliseconds**, internally scaled to **microseconds**

--------------------------------------------------------------------------------

Future Extensions:
------------------
- Predictive synchronization
- Timeout-aware synchronization strategies
- Multi-base search for more optimal matching

--------------------------------------------------------------------------------

License:
--------
MIT License

================================================================================
*/


#pragma once

#include <vector>
#include <tuple>
#include <optional>
#include <limits>
#include <atomic>
#include <memory>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <chrono>
#include <array>

//// CONCEPTS /////////////////////////////////////////////////

template<typename T>
concept HasTimestamp = requires(T a) {
    { a.timestamp } -> std::convertible_to<long double>;
};

template<typename T>
concept Copyable = std::copy_constructible<T>;

//////////////////////////////////////////////////////////////

template<typename T>
class LockFreeCircularBuffer
{
public:
    using value_type = T;
    LockFreeCircularBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity) {}

    void push(const T& item)
    {
        const uint64_t head = head_.load(std::memory_order_relaxed);
        buffer_[head % capacity_] = item;
        head_.store(head + 1, std::memory_order_release);
    }

    void push(T&& item)
    {
        const uint64_t head = head_.load(std::memory_order_relaxed);
        buffer_[head % capacity_] = std::move(item);
        head_.store(head + 1, std::memory_order_release);
    }

    size_t size() const
    {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head >= tail) ? head - tail : (head + 2 * capacity_) - tail;
    }

    std::optional<std::reference_wrapper<const T>> peek_latest(size_t idx_from_end) const
    {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t size = this->size();
        if (idx_from_end >= size)
        {
            qDebug() << "LockFreeCircularBuffer - Index out of bounds in peek_latest. Returning nullopt.";
            return std::nullopt;
        }
        return std::cref(buffer_[(head + 2 * capacity_ - 1 - idx_from_end) % capacity_]);
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    std::atomic<std::int64_t> head_{0}; // 500M years to overflow
    std::atomic<std::int64_t> tail_{0};
};

//////////////////////////////////////////////////////////////

template<typename... SensorPairs>
class SyncBuffer
{
    static_assert((HasTimestamp<typename SensorPairs::first_type> && ...),
                  "All input types must have a .timestamp convertible to long double");

    static_assert((Copyable<typename SensorPairs::first_type> && ...),
                  "All input types must be copyable");

    static_assert((Copyable<typename SensorPairs::second_type> && ...),
                  "All output types must be copyable");

public:
    using InputTuple = std::tuple<typename SensorPairs::first_type...>;
    using OutputTuple = std::tuple<typename SensorPairs::second_type...>;

    /**
     * \brief Constructor for SyncBuffer
     *
     * \param buffer_capacity Capacity of each internal circular buffer.
     * \param max_allowed_spread_usec Maximum allowed timestamp spread in microseconds.
     * \param timeout_usec Reserved for future timeout-based features.
     * \param relaxation_factor Spread growth slope when candidate gets old (default: 1.33).
     * \param max_relaxed_spread_usec Maximum spread allowed under relaxation in microseconds (default: 100000.0).
     * \param max_candidate_age_usec Maximum age of acceptable samples relative to the newest in microseconds (default: 100000.0).
     */
    SyncBuffer(size_t buffer_capacity,
               double max_allowed_spread_usec,
               double timeout_usec,
               double relaxation_factor = 1.33,
               double max_relaxed_spread_usec = 70000.0,
               double max_candidate_age_usec = 100000.0)
        : buffers_(std::make_unique<LockFreeCircularBuffer<typename SensorPairs::first_type>>(buffer_capacity)...),
          transforms_(std::make_tuple(std::function<typename SensorPairs::second_type(const typename SensorPairs::first_type&)>()...)),
          max_allowed_spread_(max_allowed_spread_usec),
          timeout_(timeout_usec),
          relaxation_factor_(relaxation_factor),
          max_relaxed_spread_usec_(max_relaxed_spread_usec),
          max_candidate_age_usec_(max_candidate_age_usec)
    {}

    template<size_t SensorIdx, typename InputType, typename TransformFunc>
    void push(InputType&& input, TransformFunc&& transform)
    {
        auto& buffer = std::get<SensorIdx>(buffers_);
        auto& trans_func = std::get<SensorIdx>(transforms_);
        if (!trans_func)
            trans_func = std::forward<TransformFunc>(transform);

        if (auto last = buffer->peek_latest(0); last.has_value())
        {
            if (std::abs(last->get().timestamp * timestamp_scale_factor - input.timestamp * timestamp_scale_factor) < duplicate_timestamp_epsilon)
                return; // skip duplicate
        }
        buffer->push(std::forward<InputType>(input));
    }

    template<size_t SensorIdx, typename InputType>
    void push(InputType&& input)
    {
        auto& buffer = std::get<SensorIdx>(buffers_);
        auto& trans_func = std::get<SensorIdx>(transforms_);
        if (!trans_func)
        {
            using OutputType = typename std::tuple_element_t<SensorIdx, std::tuple<typename SensorPairs::second_type...>>;
            if constexpr (std::is_same_v<std::decay_t<InputType>, OutputType>)
                trans_func = [](const InputType& in) { return in; };
            else
                static_assert(std::is_same_v<InputType, OutputType>, "Missing transform lambda: InputType != OutputType");
        }

        if (auto last = buffer->peek_latest(0); last.has_value())
        {
            if (std::abs(last->get().timestamp * timestamp_scale_factor - input.timestamp * timestamp_scale_factor) < duplicate_timestamp_epsilon)
                return; // skip duplicate
        }
        buffer->push(std::forward<InputType>(input));
    }

    std::optional<OutputTuple> read(std::optional<size_t> max_samples_to_consider = std::nullopt)
    {
        const auto now = now_microseconds();

        auto timestamp_arrays = std::apply(
            [&](const auto&... buffers)
            {
                return std::make_tuple(preload_timestamps(buffers, max_samples_to_consider)...);
            },
            buffers_);

        if (const bool all_have_data = std::apply([](const auto&... arrs) { return (... && (!arrs.empty())); }, timestamp_arrays); !all_have_data)
        {
            qDebug() << "SyncBuffer - Empty buffers in read(). Returning nullopt.";
            return std::nullopt;
        }

        const double newest_sensor_timestamp = [&]
        {
            double max_ts = -std::numeric_limits<double>::infinity();
            std::apply([&](const auto&... arrs)
            {
                ((max_ts = std::max(max_ts, arrs.front().first)), ...);
            }, timestamp_arrays);
            return max_ts;
        }();

        double best_spread = std::numeric_limits<double>::max();
        std::array<size_t, sizeof...(SensorPairs)> best_indices{};
        bool found = false;

        const auto& base_array = std::get<0>(timestamp_arrays);

        for (const auto& [base_time, base_idx] : base_array)
        {
            std::array<size_t, sizeof...(SensorPairs)> current_indices{base_idx};
            std::array<double, sizeof...(SensorPairs)> current_times{base_time};

            [&]<size_t... Is>(std::index_sequence<Is...>)
            {
                (..., ([&]
                {
                    if constexpr (Is > 0)
                    {
                        const auto& arr = std::get<Is>(timestamp_arrays);
                        double min_diff = std::numeric_limits<double>::max();
                        size_t closest_idx = 0;
                        for (const auto& [t, idx] : arr)
                        {
                            double diff = std::abs(t - base_time);
                            if (diff < min_diff)
                            {
                                min_diff = diff;
                                closest_idx = idx;
                            }
                        }
                        current_indices[Is] = closest_idx;
                        current_times[Is] = base_time + min_diff;
                    }
                }()));
            }(std::make_index_sequence<sizeof...(SensorPairs)>{});

            auto [min_it, max_it] = std::minmax_element(current_times.begin(), current_times.end());
            const double spread = *max_it - *min_it;
            const double candidate_age = newest_sensor_timestamp - base_time;

            double allowed_spread = max_allowed_spread_ + relaxation_factor_ * candidate_age;
            allowed_spread = std::min(allowed_spread, max_relaxed_spread_usec_);

            if (spread <= allowed_spread)
            {
                if (candidate_age <= max_candidate_age_usec_)
                {
                    best_spread = spread;
                    best_indices = current_indices;
                    found = true;
                    break;
                }
                else if (!found || spread < best_spread)
                {
                    best_spread = spread;
                    best_indices = current_indices;
                    found = true;
                }
            }
        }

        if (!found)
        {
            qDebug() << "NO MATCH FOUND.";
            return std::nullopt;
        }

        const double selected_base_timestamp = base_array[best_indices[0]].first;
        const double final_candidate_age = newest_sensor_timestamp - selected_base_timestamp;
        if (final_candidate_age > max_candidate_age_usec_)
        {
            qDebug() << "Selected candidate too old:" << final_candidate_age << "usec; maximum allowed is" << max_candidate_age_usec_ << "usec.";
            return std::nullopt;
        }

        OutputTuple output_tuple;

        [&]<size_t... Is>(std::index_sequence<Is...>) // Generic lambda with a non-type template parameter pack `Is...`.
        {
            // Fold expression to iterate over all indices in the parameter pack `Is...`.
            (..., ([&] // Comma fold expression ensures all lambda calls are evaluated in sequence.
            {
                // Access the buffer corresponding to the current index `Is`.
                auto& buffer = std::get<Is>(buffers_);

                // Retrieve the element at the index `best_indices[Is]` from the buffer.
                auto opt_input = buffer->peek_latest(best_indices[Is]);
                if (!opt_input) // If the element is not found, throw an exception.
                    throw std::runtime_error("Invalid peek in SyncBuffer: index mismatch.");

                // Access the transformation function for the current index `Is`.
                auto& transform = std::get<Is>(transforms_);

                // Apply the transformation function to the retrieved element and store the result in the output tuple.
                std::get<Is>(output_tuple) = transform(opt_input.value().get());
            }())); // Invoke the inner lambda immediately for each index in the fold expression.
        }(std::make_index_sequence<sizeof...(SensorPairs)>{}); // Invoke the outer lambda with a compile-time sequence of indices.

        last_success_timestamp_ = now;
        return output_tuple;
    }

private:
    std::tuple<std::unique_ptr<LockFreeCircularBuffer<typename SensorPairs::first_type>>...> buffers_;
    std::tuple<std::function<typename SensorPairs::second_type(const typename SensorPairs::first_type&)>...> transforms_;

    double max_allowed_spread_;
    double timeout_;
    double relaxation_factor_;
    double max_relaxed_spread_usec_;
    double max_candidate_age_usec_;
    double last_success_timestamp_ = 0.0;

    static constexpr double timestamp_scale_factor = 1000.0; // ms -> us
    static constexpr double duplicate_timestamp_epsilon = 1000.0; // 1ms

    static double now_microseconds()
    {
        using namespace std::chrono;
        return duration_cast<std::chrono::microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    template<typename BufferPtr>
    auto preload_timestamps(const BufferPtr& buffer_ptr, std::optional<size_t> max_samples)
    {
        const size_t available = buffer_ptr->size();
        const size_t to_read = max_samples.has_value() ? std::min(max_samples.value(), available) : available;
        std::vector<std::pair<double, size_t>> timestamps;
        timestamps.reserve(to_read);

        for (size_t j = 0; j < to_read; ++j)
        {
            if (auto opt = buffer_ptr->peek_latest(j); opt.has_value())
                timestamps.emplace_back(opt.value().get().timestamp * timestamp_scale_factor, j);
        }
        return timestamps;
    }
};
