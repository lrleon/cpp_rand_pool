//
// Created by lrleon on 2/7/25.
//

# include <gtest/gtest.h>

# include <tuple>
# include <chrono>
# include <numeric>
# include <ranges>
#include <bits/ranges_algo.h>

# include "rand-pool.H"

using namespace std;
using namespace std::chrono;

void use_random_array(const vector<double> &array)
{
  const double sum =
    accumulate(array.begin(), array.end(), 0.0);

  cout << "Sum: " << sum << endl;
}

TEST(basic, ctor)
{
  auto pool =
    RandomNumberPool(10, 5, 0);

  EXPECT_EQ(pool.pool_size, 10);
  EXPECT_EQ(pool.array_size, 5);
  EXPECT_EQ(pool.seed, 0);

  vector<size_t> indices;
  vector<thread> threads;

  // lock two arrays and use them in use_random_array threads
  for (size_t i = 0; i < 2; ++i)
    {
      const size_t idx = pool.lock_array();
      vector<double> &array = pool.arrays[idx];
      indices.push_back(idx);
      threads.emplace_back(use_random_array, ref(array));
    }

  cout << "Locked " << indices.size() << " arrays" << endl
    << "Waiting for threads to finish" << endl;

  for (auto &t: threads)
    t.join();

  cout << "Threads finished" << endl;

  // unlock the arrays
  ranges::for_each(indices, [&pool](const size_t idx)
                   {
                     pool.release_array(idx);
                   });

  cout << "Arrays released" << endl;
}

struct BigPool : public testing::Test
{
  static constexpr size_t Array_Size = 200000;
  static constexpr size_t Num_Threads = 10;
  RandomNumberPool pool =
    RandomNumberPool(Num_Threads + 1, Array_Size, 0);

  atomic<bool> stop;

  long avg_sum;
  long median_sum;
  long min_sum;
  long max_sum;

  long avg_lock;
  long median_lock;
  long min_lock;
  long max_lock;

  mutable long threshold = -1;

  void SetUp() override
  {
    stop = false;
    benchmark(Num_Threads, Array_Size);
  }

  void TearDown() override
  {
    // empty
  }

  // avg_sum, median_sum, min_sum, max_sum
  void benchmark(const size_t num_samples,
                 const size_t n)
  {
    vector<double> array(n);
    for (size_t i = 0; i < n; ++i)
      array[i] = i + 1;

    // first we measure the time it takes to sum the array
    vector<microseconds> durations;
    double sum = 0.0;
    for (size_t i = 0; i < num_samples; ++i)
      {
        const auto start = chrono::high_resolution_clock::now();
        for (size_t j = 0; j < n; ++j)
          sum += array[j];
        const auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
        cout << "Iteration " << i << " took " << duration.count() << " us" << endl;
        durations.push_back(duration);
      }

    // now calculate the average, median_sum, min_sum, and max_sum
    ranges::sort(durations);
    avg_sum = accumulate(durations.begin(), durations.end(), 0L,
                         [](const long a, const microseconds &b)
                         {
                           return a + b.count();
                         }) / num_samples;
    median_sum = durations[num_samples / 2].count();
    min_sum = durations[0].count();
    max_sum = durations[num_samples - 1].count();

    // Now we measure the time it takes to lock the array
    vector<microseconds> lock_durations;
    for (size_t i = 0; i < num_samples; ++i)
      {
        const auto start = chrono::high_resolution_clock::now();
        const auto &locked_array = pool.lock_array();
        const auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
        cout << "Lock iteration " << i << " took " << duration.count() << " us" << endl;
        lock_durations.push_back(duration);
        pool.release_array(locked_array);
      }

    ranges::sort(lock_durations);
    avg_lock = accumulate(lock_durations.begin(), lock_durations.end(), 0L,
                          [](const long a, const microseconds &b)
                          {
                            return a + b.count();
                          }) / num_samples;
    median_lock = lock_durations[num_samples / 2].count();
    min_lock = lock_durations[0].count();
    max_lock = lock_durations[num_samples - 1].count();

    const long max_latency = max_sum + max_lock;
    threshold = max_latency + 0.1 * max_latency;
  }

  // thread that constantly locks an array and uses it. This thread
  void use_random_array(int &result)
  {
    while (not stop)
      {
        const auto start_lock = chrono::high_resolution_clock::now();
        const size_t idx = pool.lock_array();
        const auto &array = pool.arrays[idx];
        const auto end_lock = chrono::high_resolution_clock::now();
        const auto duration_lock = chrono::duration_cast<chrono::microseconds>(end_lock - start_lock);
        cout << "Lock took " << duration_lock.count() << " us" << endl;

        const auto start = chrono::high_resolution_clock::now();
        const double sum = accumulate(array.begin(), array.end(), 0.0);

        const auto end = chrono::high_resolution_clock::now();
        const auto duration_sum = chrono::duration_cast<chrono::microseconds>(end - start);
        cout << "Sum " << sum << "took " << duration_sum.count() << " us" << endl;

        const auto start_release = chrono::high_resolution_clock::now();
        pool.release_array(idx);
        const auto end_release = chrono::high_resolution_clock::now();
        const auto duration_release = chrono::duration_cast<chrono::microseconds>(end_release - start_release);
        cout << "Release took " << duration_release.count() << " us" << endl;

        if (auto duration = duration_lock + duration_sum + duration_release;
          duration.count() > threshold)
           {
             cout << "****************************************" << endl
               << "Thread took too long: " << duration_sum.count() << " us" << endl
               << "****************************************" << endl;
             stop = true;
             result = false;
             return;
           }

        cout << "Success" << endl;
      }
    result = true;
  }
};

TEST_F(BigPool, ctor)
{
  EXPECT_EQ(pool.pool_size, Num_Threads + 1);
  EXPECT_EQ(pool.array_size, Array_Size);
  EXPECT_EQ(pool.seed, 0);

  vector<thread> threads;
  vector<int> results(Num_Threads, true);

  cout << "Starting threads" << endl
    << "Mean: " << avg_sum << endl
    << "Median: " << median_sum << endl
    << "Min: " << min_sum << endl
    << "Max: " << max_sum << endl
    << "Threshold: " << threshold << endl;

  // instantiate Num_Threads threads that constantly lock and use an array
  for (size_t i = 0; i < Num_Threads; ++i)
    threads.emplace_back(&BigPool::use_random_array, this, ref(results[i]));

  // keep thread running for 1 minute
  this_thread::sleep_for(10s);

  // stop the threads and collect the results
  stop = true;

  for (auto &t: threads)
    t.join();

  ASSERT_TRUE(ranges::all_of(results, [](const int r) { return r; }));

  cout << "Threads finished" << endl;
}
