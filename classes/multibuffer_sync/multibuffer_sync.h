/*
================================================================================
SyncBuffer - High-performance multi-sensor temporal synchronization

Author: [Pablo Bustos]
Date: [8/04/2025]

Description:
------------
SyncBuffer is a generic, high-performance C++23 class designed to synchronize
data streams from multiple asynchronous sensors (e.g., cameras, lidars, radars).
It selects, at each read(), the set of input samples with the closest timestamps
across all sensors within a user-defined maximum allowed spread.

Technologies used:
-------------------
- C++23 features: concepts, structured bindings, constexpr lambdas, std::tuple_apply.
- Lock-free circular buffers for thread-safe, low-latency data storage.
- Binary search (std::lower_bound) for fast matching.
- Move semantics and reference wrappers for zero-copy efficiency.
- Static compile-time checks with custom concepts for data safety.
- Optional parallel transformation of input to output types.

Key features:
-------------
- Variadic design: supports arbitrary number and types of sensors.
- Per-sensor customizable transform lambdas.
- Hard real-time behavior with user-defined max spread and timeout.
- Automatic fallback if no good match is found.
- Extensible and lightweight (no dependencies beyond STL).

Example usage:
--------------

using LidarImage = RoboCompLidar3D::TDataImage;
using CameraImage = RoboCompCamera360RGB::TImage;  (assumes these types have a timestamp field in milliseconds)

// Create a SyncBuffer for Lidar + Camera
SyncBuffer<std::pair<LidarImage, ProcessedLidar>, std::pair<CameraImage, ProcessedCamera>>
        sync_buffer(30 //buffer capacity//, 10000.0 //max spread in µs//, 100000.0 //timeout in µs//);

// Push new data
sync_buffer.push<0>(new_lidar_data, [](const LidarImage& in) { return process_lidar(in); });
sync_buffer.push<1>(new_camera_data, [](const CameraImage& in) { return process_camera(in); });

// Read synchronized data
if (auto synced = sync_buffer.read())
{
    auto [processed_lidar, processed_camera] = *synced;
    // Use synchronized and transformed data...
}

================================================================================
Advanced Tips for SyncBuffer
--------------------------------------------------------------------------------

⚡ Performance Optimization:
- Choose buffer size carefully:
  * Small: less latency, but higher risk of missing matches under bursty sensors.
  * Large: smoother matches but slight memory increase (~KBs per sensor).

- Tune `max_allowed_spread_usec` according to sensor timestamp jitter.
  * For hardware-triggered sensors, a small spread (e.g., 1000-5000 µs) is enough.
  * For free-running or networked sensors, you may need 5000-10000 µs.

- Prefer setting your timestamps in microseconds (`µs`) for higher precision matching.

- Enable `std::execution::par` (parallel execution) in the transformation phase
  if your transforms are heavy and independent (e.g., decoding compressed data).

⚙️ Notes on Real-Time Behavior:
- `read()` always respects timeout behavior.
- If no synchronized set is found within the timeout, it returns `nullopt`.
- Always handle the `std::optional` returned by `read()` safely.

🛡️ Type Safety:
- Compile-time concepts check that your input types:
  * Have a `.timestamp`.
  * Are copyable.

- The system is zero-copy on input storage (internal references)
  and move-efficient on output construction.

📈 Potential Extensions (for advanced users):
- Allow dynamic registration/deregistration of sensors at runtime.
- Customize matching strategy (e.g., not only closest, but also interpolation).
- Add timestamp prediction to anticipate missing samples under packet loss.

================================================================================
*/

#pragma once

#include <vector>
#include <tuple>
#include <optional>
#include <limits>
#include <atomic>
#include <memory>
//#include <execution>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <chrono>
#include <array>

//// CONCEPTS /////////////////////////////////////////////////

// A type must have a .timestamp convertible to long double
template<typename T>
concept HasTimestamp = requires(T a) {
    { a.timestamp } -> std::convertible_to<long double>;
};

// A type must be copyable
template<typename T>
concept Copyable = std::copy_constructible<T>;

//////////////////////////////////////////////////////////////

// Simple lock-free circular buffer for one producer/consumer
template<typename T>
class LockFreeCircularBuffer
{
    public:
        using value_type = T;
        LockFreeCircularBuffer(size_t capacity)
            : buffer_(capacity), capacity_(capacity) {}

        void push(const T& item)
        {
            const size_t head = head_.load(std::memory_order_relaxed);
            buffer_[head % capacity_] = item;
            head_.store((head + 1) % (2 * capacity_), std::memory_order_release);
        }

        void push(T&& item)
        {
            const size_t head = head_.load(std::memory_order_relaxed);
            buffer_[head % capacity_] = std::move(item);
            head_.store((head + 1) % (2 * capacity_), std::memory_order_release);
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
                return std::nullopt;
            return std::cref(buffer_[(head + 2 * capacity_ - 1 - idx_from_end) % capacity_]);
        }

    private:
        std::vector<T> buffer_;
        size_t capacity_;
        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};
};

