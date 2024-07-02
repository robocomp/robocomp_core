//
// Created by juancarlos on 09/05/24.
// This class is a generic container for a thread-safe synced producer-consumer
// buffer (queue like but with random access support) used to transfer data
// between threads. It can be used to synchronize multiple data sources in one
// consumer using a timestamp. For example, between the main thread of a
// component and the (threaded) middleware stubs Example of Buffer creation with
// default converters between input and output types:
//      decl: BufferSync<InOut<RoboCompLaser::TLaserData,
//      RoboCompLaser::TLaserData>, InOut<std::string, std::string>> buffer;
//            Internally it creates a queue for each data source.
//      use:
//           auto timestamp = get_timestamp();
//           buffer.put<0>(std::move(laserData), timestamp); // inserts the
//           laser data value to the queue 0. buffer.put<1>("foo", timestamp);
//           // inserts the laser data value to the queue 1.
//      use: auto [laser, str] = buffer.read(timestamp)  // return the nearest
//      values to a timestamp. (does not consume it)
//           auto [laser, str] = buffer.read(timestamp, max_diff)  // return
//           the nearest values to a timestamp given a max_difference. auto
//           [laser, str] = buffer.read_first() // return the first element
//           of the queues. It doesn't check difference
//           //Advanced use: can return unexpected values if not handled
//           carefully.
//           //between timestamps, so results may be wrong if there are missing
//           values in some of the queues. auto [laser, str] =
//           buffer.read_last()  // return the first elements of the queues.
//           It can return inconsistent values if some
//           //queue was not populated with the last timestamp.
//           auto [laser, str] = buffer.read_last(max_diff)  // return the
//           first elements of the queues. It only returns the last element
//           //from queues when the difference between the last timestamp and
//           the last element of the queues in less than `max_diff`.
//           //It is also possible to use the functions to retrieve elements
//           from specific queues only. auto str = buffer.read<0>(timestamp);
//           //Only returns the element from the InOut<std::string, std::string>
//           queue. The other queues
//           //(InOut<RoboCompLaser::TLaserData, RoboCompLaser::TLaserData>)
//           still has a value.
//
// Every read operation returns a value from the queue without consiming it, returns an optional
// and allows passing a max_time_diff to consider two values part of the same
// time group. Example of Buffer creation with user-defined converter from input
// to output types
//      decl: BufferSync<InOut<RoboCompLaser::TLaserData,
//      RoboCompLaser::TLaserData>> laser_buffer; use:
//      laser_buffer.put<0>(std::move(laserData), [](auto &&I, auto &T){
//      for(auto &&i , I){ T.append(i/2);}});

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

#include "threadpool.h"

using namespace std::chrono_literals;

// Función utilizada como argumento por defecto.
constexpr auto empty_fn = [](auto &&I, auto &T) {};

template <typename T, typename = void> struct is_iterable : std::false_type {};
template <typename T>
// especialización del template si tiene función begin y end.
struct is_iterable<T, std::void_t<decltype(std::declval<T>().begin()),
                                  decltype(std::declval<T>().end())>>
    : std::true_type {};

template <typename T>
concept printable = requires(T t) {
  { std::cout << t } -> std::same_as<std::ostream &>;
};

template <typename _I, typename _O /*, void(*Fn)(_I&&, _O&) fn */>
struct InOut {
  typedef _I I;
  typedef _O O;
};

