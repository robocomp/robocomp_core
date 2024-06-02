#include "doublebuffer.h"
#include <string>
#include <iostream>
#include <iomanip>

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
        buffer.put(std::move(data), [](std::tuple<int, float> &&inputs) -> std::tuple<std::string, std::string>
        {
            int i;
            float f;
            std::tie(i, f) = inputs;

            return std::make_tuple("Int: " + std::to_string(i), "Float: " + std::to_string(f));
        });

        // Get current time as a time_point object
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::tm* localTime = std::localtime(&currentTime);
        std::cout << "Producer: " << i << " at time: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer(DoubleBuffer<std::tuple<int, float>, std::tuple<std::string, std::string>> &buffer)
{
    using namespace std::chrono_literals;
    for (int i = 0; i < 10; ++i)
    {
        buffer.print();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

        // Get the most recent data when target time is not specified
        //static auto now = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto data_ = buffer.get_new(now);
//        if(not data.empty())
//            now = std::chrono::steady_clock::now();

        //auto time =  ms.count();
//        for(const auto &d : data)
//        {
//            std::cout << "\nRead most recent data at " << ms <<": ";
//            std::apply([](auto &&... args)
//                       { ((std::cout << args << " "), ...); },
//                       d);
//            std::cout << std::endl;
//        }
        if(data_.has_value())
        {
            std::cout << "Read most recent data at " << ms << ": ";
            std::apply([](auto &&... args)
                       { ((std::cout << args << " "), ...); },
                       data_.value());
            std::cout << std::endl;
        }

        // Get data closest to a specific timestamp
//        auto targetTime = std::chrono::steady_clock::now() - 200ms;
//        data = buffer.get(targetTime);
//        std::cout << "Read data closest to ts: ";
//        std::apply([](auto &&... args)
//                   { ((std::cout << args << " "), ...); },
//                   data);
//        std::cout << std::endl;
//        std::cout << "----------------" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int main()
{
    //DoubleBuffer<std::tuple<int, float>, std::tuple<std::string, std::string>> buffer(5, 1); // Circular buffer size of 20 elements, thread pool size of 4
    DoubleBuffer<std::tuple<int, float>, std::tuple<std::string, std::string>> buffer;
    //DoubleBuffer<std::tuple<int>, std::tuple<int>> buffer;
    buffer.set_buffer_size(10);

    std::thread producerThread(producer, std::ref(buffer));
    std::thread consumerThread(consumer, std::ref(buffer));

    producerThread.join();
    consumerThread.join();

    return 0;
}
