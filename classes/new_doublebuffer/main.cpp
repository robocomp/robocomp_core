#include "doublebuffer.h"
#include <string>
#include <iostream>

// Helper function to create the transform lambda
auto createTransform()
{
    return [](std::tuple<int, float> &&inputs) -> std::tuple<std::string, std::string>
    {
        int i;
        float f;
        std::tie(i, f) = inputs;

        return std::make_tuple("Int: " + std::to_string(i), "Float: " + std::to_string(f));
    };
}

void producer(DoubleBuffer<std::tuple<int, float>, std::tuple<std::string, std::string>> &buffer)
{
    for (int i = 0; i < 10; ++i)
    {
        std::tuple<int, float> data = std::make_tuple(i, i * i);
        buffer.put(std::move(data), createTransform());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer(DoubleBuffer<std::tuple<int, float>, std::tuple<std::string, std::string>> &buffer)
{
    using namespace std::chrono_literals;
    for (int i = 0; i < 10; ++i)
    {
        // Get the most recent data when target time is not specified
        auto data = buffer.get();
        std::cout << "Read most recent data: ";
        std::apply([](auto &&... args)
                   { ((std::cout << args << " "), ...); },
                   data);
        std::cout << std::endl;
        //std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Get data closest to a specific timestamp
        auto targetTime = std::chrono::steady_clock::now() - 50ms;
        data = buffer.get(targetTime);
        std::cout << "Read data closest to timestamp: ";
        std::apply([](auto &&... args)
                   { ((std::cout << args << " "), ...); },
                   data);
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main()
{
    DoubleBuffer<std::tuple<int, float>, std::tuple<std::string, std::string>> buffer(20, 4); // Circular buffer size of 20 elements, thread pool size of 4
    std::thread producerThread(producer, std::ref(buffer));
    std::thread consumerThread(consumer, std::ref(buffer));

    producerThread.join();
    consumerThread.join();

    return 0;
}
