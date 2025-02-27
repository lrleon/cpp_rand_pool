# Random Number Pool

A Rust library that provides efficient pooling of random number arrays for high-performance applications.

## Overview

`RandomNumberPool` is a thread-safe utility that pre-generates and manages pools of random number arrays. It's designed to minimize overhead in applications that require frequent access to random numbers by maintaining a pool of pre-generated arrays.

Key features:
- Thread-safe access to pre-generated random numbers
- Background refilling of used arrays
- Configurable pool size, array size, and seed
- Efficient resource management

## Use Cases

- Simulations and modeling that require large quantities of random numbers
- Performance-critical applications where generating random numbers on-demand could create bottlenecks
- Multi-threaded applications that need concurrent access to random data

## Installation

Add this to your `Cargo.toml`:

```toml
[dependencies]
rand_pool = "0.1.2"
```

## Basic Usage

```rust
use rand_pool::rand_pool;

fn main() {
    // Create a pool with 10 arrays, each containing 1000 random numbers,
    // using seed 42 for reproducibility
    let pool = rand_pool::RandomNumberPool::new(10, 1000, 42);
    
    // Get an array and its index from the pool
    let (index, array) = pool.get_array().unwrap();
    
    // Use the random numbers
    for (i, &num) in array.iter().enumerate().take(5) {
        println!("Random number {}: {}", i, num);
    }
    
    // Release the array back to the pool for refilling
    pool.release_array(index);
}
```

## How It Works

1. The pool maintains three collections of array indices:
   - Available arrays: ready for immediate use
   - In-use arrays: currently being used by the application
   - Refilling arrays: being repopulated with new random numbers

2. When you request an array:
   - An available array is provided and moved to the in-use collection
   - If no arrays are available, the request blocks until one becomes available

3. When you release an array:
   - It's moved to the refilling collection
   - A background thread generates new random numbers for it
   - Once refilled, it's moved back to the available collection

4. When the pool is dropped:
   - All resources are properly cleaned up
   - The refiller thread is signaled to terminate

## Configuration

The `RandomNumberPool` constructor accepts three parameters:

- `pool_size`: Number of arrays to maintain in the pool
- `array_size`: Number of random numbers in each array
- `seed`: Seed value for the random number generator (for reproducibility)

## Thread Safety

All operations on the pool are thread-safe, making it suitable for multi-threaded applications. The implementation uses standard Rust synchronization primitives:

- `Mutex` for controlling access to shared data
- `Condvar` for signaling between threads
- `Arc` for shared ownership across threads

## Performance Considerations

- Larger pool sizes reduce the chance of blocking when requesting arrays
- Smaller array sizes enable faster refilling
- Consider your application's usage patterns when configuring pool and array sizes

## License

This project is licensed under the MIT License.

## Contributing

Leandro Leon

Freddy Cuellar