/**
// Created by Juan Carlos Díaz on 09/05/24.
// This class is a generic container for a thread-safe synced producer-consumer
// circular buffer (queue like but with random access support) used to transfer data
// between threads. It can be used to synchronize multiple data sources in one
// consumer using a timestamp. For example, between the main thread of a
// component and the (threaded) middleware stubs Example of Buffer creation with
// default converters between input and output types:
//
//      decl:     (Internally creates a queue for each data source)
//           BufferSync<InOut<RoboCompLaser::TLaserData, RoboCompLaser::TLaserData>, InOut<std::string, std::string>> buffer;
//      use:
//           auto timestamp = get_timestamp();
//           buffer.put<0>(std::move(laserData), timestamp); // inserts the laser data value to the queue 0.
//
//           buffer.put<1>("foo", timestamp);  Inserts the laser data value to the queue 1.
//
//           auto [laser, str] = buffer.read(timestamp);  Returns the nearest  values to a timestamp. (does not consume it)
//
//           auto [laser, str] = buffer.read(timestamp, max_diff);  Returns the nearest values to a timestamp given a max_difference. auto
//
//           [laser, str] = buffer.read_first();  Returns the first element of the queues. It doesn't check difference
//           Advanced use: can return unexpected values if not handled carefully.
//           between timestamps, so results may be wrong if there are missing values in some of the queues.
//
//           auto [laser, str] = buffer.read_last();  Returns the first elements of the queues. It can return inconsistent values if some
//           queue was not populated with the last timestamp.
//
//           auto [laser, str] = buffer.read_last(max_diff);  Returns the first elements of the queues. It only returns the last element
//           from queues when the difference between the last timestamp and the last element of the queues in less than `max_diff`.
//           It is also possible to use the functions to retrieve elements  from specific queues only.

//           auto str = buffer.read<0>(timestamp);  Only returns the element from the first InOut<std::string, std::string> queue.
//
// Every read operation returns a value from the circular queue without consuming it, returns an optional
// and allows passing a max_time_diff to consider two values part of the same
// time group.
//
// Example of Buffer creation with user-defined converter (lambda) from input  to output types:
//
//      decl:
//          BufferSync<InOut<RoboCompLaser::TLaserData, RoboCompLaser::TLaserData>> laser_buffer;
//      use:
//        laser_buffer.put<0>(std::move(laserData), [](auto &&I, auto &T){
//        for(auto &&i , I){ T.append(i/2);}});
**/

#pragma once

#include <atomic>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <deque>
#include <ranges>
#include <shared_mutex>
#include <type_traits>
#include <vector>

#include <threadpool/threadpool.h>

using namespace std::chrono_literals;

// Function used as a default argument
template <typename T, typename = void> struct is_iterable : std::false_type {};

/**
 * This is a C++ template struct named 'is_iterable'. It is used to check if a type T is iterable,
 * i.e., if it has begin() and end() methods, which are typical for STL containers like std::vector, std::list, etc.
 *
 * The struct is specialized for any type T that has both begin() and end() methods.
 * The 'std::void_t' is a utility metafunction introduced in C++17 that yields void for any
 * template arguments, regardless of their number or types. It is often used in template metaprogramming
 * for detecting valid types or expressions.
 *
 * The 'decltype' keyword is used to inspect the declared type of an entity or the type and value category
 * of an expression. std::declval<T>() is used to create a value of type T without needing to construct it.
 *
 * If a type T has both begin() and end() methods, the specialization of the struct 'is_iterable' for that type
 * will be chosen over the primary template, and thus 'is_iterable<T>::value' will be true.
 * If a type T does not have either begin() or end() methods, the primary template will be used,
 * and 'is_iterable<T>::value' will be false.
 */
template <typename T>
struct is_iterable<T, std::void_t<decltype(std::declval<T>().begin()),
                                  decltype(std::declval<T>().end())>>
    : std::true_type {};

