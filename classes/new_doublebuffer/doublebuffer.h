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
//#include <boost/lockfree/queue.hpp>
#include "../threadpool/threadpool.h" // Include your ThreadPool header

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

    DoubleBuffer(size_t size, size_t threadPoolSize)
            : buffer(size), bufferSize(size), head(0), count(0), stopWorker(false), threadPool(threadPoolSize)
    {
    }

    ~DoubleBuffer()
    {
        stopWorker = true;
    }

    // Producer puts input data into the buffer queue with a transformation function
    void put(std::tuple<InputTypes...> &&inputs, std::function<std::tuple<OutputTypes...>(std::tuple<InputTypes...> &&)> transform)
    {
        // Submit the transformation task to the thread pool
        threadPool.spawn_task([this, inputs = std::move(inputs), transform = std::move(transform)]() mutable
                              {
                                  auto transformedData = transform(std::move(inputs));
                                  auto timestamp = std::chrono::steady_clock::now();

                                  {
                                      std::unique_lock<std::mutex> lock(mtx);
                                      buffer[head] = {timestamp, std::move(transformedData)};
                                      head = (head + 1) % bufferSize;
                                      if (count < bufferSize)
                                          ++count;
                                  }
                                  cv.notify_all(); });
    }

    // Consumer gets data closest to the given timestamp (or the most recent if timestamp is zero)
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

private:
    std::vector<DataElement> buffer;
    size_t bufferSize;
    size_t head;
    size_t count;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopWorker;
    ThreadPool threadPool;
};