template <class... DBs> class BufferSync {
private:
  template <typename T> using pair_t_time = std::pair<T, size_t>;
  template <typename T> using deque_db_t = std::deque<pair_t_time<T>>;

  static constexpr size_t DBs_size = sizeof...(DBs);

  std::tuple<deque_db_t<typename DBs::O>...> _out;
  std::array<size_t, DBs_size> last_write;

  mutable std::shared_mutex bufferMutex;
  std::atomic_bool empty;
  ThreadPool worker;
  size_t queue_size;

public:
  BufferSync() : worker(1), queue_size(5) {};
  BufferSync(size_t size) : worker(1), queue_size(size) {};

  ~BufferSync() {};

  auto read_first() -> std::tuple<std::optional<typename DBs::O>...> {
    constexpr auto seq = std::make_index_sequence<DBs_size>{};
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return read_first<Is...>();
    }(seq);
  }

  template <size_t... idx> auto read_first() {

    auto ret = subtuple<idx...>();

    if (empty.load()) {
      return ret;
    }
    std::shared_lock lock(bufferMutex);

    (
        [](auto &q, auto &r) {
          if (!q.empty()) {
            r = q.front().first;
          }
        }(std::get<idx>(_out), std::get<idx>(ret)),
        ...);

    if ((std::get<idx>(_out).empty() && ...)) {
      empty.store(true);
    }

    return ret;
  }

  auto read_last(size_t max_diff = std::numeric_limits<size_t>::max())
      -> std::tuple<std::optional<typename DBs::O>...> {
    constexpr auto seq = std::make_index_sequence<DBs_size>{};
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return read_last<Is...>(max_diff);
    }(seq);
  }

  template <size_t... idx>
  auto read_last(size_t max_diff = std::numeric_limits<size_t>::max()) {

    auto ret = subtuple<idx...>();

    if (empty.load()) {
      return ret;
    }
    std::shared_lock lock(bufferMutex);

    size_t max = *std::max_element(last_write.begin(), last_write.end());
    (
        [max, max_diff](auto &q, auto &r) {
          if (!q.empty() && (max - q.back().second < max_diff)) {
            r = q.back().first;
          }
        }(std::get<idx>(_out), std::get<idx>(ret)),
        ...);

    if ((std::get<idx>(_out).empty() && ...)) {
      empty.store(true);
    }

    return ret;
  }

  auto read(size_t timestamp,
            size_t max_diff = std::numeric_limits<size_t>::max())
      -> std::tuple<std::optional<typename DBs::O>...> {
    constexpr auto seq = std::make_index_sequence<DBs_size>{};
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return read<Is...>(timestamp, max_diff);
    }(seq);
  }

  template <size_t... idx>
  auto read(size_t timestamp,
            size_t max_diff = std::numeric_limits<size_t>::max()) {
    auto ret = subtuple<idx...>();

    if (empty.load()) {
      return ret;
    }

    std::shared_lock lock(bufferMutex);

    size_t dropped = 0;
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
          if (it != q.end() && timestamp - it->second <= max_diff) {
            r = it->first;
          }
        }(std::get<idx>(_out), std::get<idx>(ret)),
        ...);

    if ((std::get<idx>(_out).empty() && ...)) {
      empty.store(true);
    }

    return ret;
  }

  template <size_t idx, typename InOut = std::remove_cvref_t<
                            decltype(std::get<idx>(std::tuple<DBs...>()))>>
  bool put(typename InOut::I &&d, size_t timestamp,
           std::function<void(typename InOut::I &&, typename InOut::O &)> t =
               empty_fn) {

    worker.spawn_task(
        [this, d = std::move(d), t = std::move(t), timestamp]() mutable {
          typename InOut::O temp;
          this->ItoO(std::move(d), temp, t);
          std::unique_lock lock(this->bufferMutex);
          last_write[idx] =
              std::chrono::steady_clock::now().time_since_epoch().count();

          if (std::get<idx>(_out).size() + 1 > queue_size) {
            std::get<idx>(_out).pop_front();
          }
          std::get<idx>(_out).emplace_back(std::move(temp), timestamp);

          empty.store(false);
        });
    return true;
  }

  void show()
    requires(printable<typename DBs::O> && ...)
  {
    std::cout << "--------------------------------------------------\n";
    for (auto i : std::views::iota(0, (int)queue_size)) {
      constexpr auto seq = std::make_index_sequence<DBs_size>{};
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        std::cout << "Element: " << i << "\n";
        std::cout << "idx: |" << std::setw(15) << "val:" << " | "
                  << std::setw(12) << "timestamp:" << "\n";
        std::cout << "--------------------------------------------------\n";
        (
            [&]<size_t idx>() {
              if (i < std::get<idx>(_out).size()) {
                auto &[f, s] = std::get<idx>(_out)[i];
                std::cout << std::setw(4) << idx << " | " << std::setw(14) << f
                          << " | " << std::setw(15) << s << "\n";
              } else {
                std::cout << std::setw(4) << idx << " | " << std::setw(14)
                          << " empty" << " |\n";
              }
            }.template operator()<Is>(),
            ...);
      }(seq);
      std::cout << "--------------------------------------------------\n";
    }
  }

private:
  template <std::size_t... Is> constexpr auto subtuple() {
    constexpr std::tuple<std::optional<typename DBs::O>...> tuple{};
    return std::make_tuple(std::get<Is>(tuple)...);
  }

  template <typename I, typename O>
  void ItoO(I &&iTypeData, O &oTypeData,
            const std::function<void(I &&, O &)> &t = empty_fn) {
    if constexpr (std::is_same<I, O>::value ||
                  std::is_convertible<I, O>::value) {
      oTypeData = std::move(iTypeData);
    } else if constexpr (is_iterable<I>::value && is_iterable<O>::value) {
      using I_T = typename std::decay<decltype(*iTypeData.begin())>::type;
      using O_T = typename std::decay<decltype(*oTypeData.begin())>::type;
      if constexpr (std::is_convertible<I_T, O_T>::value) {
        oTypeData = O(std::make_move_iterator(iTypeData.begin()),
                      std::make_move_iterator(iTypeData.end()));
      } else {
        static_assert(!std::is_same<decltype(t), decltype(empty_fn)>::value,
                      "A function needs to be implemented to transform ItoO");
        t(std::move(iTypeData), oTypeData);
      }
    } else {
      static_assert(!std::is_same<decltype(t), decltype(empty_fn)>::value,
                    "A function needs to be implemented to transform ItoO");
      t(std::move(iTypeData), oTypeData);
    }
  };
};
