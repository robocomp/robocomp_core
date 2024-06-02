// DounbleBuffer class template definition

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <tuple>
#include <functional>
#include <queue>
#include <future>
#include <optional>
//#include <boost/lockfree/queue.hpp>
#include "../threadpool/threadpool.h"

// concept Printable to check if the type can be printed to std::cout
template <typename T>
concept Printable = requires(T t)
{
    { std::cout << t } -> std::same_as<std::ostream &>;
};

// concept AllPrintable to check if all types in a tuple can be printed
template <typename... Args>
concept AllPrintable = (Printable<Args> && ...);

// Default empty function for input to output transformation
constexpr auto empty_fn = [](auto &&I, auto &T) {};

// Check if a type is iterable
template <typename T, typename = void>
struct is_iterable : std::false_type {};

template <typename  T>
struct is_iterable<T, std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end())>> : std::true_type {};

// Primary template (never used directly)
template <typename InputTypes, typename OutputTypes>
class DoubleBuffer;

template <typename... InputTypes, typename... OutputTypes>
class DoubleBuffer<std::tuple<InputTypes...>, std::tuple<OutputTypes...>>
{
    public:
        struct DataElement
        {
            std::chrono::steady_clock::time_point timestamp;
            std::tuple<OutputTypes...> data;
        };

        DoubleBuffer() : buffer(1), bufferSize(1), head(0), count(0), threadPool(1) {};
        DoubleBuffer(size_t size, size_t threadPoolSize)
                : buffer(size), bufferSize(size), head(0), count(0), threadPool(threadPoolSize)
        {
            if (size == 0)
                throw std::invalid_argument("Buffer size must be greater than zero");
            if (threadPoolSize == 0)
                throw std::invalid_argument("Thread pool size must be greater than zero");
        };
        ~DoubleBuffer() {};
        void set_buffer_size(size_t size)
        {
            if (size == 0)
                throw std::invalid_argument("Buffer size must be greater than zero");
            bufferSize = size;
            buffer.resize(size);
        }

        /// Producer puts input data into the buffer queue with a transformation function
        void put(std::tuple<InputTypes...> &&inputs, std::function<std::tuple<OutputTypes...>(std::tuple<InputTypes...> &&)> transform)
        {
            // Submit the transformation task to the thread pool
            threadPool.spawn_task([this, inputs = std::move(inputs), transform = std::move(transform)]() mutable
                                  {
                                      std::tuple<OutputTypes...> temp;
                                      if(this->convert_is_possible(std::move(inputs), temp, transform))
                                      {
                                          auto transformedData = transform(std::move(inputs));
                                          auto timestamp = std::chrono::steady_clock::now();
                                          std::unique_lock<std::mutex> lock(mtx);
                                          buffer[head] = {timestamp, std::move(transformedData)};
                                          head = (head + 1) % bufferSize;
                                          if (count < bufferSize)
                                              ++count;
                                          cv.notify_all();
                                      }
                                  });
        }

        /// Consumer requests data closest to the given timestamp (or the most recent if timestamp is zero)
        std::tuple<OutputTypes...> get(const std::chrono::steady_clock::time_point &targetTime = std::chrono::steady_clock::time_point::min())
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]()
            { return count > 0; });

            if (targetTime == std::chrono::steady_clock::time_point::min())
            {
                // Return the most recent element
                return buffer[(head + bufferSize - 1) % bufferSize].data;
            }

            size_t closestIndex = 0;
            auto minDiff = std::chrono::steady_clock::duration::max();

            for (size_t i = 0; i < count; ++i)
            {
                size_t index = (head + bufferSize - i - 1) % bufferSize;
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(buffer[index].timestamp - targetTime);
                if (std::abs(diff.count()) < std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(minDiff).count()))
                {
                    closestIndex = index;
                    minDiff = diff;
                }
            }

            return buffer[closestIndex].data;
        }

        /// Consumer requests a new, fresh data given the timestamp of its latest read
        std::optional<std::tuple<OutputTypes...>> get_new(const std::chrono::steady_clock::time_point &lastTime)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]()
            { return count > 0; });

            // if lastTime is newer than the most recent data, return most recent data, else return empty tuple
            if (lastTime >= buffer[(head + bufferSize - 1) % bufferSize].timestamp)
                return buffer[(head + bufferSize - 1) % bufferSize].data;
            else    // return empty tuple
                return {};
        }

        /// Consumer requests all data newer the timestamp of its latest read
        std::vector<std::tuple<OutputTypes...>> get_all_new(const std::chrono::steady_clock::time_point &lastTime)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]()
            { return count > 0; });

            std::vector<std::tuple<OutputTypes...>> newData;
           for (int i = count - 1; i >= 0; --i) // iterate from the most recent data backwards so they are inserted in the correct order
           {
                size_t index = (head + bufferSize - i - 1) % bufferSize;
                if (buffer[index].timestamp > lastTime)
                    newData.push_back(buffer[index].data);
           }
            return newData;
        }

        /// Prints the current state of the buffer with timestamps
        void print() const requires AllPrintable<OutputTypes...>
        {
            std::unique_lock<std::mutex> lock(mtx);
            std::cout << "[print] Buffer state: \n";
            for (size_t i = 0; i < count; ++i)
            {
                size_t index = (head + bufferSize - i - 1) % bufferSize;
                auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(buffer[index].timestamp.time_since_epoch()).count();
                std::cout << "  " << ts << " ms, Data: [";
                printTuple(buffer[index].data);
                std::cout << "]\n";
            }
        }

    private:
        std::vector<DataElement> buffer;
        size_t bufferSize;
        size_t head;
        size_t count;
        mutable std::mutex mtx;
        std::condition_variable cv;
        bool stopWorker;
        ThreadPool threadPool;

        template <std::size_t Index = 0>
        void printTuple(const std::tuple<OutputTypes...> &t) const
        {
            if constexpr (Index < sizeof...(OutputTypes))
            {
                std::cout << std::get<Index>(t) << " ";
                printTuple<Index + 1>(t);
            }
        };

    // Function to check if input types are convertible to output types

    template <typename... Input, typename... Output>
    bool convert_is_possible(std::tuple<InputTypes...> &&iTypeData, std::tuple<OutputTypes...> &oTypeData,
                             const std::function<std::tuple<OutputTypes...>(std::tuple<InputTypes...> &&)> &transform = empty_fn)
    {
        ([&](auto &&input, auto &output)
        {
            using I_T = std::decay_t<decltype(input)>;
            using O_T = std::decay_t<decltype(output)>;

            if constexpr (std::is_same_v<I_T, O_T> || std::is_convertible_v<I_T, O_T>)
                output = std::forward<I_T>(input);
            else if constexpr (is_iterable<I_T>::value && is_iterable<O_T>::value)
            {
                using I_T_Element = typename std::decay<decltype(*std::begin(input))>::type;
                using O_T_Element = typename std::decay<decltype(*std::begin(output))>::type;
                if constexpr (std::is_convertible_v<I_T_Element, O_T_Element>)
                {
                    output = O_T(std::make_move_iterator(std::begin(input)), std::make_move_iterator(std::end(input)));
                }
                else
                {
                    static_assert(!std::is_same_v<decltype(transform), decltype(empty_fn)>, "A function needs to be implemented to transform input to output types");
                    transform(std::forward<I_T>(input), output);
                }
            }
            else
            {
                static_assert(!std::is_same_v<decltype(transform), decltype(empty_fn)>, "A function needs to be implemented to transform input to output types");
                transform(std::forward<I_T>(input), output);
            } }(std::forward<Input>(iTypeData), oTypeData),
                ...);
        return true;
    }
};
