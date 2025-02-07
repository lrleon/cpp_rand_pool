//
// Created by lrleon on 2/7/25.
//

#ifndef RAND_POLL_H
#define RAND_POLL_H

# include <gsl/gsl_rng.h>

#include <iostream>
#include <vector>
#include <ranges>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

using namespace std;

class RandomNumberPool
{
  // parallel arrays
  vector<vector<double>> pool; // true means available
  vector<bool> available;

  // arrays that are being used by clients and later refilled
  vector<vector<double>*> pending_arrays;

  mutex mtx;
  condition_variable cv;
  thread refiller;
  atomic<bool> stop;
  gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);

  public:
    const size_t pool_size;
    const size_t array_size;
    const unsigned long seed;

    RandomNumberPool(const size_t pool_size, const size_t array_size,
                     const unsigned long seed)
      : stop(false), pool_size(pool_size), array_size(array_size),
        seed(seed)
    {
      gsl_rng_set(rng, seed);
      for (size_t i = 0; i < pool_size; ++i)
        {
          pool.emplace_back(array_size);
          available.push_back(true);
          fill_with_random_numbers(pool[i]);
        }
      refiller = thread(&RandomNumberPool::refill_loop, this);
    }

    ~RandomNumberPool()
    {
      stop = true;
      cv.notify_all();
      refiller.join();
      gsl_rng_free(rng);
    }

    // When i need an random buffer, I call this method.
    // It throws an exception if no buffer is available
    vector<double>& lock_array()
    {
      unique_lock<mutex> lock(mtx);
      cv.wait(lock, [this]()
      {
        for (size_t i = 0; i < pool_size; ++i)
          if (available[i])
            return true;
        return false;
      });

      for (size_t i = 0; i < pool_size; ++i)
        if (available[i])
          {
            available[i] = false;
            pending_arrays.push_back(&pool[i]);
            return pool[i];
          }

      throw runtime_error("No available array");
    }

    // Once I have used an array, I call this method for disposing it
    // and refilling it with fresh random numbers
    void release_array(const vector<double> &array)
    {
      unique_lock lock(mtx);
      for (size_t i = 0; i < pool_size; ++i)
        {
          if (&pool[i] == &array)
            {
              available[i] = true; // this will wake up the refiller once it is signaled
              // notify the refiller, the pending_arrays should contain the array
              // that was just released and the refiller should refill it
              cv.notify_one();
              return;
            }
        }
      throw runtime_error("Array not found in pool");
    }

  private:
    void fill_with_random_numbers(vector<double> &array) const
    {
      cout << "Filling array with random numbers" << endl;
      for (size_t i = 0; i < array_size; ++i)
        array[i] = gsl_rng_uniform(rng);
      cout << "Array filled" << endl;
    }

    void refill_loop()
    {
      while (not stop)
        {
          unique_lock<mutex> lock(mtx);
          cv.wait(lock, [this]()
          {
            return (not stop) and
                   (not all_of(available.begin(), available.end(),
                               [](const bool b) { return b; }));
          });

          if (stop)
            return;

          for (size_t i = 0; i < pool_size; ++i)
            {
              if (available[i] and
                  ranges::find(pending_arrays, &pool[i]) != pending_arrays.end())
                {
                  available[i] = false;
                  lock.unlock();
                  fill_with_random_numbers(pool[i]);
                  lock.lock();
                  available[i] = true;
                  pending_arrays.erase(ranges::find(pending_arrays, &pool[i]));
                  //cv.notify_one();
                }
            }
        }
    }
};

#endif // RAND_POLL_H