// Main class SyncBuffer
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

        SyncBuffer(size_t buffer_capacity, double max_allowed_spread_usec, double timeout_usec)
            : buffers_(std::make_unique<LockFreeCircularBuffer<typename SensorPairs::first_type>>(buffer_capacity)...),
              transforms_(std::make_tuple(std::function<typename SensorPairs::second_type(const typename SensorPairs::first_type&)>()...)),
              max_allowed_spread_(max_allowed_spread_usec),
              timeout_(timeout_usec)
        {}

        // Push with explicit transform
        template<size_t SensorIdx, typename InputType, typename TransformFunc>
        void push(InputType&& input, TransformFunc&& transform)
        {
            auto& buffer = std::get<SensorIdx>(buffers_);

            auto& trans_func = std::get<SensorIdx>(transforms_);
            if (!trans_func)
            {
                trans_func = std::forward<TransformFunc>(transform);
            }

            // 🛡️ Check for duplicate timestamps
            if (auto last = buffer->peek_latest(0); last.has_value())
            {
                if (std::abs(last->get().timestamp * timestamp_scale_factor - input.timestamp * timestamp_scale_factor) < duplicate_timestamp_epsilon)
                {
                    // Duplicate detected, skip push
                    return;
                }
            }
            buffer->push(std::forward<InputType>(input));  // ✅ move if possible
        }

        // Push without explicit transform (auto identity)
        template<size_t SensorIdx, typename InputType>
        void push(InputType&& input)
        {
            auto& buffer = std::get<SensorIdx>(buffers_);

            auto& trans_func = std::get<SensorIdx>(transforms_);
            if (!trans_func)
            {
                using OutputType = typename std::tuple_element_t<SensorIdx, std::tuple<typename SensorPairs::second_type...>>;
                if constexpr (std::is_same_v<std::decay_t<InputType>, OutputType>)
                {
                    trans_func = [](const InputType& in) { return in; };
                }
                else
                {
                    static_assert(std::is_same_v<InputType, OutputType>,
                                  "Missing transform lambda: InputType != OutputType");
                }
            }

            // 🛡️ Check for duplicate timestamps
            if (auto last = buffer->peek_latest(0); last.has_value())
            {
                if (std::abs(last->get().timestamp * timestamp_scale_factor - input.timestamp * timestamp_scale_factor) < duplicate_timestamp_epsilon)
                {
                    // Duplicate detected, skip push
                    return;
                }
            }

            buffer->push(std::forward<InputType>(input));  // ✅ move if possible
        }

        std::optional<OutputTuple> read(std::optional<size_t> max_samples_to_consider = std::nullopt)
        {
            const auto now = now_microseconds();

            // Step 1: Preload timestamps arrays
            using TimestampIndex = std::pair<double, size_t>; // (timestamp, index_in_buffer)
            auto timestamp_arrays = std::apply(
                [&](const auto&... buffers)
                {
                    return std::make_tuple(
                        preload_timestamps(buffers, max_samples_to_consider)...
                    );
                },
                buffers_
            );

            // Step 2: Search best combination
            double best_spread = std::numeric_limits<double>::max();
            std::array<size_t, sizeof...(SensorPairs)> best_indices{};
            bool found = false;

            // Use first sensor as base
            const auto& base_array = std::get<0>(timestamp_arrays);
            for (const auto& [base_time, base_idx] : base_array)
            {
                std::array<size_t, sizeof...(SensorPairs)> current_indices{base_idx};
                std::array<double, sizeof...(SensorPairs)> current_times{base_time};

                bool valid = true;

                [&]<size_t... Is>(std::index_sequence<Is...>)
                {
                    (..., ([&]
                    {
                        if constexpr (Is > 0)
                        {
                            const auto& arr = std::get<Is>(timestamp_arrays);
                            auto it = std::lower_bound(arr.begin(), arr.end(), base_time,
                                [](const TimestampIndex& ti, double t) { return ti.first < t; });

                            if (it == arr.end())
                            {
                                valid = false;
                                return;
                            }

                            // Buscar el más cercano (puede ser it o it-1)
                            if (it != arr.begin())
                            {
                                auto prev = std::prev(it);
                                if (std::abs(prev->first - base_time) < std::abs(it->first - base_time))
                                    it = prev;
                            }

                            // it ahora apunta al timestamp más cercano
                            current_indices[Is] = it->second;
                            current_times[Is] = it->first;
                        }
                    }()));
                }(std::make_index_sequence<sizeof...(SensorPairs)>{});

                if (!valid) continue;

                auto [min_it, max_it] = std::minmax_element(current_times.begin(), current_times.end());
                double spread = *max_it - *min_it;

                if (spread <= max_allowed_spread_ && spread < best_spread)
                {
                    best_spread = spread;
                    best_indices = current_indices;
                    found = true;
                }
            }

            if (!found || best_spread > max_allowed_spread_)
            {
                if (now - last_success_timestamp_ > timeout_)
                    last_success_timestamp_ = now;
                return std::nullopt;
            }

            // Step 3: Peek and build output tuple
            OutputTuple output_tuple;

            [&]<size_t... Is>(std::index_sequence<Is...>)
            {
                (..., ([&]
                {
                    auto& buffer = std::get<Is>(buffers_);
                    auto opt_input = buffer->peek_latest(best_indices[Is]);
                    if (!opt_input) throw std::runtime_error("Invalid peek in SyncBuffer");

                    auto& transform = std::get<Is>(transforms_);
                    std::get<Is>(output_tuple) = transform(opt_input.value().get());
                }()));
            }(std::make_index_sequence<sizeof...(SensorPairs)>{});

            last_success_timestamp_ = now;
            return output_tuple;
        }


        // Helper: preload timestamps
        template<typename BufferPtr>
        auto preload_timestamps(const BufferPtr& buffer_ptr, std::optional<size_t> max_samples)
        {
            const size_t available = buffer_ptr->size();
            const size_t to_read = max_samples.has_value() ? std::min(max_samples.value(), available) : available;
            std::vector<std::pair<double, size_t>> timestamps;
            timestamps.reserve(to_read);  // <- Aquí ya to_read existe ✅

            for (size_t j = 0; j < to_read; ++j)
                if (auto opt = buffer_ptr->peek_latest(available - 1 - j); opt.has_value())
                    timestamps.emplace_back(opt.value().get().timestamp * timestamp_scale_factor, available - 1 - j);

            std::ranges::sort(timestamps); // important for binary search!
            return timestamps;
        }

    private:
        std::tuple<std::unique_ptr<LockFreeCircularBuffer<typename SensorPairs::first_type>>...> buffers_;
        std::tuple<std::function<typename SensorPairs::second_type(const typename SensorPairs::first_type&)>...> transforms_;
        static constexpr double timestamp_scale_factor = 1000.0; // µs to seconds for timestamp conversion
        static constexpr double duplicate_timestamp_epsilon = 1000.0; // µs, to avoid duplicates from the same sensor

        double max_allowed_spread_;
        double timeout_;
        double last_success_timestamp_ = 0.0;

        static double now_microseconds()
        {
            using namespace std::chrono;
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        }

        auto get_all_candidates(std::optional<size_t> max_samples) const
        {
            return std::apply(
                [&](const auto&... buffers)
                {
                    return std::make_tuple(
                        get_candidates(buffers, max_samples)...
                    );
                },
                buffers_
            );
        }

        template<typename BufferT>
        static auto get_candidates(const BufferT& buffer_ptr, std::optional<size_t> max_samples)
        {
            using RawBuffer = typename std::pointer_traits<BufferT>::element_type;
            using ValueType = typename RawBuffer::value_type;

            std::vector<std::optional<ValueType>> candidates;

            const size_t available = buffer_ptr->size();
            const size_t to_read = max_samples.has_value() ? std::min(max_samples.value(), available) : available;

            for (size_t j = 0; j < to_read; ++j)
                candidates.push_back(buffer_ptr->peek_latest(available - 1 - j));

            return candidates;
        }

        template<typename CandidatesTuple>
        bool valid_combination(const CandidatesTuple& candidates, const std::vector<size_t>& indices) const
        {
            return [&]<size_t... Is>(std::index_sequence<Is...>)
            {
                return (... && (indices[Is] < std::get<Is>(candidates).size()));
            }(std::make_index_sequence<std::tuple_size_v<CandidatesTuple>>{});
        }

        template<typename CandidatesTuple>
        InputTuple build_tuple(const CandidatesTuple& candidates, const std::vector<size_t>& indices) const
        {
            return [&]<size_t... Is>(std::index_sequence<Is...>)
            {
                return std::make_tuple(*std::get<Is>(candidates)[indices[Is]]...);
            }(std::make_index_sequence<std::tuple_size_v<CandidatesTuple>>{});
        }

        template<typename CandidatesTuple>
        bool increment_indices(const CandidatesTuple& candidates, std::vector<size_t>& indices) const
        {
            bool incremented = false;
            const auto N = indices.size();

            for (size_t i = 0; i < N; ++i)
            {
                indices[i]++;
                bool within_bounds = [&]<size_t... Is>(std::index_sequence<Is...>)
                {
                    return ((i == Is ? indices[i] < std::get<Is>(candidates).size() : true) && ...);
                }(std::make_index_sequence<std::tuple_size_v<CandidatesTuple>>{});

                if (within_bounds)
                {
                    incremented = true;
                    break;
                }
                indices[i] = 0;
            }
            return incremented;
        }

        double compute_spread(const InputTuple& tuple) const
        {
            std::array<double, sizeof...(SensorPairs)> timestamps;

            std::apply(
                [&](const auto&... elems)
                {
                    timestamps = {elems.timestamp...};
                },
                tuple
            );
            auto [min_it, max_it] = std::minmax_element(timestamps.begin(), timestamps.end());
            return *max_it - *min_it;
        }
};

