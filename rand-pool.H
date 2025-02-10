//
// Created by lrleon on 2/7/25.
//

#ifndef RAND_POLL_H
#define RAND_POLL_H

# include <gsl/gsl_rng.h>

#include <functional>
#include <iostream>
#include <vector>
# include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

using namespace std;

class RandomNumberPool
{
  list<size_t> available_arrays; // indices of available arrays
  list<size_t> in_use_arrays; // indices of arrays in use
  list<size_t> refilling_arrays; // indices of arrays being refilled

  // parallel arrays
  vector<vector<double>> pool; // true means states

  mutex mtx;
  condition_variable available_cv;
  condition_variable refilling_cv;

  thread refiller;
  atomic<bool> stop = false;
  gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);

 public:
  const size_t pool_size;
  const size_t array_size;
  const unsigned long seed;

  RandomNumberPool(const size_t pool_size, const size_t array_size,
                   const unsigned long seed)
    : pool_size(pool_size), array_size(array_size),
      seed(seed)
  {
    gsl_rng_set(rng, seed);
    for (size_t i = 0; i < pool_size; ++i)
      {
        pool.emplace_back(array_size);
        available_arrays.push_back(i);
        fill_with_random_numbers(pool[i]);
      }
    refiller = thread(&RandomNumberPool::refill_loop, this);
  }

  ~RandomNumberPool()
  {
    stop = true;

    cout << "Destroying pool" << endl;

    available_cv.notify_all();
    cout << "Notified available_cv" << endl;

    refilling_cv.notify_one();
    cout << "Notified refilling_cv" << endl;

    cout << "Waiting for refiller to finish" << endl;
    refiller.join();
    cout << "Refiller finished" << endl;
    gsl_rng_free(rng);
  }

  // When I need a random buffer, I call this method.
  // It throws an exception if no buffer is states
  // Extremely careful with this method:
  // 1) It blocks until a buffer is states (all buffers are in use)
  // 2) It does not check if the buffer is already in use by another thread
  // 3) It does not check if the pool is being destroyed
  vector<double> &lock_array()
  {
    int i = 0;
    unique_lock<mutex> lock(mtx);
    available_cv.wait(lock, [this, &i]()
    {
      if (stop)
        return true;

      if (available_arrays.empty())
        return false;

      i = available_arrays.front();
      available_arrays.pop_front();
      in_use_arrays.push_back(i);
      return true;
    });

    if (stop)
      throw runtime_error("Pool is being destroyed");

    cout << "Locking array " << i << endl;

    return pool[i];
  }

  // Once I have used an array, I call this method for disposing it
  // and refilling it with fresh random numbers
  void release_array(const vector<double> &array)
  {
    const size_t idx = &array - &pool[0];
    unique_lock lock(mtx);
    in_use_arrays.remove(idx);
    refilling_arrays.push_back(idx);
    refilling_cv.notify_one();
    cout << "Releasing array " << idx << endl;
  }

 private:
  void fill_with_random_numbers(vector<double> &array) const
  {
    cout << "Filling array with random numbers" << endl;
    for (size_t i = 0; i < array_size; ++i)
      array[i] = gsl_rng_uniform(rng);
    cout << "Array filled" << endl;
  }

  void refill_loop() // THis thread refills the arrays
  {
    while (not stop)
      {
        vector<size_t> to_refill_indices;
        unique_lock<mutex> lock(mtx);
        refilling_cv.wait(lock, [this]()// blocks until there are arrays to refill or the pool is being destroyed
        {
          if (stop)
            return true;

          return not refilling_arrays.empty();
        });

        cout << "Refiller thread woken" << endl;

        if (stop)
          {
            cout << "Refiller thread stopping" << endl;
            return;
          }

        cout << "Refilling arrays" << endl;
        int i = refilling_arrays.front();
        refilling_arrays.pop_front();

        fill_with_random_numbers(pool[i]);

        available_arrays.push_back(i);
        available_cv.notify_one();
        cout << "Refilled array " << i << endl;
      }
      cout << "Refiller thread finished" << endl;
  }
};

#endif // RAND_POLL_H