/**
 * This is a C++20 concept named 'printable'. A concept in C++ is a compile-time
 * predicate that applies to types. This particular concept checks if a type T
 * can be printed to the standard output stream (std::cout).
 *
 * The 'requires' keyword introduces a requirement for the concept. The expression
 * inside the braces is a valid expression statement using the type T. The
 * expression '{ std::cout << t }' checks if the type T can be used with the
 * output stream operator '<<'.
 *
 * The '-> std::same_as<std::ostream &>' part checks if the result of the
 * expression is the same as a reference to std::ostream. This is typically
 * what happens when you use the '<<' operator with std::cout, as the operator
 * returns a reference to the stream to allow for chaining of output operations.
 *
 * In summary, this concept can be used to check at compile-time if a type can
 * be printed to the standard output using the '<<' operator.
 */
template <typename T>
concept printable = requires(T t) {
  { std::cout << t } -> std::same_as<std::ostream &>;
};

/**
 * This function is a lambda function that takes two arguments but does nothing.
 * It is used as a default argument for other functions in the code where a function is expected as an argument.
 * If no function is provided, 'empty_fn' is used, effectively meaning that no operation will be performed.
 */
constexpr auto empty_fn = [](auto &&I, auto &T) {};

// Auxiliary templated struct to hold individual input/output buffer declarations
template <typename _I, typename _O>
struct InOut
{
  typedef _I I;
  typedef _O O;
};

