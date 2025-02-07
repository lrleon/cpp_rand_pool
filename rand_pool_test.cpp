//
// Created by lrleon on 2/7/25.
//

# include <gtest/gtest.h>

# include "rand-pool.H"

void use_random_array(const vector<double> &array)
{
  double sum = 0.0;
  for (size_t i = 0; i < array.size(); ++i)
    sum += array[i];

  cout << "Sum: " << sum << endl;
}

TEST(basic, ctor)
{
  auto pool =
    RandomNumberPool(10, 5, 0);

  EXPECT_EQ(pool.pool_size, 10);
  EXPECT_EQ(pool.array_size, 5);
  EXPECT_EQ(pool.seed, 0);

  vector<vector<double>*> locked_arrays;
  vector<thread> threads;

  // lock two arrays and use them in use_random_array threads
  for (size_t i = 0; i < 2; ++i)
    {
      locked_arrays.push_back(&pool.lock_array());
      threads.emplace_back(use_random_array, ref(*locked_arrays.back()));
    }

  cout << "Locked " << locked_arrays.size() << " arrays" << endl
    << "Waiting for threads to finish" << endl;

  for (auto &t: threads)
    t.join();

  cout << "Threads finished" << endl;

  // unlock the arrays
  for (size_t i = 0; i < locked_arrays.size(); ++i)
    pool.release_array(*locked_arrays[i]);
}

struct BigPool : public testing::Test
{
  RandomNumberPool pool =
    RandomNumberPool(10, 200000, 0);

  atomic<bool> stop;

  // thread that constantly locks an array and uses it
  void use_random_array()
  {
    while (not stop)
      {
        const auto &array = pool.lock_array();
        double sum = 0.0;
        for (size_t i = 0; i < array.size(); ++i)
          sum += array[i];

        cout << "Sum: " << sum << endl;
        pool.release_array(array);
      }
  }
};

TEST_F(BigPool, ctor)
{
  EXPECT_EQ(pool.pool_size, 10);
  EXPECT_EQ(pool.array_size, 200000);
  EXPECT_EQ(pool.seed, 0);

  vector<vector<double>*> locked_arrays;
  vector<thread> threads;

  // instantiate 9 threads that constantly lock and use an array
  for (size_t i = 0; i < 9; ++i)
    threads.emplace_back(&BigPool::use_random_array, this);

  // keep thread running for 1 minute
  this_thread::sleep_for(1min);

  // stop the threads
  stop = true;
  for (auto &t: threads)
    t.join();

  cout << "Threads finished" << endl;

  // unlock the arrays
  for (size_t i = 0; i < locked_arrays.size(); ++i)
    pool.release_array(*locked_arrays[i]);
}
