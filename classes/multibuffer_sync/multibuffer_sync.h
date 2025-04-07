// Final professional-grade SyncBuffer
// Using variadic std::pair<InputType, OutputType> per sensor
// Transform lambda set at push time
// Transformations happen in parallel at read time using std::execution::par
// Clean C++23 code
#pragma once

#include <vector>
#include <tuple>
#include <optional>
#include <limits>
#include <atomic>
//#include <execution>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <chrono>
#include <array>

// Simple lock-free circular buffer for one producer/consumer
template<typename T>
class LockFreeCircularBuffer
{
    public:
        using value_type = T; // <-- Add this alias!
        LockFreeCircularBuffer(size_t capacity)
            : buffer_(capacity), capacity_(capacity)
        {
            static_assert(std::atomic<size_t>::is_always_lock_free, "Atomic size_t must be lock-free!");
        }

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

        std::optional<T> peek(size_t offset) const
        {
            const size_t tail = tail_.load(std::memory_order_acquire);
            const size_t head = head_.load(std::memory_order_acquire);

            if (tail + offset >= head)
                return std::nullopt;

            return buffer_[(tail + offset) % capacity_];
        }

        size_t size() const
        {
            const size_t head = head_.load(std::memory_order_acquire);
            const size_t tail = tail_.load(std::memory_order_acquire);
            if (head >= tail)
                return head - tail;
            else
                return (head + 2 * capacity_) - tail;
        }

    private:
        std::vector<T> buffer_;
        size_t capacity_;
        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};
};

// SyncBuffer

template<typename... SensorPairs>
class SyncBuffer
{
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
            buffer->push(std::forward<InputType>(input));  // ✅ move if possible

            auto& trans_func = std::get<SensorIdx>(transforms_);
            if (!trans_func)
            {
                trans_func = std::forward<TransformFunc>(transform);
            }
        }

        // Push without explicit transform (auto identity)
        template<size_t SensorIdx, typename InputType>
        void push(InputType&& input)
        {
            auto& buffer = std::get<SensorIdx>(buffers_);
            buffer->push(std::forward<InputType>(input));  // ✅ move if possible

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
        }

        std::optional<OutputTuple> read(std::optional<size_t> max_samples_to_consider = std::nullopt)
        {
            const auto now = now_microseconds();
            auto candidates = get_all_candidates(max_samples_to_consider);

            double best_spread = std::numeric_limits<double>::max();
            InputTuple best_tuple;
            bool found = false;

            // Find best combination (small N, brute-force)
            std::vector<size_t> indices(sizeof...(SensorPairs), 0);

            while (true)
            {
                if (!valid_combination(candidates, indices)) break;
                auto tuple = build_tuple(candidates, indices);
                if (const double spread = compute_spread(tuple); spread < best_spread && spread <= max_allowed_spread_)
                {
                    best_spread = spread;
                    best_tuple = tuple;
                    found = true;
                }
                if (!increment_indices(candidates, indices))
                    break;
            }

            if (!found)
            {
                if (now - last_success_timestamp_ > timeout_)
                {
                    last_success_timestamp_ = now;
                    return std::nullopt;
                }
                else
                {
                    return std::nullopt;
                }
            }

            // Parallel transform raw tuple -> output tuple (corrected with index_sequence)
            OutputTuple output_tuple;

            [&]<size_t... Is>(std::index_sequence<Is...>)
            {
                ((std::get<Is>(output_tuple) = std::get<Is>(transforms_)(std::get<Is>(best_tuple))), ...);
            }(std::make_index_sequence<sizeof...(SensorPairs)>{});

            last_success_timestamp_ = now;
            return output_tuple;
        }

    private:
        std::tuple<std::unique_ptr<LockFreeCircularBuffer<typename SensorPairs::first_type>>...> buffers_;
        std::tuple<std::function<typename SensorPairs::second_type(const typename SensorPairs::first_type&)>...> transforms_;

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
                candidates.push_back(buffer_ptr->peek(available - 1 - j));

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

