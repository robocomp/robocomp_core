#include "doublebuffer_sync.h"
#include <string>
#include <iostream>
#include <iomanip>

using namespace std::chrono_literals;

// Helper function to create the transform lambda
//auto createTransform()
//{
//    return [](std::tuple<int, float> &&inputs) -> std::tuple<std::string, std::string>
//    {
//        int i;
//        float f;
//        std::tie(i, f) = inputs;
//
//        return std::make_tuple("Int: " + std::to_string(i), "Float: " + std::to_string(f));
//    };
//}

void producer(BufferSync<InOut<int, float>, InOut<std::string, std::string>> &buffer)
{
    for (int i = 0; i < 10; ++i)
    {
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        buffer.put<0>(std::move(i), static_cast<size_t>(timestamp));
        buffer.put<1>(std::move("pepe"), static_cast<size_t>(timestamp));
        //buffer.put<1>(std::move(1.f*i*i), static_cast<size_t>(timestamp));

        std::cout << "Producer: " << i << " at time: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer(BufferSync<InOut<int, float>, InOut<std::string, std::string>> &buffer)
{
    for (int i = 0; i < 10; ++i)
    {
        //buffer.show();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

        // Get the most recent data when target time is not specified

        auto [in, f] = buffer.read_last();
        if(in.has_value())
            std::cout << "Read most recent data at " << ms << ": " << in.value()  << std::endl;

//        //static auto now = std::chrono::steady_clock::now();
//        auto now = std::chrono::steady_clock::now();
//        auto data_ = buffer.get_new(now);
////        if(not data.empty())
////            now = std::chrono::steady_clock::now();
//
//        //auto time =  ms.count();
////        for(const auto &d : data)
////        {
////            std::cout << "\nRead most recent data at " << ms <<": ";
////            std::apply([](auto &&... args)
////                       { ((std::cout << args << " "), ...); },
////                       d);
////            std::cout << std::endl;
////        }
//        if(data_.has_value())
//        {
//            std::cout << "Read most recent data at " << ms << ": ";
//            std::apply([](auto &&... args)
//                       { ((std::cout << args << " "), ...); },
//                       data_.value());
//            std::cout << std::endl;
//        }
//
//        // Get data closest to a specific timestamp
////        auto targetTime = std::chrono::steady_clock::now() - 200ms;
////        data = buffer.get(targetTime);
////        std::cout << "Read data closest to ts: ";
////        std::apply([](auto &&... args)
////                   { ((std::cout << args << " "), ...); },
////                   data);
////        std::cout << std::endl;
////        std::cout << "----------------" << std::endl;
//
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
}

int main()
{

    BufferSync<InOut<int, float>, InOut<std::string, std::string>> buffer;
    //buffer.set_buffer_size(10);

    std::thread producerThread(producer, std::ref(buffer));
    std::thread consumerThread(consumer, std::ref(buffer));

    producerThread.join();
    consumerThread.join();

    buffer.show();

    return 0;
}