/**
 * BufferSync is a template class that provides a thread-safe buffer for synchronizing data between multiple threads.
 * It is designed to be used in a producer-consumer scenario where data is produced by one or more threads (producers)
 * and consumed by another thread (consumer).
 *
 * The template parameter 'DBs' is a parameter pack representing the types of data that can be stored in the buffer.
 * Each type in 'DBs' is expected to be an instance of the 'InOut' template struct, which represents a pair of input and output types.
 *
**/
template <class... DBs> class BufferSync
{
    private:
        template <typename T> using pair_t_time = std::pair<T, size_t>;
        template <typename T> using deque_db_t = std::deque<pair_t_time<T>>;

        /**
        * 'DBs_size' is a static constexpr (constant expression) of type size_t. It is initialized with the number of types in the template parameter pack 'DBs'.
        * The 'sizeof...' operator is a compile-time operator in C++ that returns the number of elements in a parameter pack.
        * This line of code is calculating the number of types in the 'DBs' parameter pack and storing the result in 'DBs_size'.
        * As a 'constexpr', 'DBs_size' can be used in other constant expressions, such as array sizes or template arguments, where a compile-time constant is required.
        */
        static constexpr size_t DBs_size = sizeof...(DBs);

        /**
        * '_out' is a tuple of deques, where each deque is of (output)  type 'deque_db_t<typename DBs::O>'.
        * 'DBs' is a parameter pack representing the types of data that can be stored in the buffer.
        * Each type in 'DBs' is expected to be an instance of the 'InOut' template struct, which represents a pair of input and output types.
        * 'deque_db_t<typename DBs::O>' is a deque that stores pairs, where each pair consists of an output type and a timestamp.
        * This tuple '_out' is used to store the output data for each type in 'DBs'. Each output data is associated with a timestamp.
        * The data is stored in a deque to allow efficient insertion and removal of elements at both ends.
        */
        std::tuple<deque_db_t<typename DBs::O>...> _out;
        std::array<size_t, DBs_size> last_write;

        mutable std::shared_mutex bufferMutex;
        std::atomic_bool empty;
        /**
        * 'worker' is an instance of the ThreadPool class.
        * ThreadPool is a class that manages a pool of worker threads.
        * These threads can be used to execute tasks concurrently, which can improve the performance of the program if the tasks can be run in parallel.
        * The 'worker' instance is used in the BufferSync class to spawn tasks that are executed in separate threads.
        * This is particularly used in the 'put' method where data is added to the buffer in a separate thread.
        */
        ThreadPool worker;
        size_t queue_size;

    public:
        BufferSync() : worker(1), queue_size(10) {};
        BufferSync(size_t size) : worker(1), queue_size(size) {};
        ~BufferSync() {};

        /**
        * 'read_first' is a method that returns a tuple of optional output types for each data buffer.
        * The method uses a lambda function to generate a const index sequence equal to the size of the data buffers (DBs_size).
        * This index sequence is then used to call the 'read_first' method for each data buffer.
        * The 'read_first' method for each data buffer returns the first element in the buffer (if it exists) as an optional value.
        * The resulting tuple contains an optional value for each data buffer, where each optional value is the first element in the corresponding data buffer.
        * If a data buffer is empty, the corresponding optional value in the tuple will not contain a value.
        * This method is used to retrieve the first elements from all data buffers at once, without removing them from the buffers.
        */
        auto read_first() -> std::tuple<std::optional<typename DBs::O>...>
        {
            constexpr auto seq = std::make_index_sequence<DBs_size>{};
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
              return read_first<Is...>();
            }(seq);
        }

        /**
        * 'read_first' is a template method that returns the first elements from the data buffers without removing them.
        * The template parameters 'idx...' represent the indices of the data buffers.
        * The method first creates a tuple 'ret' of optional output types for each data buffer.
        * If the buffer is empty (checked by 'empty.load()'), the method returns 'ret' immediately.
        * Otherwise, the method locks the buffer for shared access using a 'std::shared_lock'.
        * Then, using a fold expression (lines 210-217) for each data buffer, the method checks if the buffer is not empty.
        * If the buffer is not empty, the first element of the buffer is assigned to the corresponding element in 'ret'.
        * After that, the method checks if all data buffers are empty.
        * If all data buffers are empty, the method sets 'empty' to true.
        * Finally, the method returns 'ret', which contains the first elements from the data buffers.
        * This method is used to retrieve the first elements from all data buffers at once, without removing them from the buffers.
        */
        template <size_t... idx> auto read_first()
        {
            auto ret = subtuple<idx...>();
            if (empty.load())
                return ret;

            std::shared_lock lock(bufferMutex);
            (
                [](auto &q, auto &r)
                {
                  if (!q.empty())
                    r = q.front().first;
                }(std::get<idx>(_out), std::get<idx>(ret)), // arguments to call lambda in place
            ...);

            /**
             * The && ... is a fold expression that applies the logical AND operator (&&) to all elements in the parameter pack.
             * If all data buffers are empty, the expression will be true.
            **/
            if ((std::get<idx>(_out).empty() && ...))
                empty.store(true);

            return ret;
        }

        /**  Note: 'read_last' is overloaded in two versions
        * This version of 'read_last' is a non-template function that creates an index sequence and then calls the second version of 'read_last' with this index sequence.
        * The 'max_diff' parameter is used to determine the maximum allowed difference between the timestamps of the data elements.
        * The method first creates a sequence 'seq' of indices from 0 to 'DBs_size' - 1.
        * Then, the method returns the result of calling the 'read_last' method with the indices in 'seq' and 'max_diff'.
        * The second 'read_last' function is called using a lambda function that takes an 'std::index_sequence<Is...>' as argument.
        */
        auto read_last(size_t max_diff = std::numeric_limits<size_t>::max()) -> std::tuple<std::optional<typename DBs::O>...>
        {
            constexpr auto seq = std::make_index_sequence<DBs_size>{};
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                  return read_last<Is...>(max_diff);
                }(seq);
        }

        /**
        * This version of 'read_last' is a template function that takes a variadic template argument 'idx...'. This argument represents the indices of the data buffers.
        * The function retrieves the last elements from the data buffers at these indices.
        * The function first creates a tuple 'ret' of optional output types for each data buffer.
        * If the buffer is empty, the function returns 'ret' immediately.
        * Otherwise, it locks the buffer for shared access using a 'std::shared_lock'.
        * Then, for each data buffer, it checks if the buffer is not empty and if the difference between the maximum timestamp
        * and the timestamp of the last element in the buffer is less than 'max_diff'.
        * If these conditions are met, the last element of the buffer is assigned to the corresponding element in 'ret'.
        * After that, the function checks if all data buffers are empty. If they are, it sets 'empty' to true.
        * Finally, it returns 'ret', which contains the last elements from the data buffers.
        */
        template <size_t... idx>
        auto read_last(size_t max_diff = std::numeric_limits<size_t>::max())
        {
            auto ret = subtuple<idx...>();

            if (empty.load())
              return ret;

            std::shared_lock lock(bufferMutex);
            size_t max = *std::max_element(last_write.begin(), last_write.end());
            // fold expression
            (
                [max, max_diff](auto &q, auto &r)
                {
                  if (!q.empty() && (max - q.back().second < max_diff))
                    r = q.back().first;
                }(std::get<idx>(_out), std::get<idx>(ret)),
                ...);

            if ((std::get<idx>(_out).empty() && ...))
              empty.store(true);

            return ret;
        }

        /**  Note: 'read' is overloaded in two versions
        * This version of 'read' is a non-template function that creates an index sequence and then calls the second version of 'read' with this index sequence.
        * The 'timestamp' and 'max_diff' parameters are used to determine the maximum allowed difference between the timestamps of the data elements.
        * The method first creates a sequence 'seq' of indices from 0 to 'DBs_size' - 1.
        * Then, the method returns the result of calling the 'read' method with the indices in 'seq', 'timestamp' and 'max_diff'.
        * The second 'read' function is called using a lambda function that takes an 'std::index_sequence<Is...>' as argument.
        */
        auto read(size_t timestamp, size_t max_diff = std::numeric_limits<size_t>::max())
          -> std::tuple<std::optional<typename DBs::O>...>
        {
            constexpr auto seq = std::make_index_sequence<DBs_size>{};
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                return read<Is...>(timestamp, max_diff);
            }(seq);
        }

        /**
        * This version of 'read' is a template function that takes a variadic template argument 'idx...'. This argument represents the indices of the data buffers.
        * The function retrieves the elements from the data buffers at these indices that are closest to the provided timestamp and within the 'max_diff' limit.
        * The function first creates a tuple 'ret' of optional output types for each data buffer.
        * If the buffer is empty (checked by 'empty.load()'), the function returns 'ret' immediately.
        * Otherwise, it locks the buffer for shared access using a 'std::shared_lock'.
        * Then, for each data buffer, it calculates the absolute difference between the timestamp of each element and the provided timestamp.
        * It finds the element with the minimum difference and if this difference is less than or equal to 'max_diff', it assigns this element to the corresponding element in 'ret'.
        * After that, the function checks if all data buffers are empty. If they are, it sets 'empty' to true.
        * Finally, it returns 'ret', which contains the elements from the data buffers that are closest to the provided timestamp and within the 'max_diff' limit.
        */
        template <size_t... idx>
        auto read(size_t timestamp, size_t max_diff = std::numeric_limits<size_t>::max())
        {
            auto ret = subtuple<idx...>();

            if (empty.load())
            return ret;

            std::shared_lock lock(bufferMutex);
            size_t dropped = 0;
            // fold expression
            (
                [timestamp, max_diff, &dropped](auto &q, auto &r) {
                  std::vector<size_t> diffs;
                  std::transform(q.begin(), q.end(), std::back_inserter(diffs),
                                 [timestamp](auto &val) {
                                   return std::abs(static_cast<ssize_t>(val.second) -
                                                   static_cast<ssize_t>(timestamp));
                                 });
                  auto it_idx = std::min(diffs.begin(), diffs.end()) - diffs.begin();
                  auto it = q.begin() + it_idx;
                  if (it != q.end() && timestamp - it->second <= max_diff)
                    r = it->first;
                }(std::get<idx>(_out), std::get<idx>(ret)),
                ...);

            if ((std::get<idx>(_out).empty() && ...))
              empty.store(true);

            return ret;
        }

        /**
        * 'put' is a template method that inserts a new data item into the buffer at a specific index.
        * The template parameter 'idx' represents the index of the data buffer.
        * The method takes three parameters:
        * - 'd' is the data item to be inserted. It is of input type 'typename InOut::I' and is passed by rvalue reference.
        * - 'timestamp' is the timestamp associated with the data item.
        * - 't' is a function that transforms the input data item to the output data type. It defaults to 'empty_fn' if not provided.
        *
        * The method first spawns a new task in the worker thread pool. The task is a lambda function that does the following:
        * - It creates a temporary variable 'temp' of output type 'typename InOut::O'.
        * - It calls the 'ItoO' method to transform the input data item 'd' to the output data type and stores the result in 'temp'.
        * - It locks the buffer for exclusive access using a 'std::unique_lock'.
        * - It updates the 'last_write' timestamp for the data buffer at index 'idx'.
        * - If the size of the data buffer at index 'idx' is greater than or equal to 'queue_size', it removes the first element from the buffer.
        * - It inserts the transformed data item 'temp' and its associated timestamp into the data buffer at index 'idx'.
        * - It sets 'empty' to false to indicate that the buffer is not empty.
        *
        * Finally, the method returns true to indicate that the data item was successfully inserted into the buffer.
        */
        template <size_t idx, typename InOut = std::remove_cvref_t<decltype(std::get<idx>(std::tuple<DBs...>()))>>
        bool put(typename InOut::I &&d, size_t timestamp, std::function<void(typename InOut::I &&, typename InOut::O &)> t = empty_fn)
            {
                worker.spawn_task
                (
                    [this, d = std::move(d), t = std::move(t), timestamp]() mutable
                    {
                      typename InOut::O temp;
                      this->ItoO(std::move(d), temp, t);
                      std::unique_lock lock(this->bufferMutex);
                      last_write[idx] = std::chrono::steady_clock::now().time_since_epoch().count();
                      if (std::get<idx>(_out).size() + 1 > queue_size)
                        std::get<idx>(_out).pop_front();
                      std::get<idx>(_out).emplace_back(std::move(temp), timestamp);
                      empty.store(false);
                    }
                );
                return true;
            }

        /**
        * 'show' is a method that prints the contents of the data buffers to the standard output.
        * The method is constrained by a C++20 concept 'printable', which checks if the output types of the data buffers can be printed to the standard output.
        * The method first prints a line of dashes to the standard output.
        * Then, it iterates over the range [0, queue_size) using a range-based for loop and the 'std::views::iota' function.
        * For each index 'i' in the range, the method does the following:
        * - It creates a const index sequence 'seq' equal to the size of the data buffers (DBs_size).
        * - It prints the index 'i' and the headers for the data buffers to the standard output.
        * - It then calls a lambda function with the index sequence 'seq'. This lambda function does the following for each index 'idx' in 'seq':
        *   - If the size of the data buffer at index 'idx' is greater than 'i', it prints the index 'idx', the value of the element at index 'i' in the data buffer, and its associated timestamp to the standard output.
        *   - If the size of the data buffer at index 'idx' is not greater than 'i', it prints the index 'idx' and the word "empty" to the standard output.
        * - Finally, it prints another line of dashes to the standard output.
        * This method is used to print the contents of the data buffers to the standard output for debugging or logging purposes.
        */
        void show() requires(printable<typename DBs::O> && ...)
        {
            std::cout << "--------------------------------------------------\n";
            for (std::weakly_incrementable auto i : std::views::iota(0, (int)queue_size))
            {
                constexpr auto seq = std::make_index_sequence<DBs_size>{};
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::cout << "Element: " << i << "\n";
                    std::cout << "idx: |" << std::setw(15) << "val:" << " | " << std::setw(12) << "timestamp:" << "\n";
                    std::cout << "--------------------------------------------------\n";
                    (
                        [&]<size_t idx>()
                        {
                          if (i < std::get<idx>(_out).size())
                          {
                            auto &[f, s] = std::get<idx>(_out)[i];
                            std::cout << std::setw(4) << idx << " | " << std::setw(14) << f << " | " << std::setw(15) << s << "\n";
                          }
                          else
                              std::cout << std::setw(4) << idx << " | " << std::setw(14) << " empty" << " |\n";
                        }.template operator()<Is>(),
                    ...);
                }(seq);
                std::cout << "--------------------------------------------------\n";
            }
        }

    private:

        /**
        * 'subtuple' is a template method that creates a new tuple of optional output types for each data buffer.
        * The template parameters 'Is...' represent the indices of the data buffers.
        * The method first creates a tuple 'tuple' of optional output types for each data buffer.
        * Then, the method returns a new tuple that contains the elements of 'tuple' at the indices specified by 'Is...'.
        * This method is used to create a new tuple that contains a subset of the data buffers.
        * The subset of data buffers is determined by the indices 'Is...'.
        */
        template <std::size_t... Is> constexpr auto subtuple()
        {
            constexpr std::tuple<std::optional<typename DBs::O>...> tuple{};
            return std::make_tuple(std::get<Is>(tuple)...);
        }

        /**
        * 'ItoO' is a template method that transforms an input data item of type 'I' to an output data item of type 'O'.
        * The method takes three parameters:
        * - 'iTypeData' is the input data item to be transformed. It is of type 'I' and is passed by rvalue reference.
        * - 'oTypeData' is the output data item where the result of the transformation is stored. It is of type 'O' and is passed by reference.
        * - 't' is a function that performs the transformation from 'I' to 'O'. It defaults to 'empty_fn' if not provided.
        *
        * The method first checks if 'I' and 'O' are the same type or if 'I' can be converted to 'O'.
        * If this is the case, it simply moves 'iTypeData' to 'oTypeData'.
        *
        * If 'I' and 'O' are not the same type and cannot be converted, the method checks if both 'I' and 'O' are iterable types.
        * If they are, it checks if the element type of 'I' can be converted to the element type of 'O'.
        * If this is the case, it creates 'oTypeData' by moving all elements from 'iTypeData' to 'oTypeData'.
        *
        * If the element type of 'I' cannot be converted to the element type of 'O', the method requires a transformation function 't' to be provided.
        * It asserts that 't' is not the same as 'empty_fn', and then calls 't' with 'iTypeData' and 'oTypeData'.
        *
        * If 'I' and 'O' are not iterable types, the method also requires a transformation function 't' to be provided.
        * It asserts that 't' is not the same as 'empty_fn', and then calls 't' with 'iTypeData' and 'oTypeData'.
        *
        * This method is used in the 'put' method to transform the input data item to the output data type before inserting it into the buffer.
        */
        template <typename I, typename O>
        void ItoO(I &&iTypeData, O &oTypeData, const std::function<void(I &&, O &)> &t = empty_fn)
        {
            if constexpr (std::is_same<I, O>::value || std::is_convertible<I, O>::value)
                oTypeData = std::move(iTypeData);
            else if constexpr (is_iterable<I>::value && is_iterable<O>::value)
            {
                using I_T = typename std::decay<decltype(*iTypeData.begin())>::type;
                using O_T = typename std::decay<decltype(*oTypeData.begin())>::type;
                if constexpr (std::is_convertible<I_T, O_T>::value)
                    oTypeData = O(std::make_move_iterator(iTypeData.begin()), std::make_move_iterator(iTypeData.end()));
                else
                {
                    static_assert(!std::is_same<decltype(t), decltype(empty_fn)>::value, "A function needs to be implemented to transform ItoO");
                    t(std::move(iTypeData), oTypeData);
                }
            }
            else
            {
                static_assert(!std::is_same<decltype(t), decltype(empty_fn)>::value, "A function needs to be implemented to transform ItoO");
                t(std::move(iTypeData), oTypeData);
            }
        };
};
