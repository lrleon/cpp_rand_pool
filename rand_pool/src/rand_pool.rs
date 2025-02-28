use rand::prelude::*;
use rand::rngs::SmallRng;
use rand::SeedableRng;
use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::{array, thread};

#[derive(Debug)]
pub struct RandomNumberPool {
    available_arrays: Arc<Mutex<VecDeque<usize>>>, // indices of available arrays
    in_use_arrays: Arc<Mutex<VecDeque<usize>>>,    // indices of arrays in use
    refilling_arrays: Arc<Mutex<VecDeque<usize>>>, // indices of arrays being refilled
    available_cv: Arc<Condvar>,
    refilling_cv: Arc<Condvar>,
    stop: Arc<AtomicBool>,
    rng: Arc<Mutex<SmallRng>>,
    arrays: Arc<Mutex<Vec<Arc<Vec<f64>>>>>,
    pool_size: usize,
    array_size: usize,
    seed: u64,
}

impl RandomNumberPool {
    pub fn new(pool_size: usize, array_size: usize, seed: u64) -> Self {
        let available_arrays = Arc::new(Mutex::new(VecDeque::new()));
        let in_use_arrays = Arc::new(Mutex::new(VecDeque::new()));
        let refilling_arrays = Arc::new(Mutex::new(VecDeque::new()));
        let available_cv = Arc::new(Condvar::new());
        let refilling_cv = Arc::new(Condvar::new());
        let stop = Arc::new(AtomicBool::new(false));

        let rng = if seed == 0 {
            Arc::new(Mutex::new(SmallRng::from_entropy()))
        } else {
            Arc::new(Mutex::new(SmallRng::seed_from_u64(seed)))
        };

        let arrays = Arc::new(Mutex::new(Vec::new()));

        // Initialize arrays and available indices
        {
            let mut arrays_guard = arrays.lock().unwrap();
            let mut available_guard = available_arrays.lock().unwrap();

            for i in 0..pool_size {
                let mut arr = vec![0.0; array_size];
                Self::fill_with_random_numbers(&mut arr, &rng);
                arrays_guard.push(Arc::new(arr));
                available_guard.push_back(i);
            }
        }

        // Create instance
        let pool = RandomNumberPool {
            available_arrays,
            in_use_arrays,
            refilling_arrays,
            available_cv,
            refilling_cv,
            stop,
            rng,
            arrays,
            pool_size,
            array_size,
            seed,
        };

        // Start refiller thread
        pool.start_refiller();

        pool
    }

    fn fill_with_random_numbers(array: &mut Vec<f64>, rng: &Arc<Mutex<SmallRng>>) {
        // println!("Filling array with random numbers");
        let mut rng_guard = rng.lock().unwrap();
        rng_guard.fill(array.as_mut_slice());
    }

    fn start_refiller(&self) {
        let available_arrays = Arc::clone(&self.available_arrays);
        let refilling_arrays = Arc::clone(&self.refilling_arrays);
        let available_cv = Arc::clone(&self.available_cv);
        let refilling_cv = Arc::clone(&self.refilling_cv);
        let stop = Arc::clone(&self.stop);
        let arrays = Arc::clone(&self.arrays);
        let rng = Arc::clone(&self.rng);
        let array_size = self.array_size;

        thread::spawn(move || {
            while !stop.load(Ordering::Relaxed) {
                let idx = {
                    let mut refilling_guard = refilling_arrays.lock().unwrap();

                    // Wait for arrays to refill or stop signal
                    while refilling_guard.is_empty() && !stop.load(Ordering::Relaxed) {
                        refilling_guard = refilling_cv.wait(refilling_guard).unwrap();
                    }

                    if stop.load(Ordering::Relaxed) {
                        // println!("Refiller thread stopping");
                        return;
                    }

                    // println!("Refiller thread woken");
                    refilling_guard.pop_front()
                };

                if let Some(i) = idx {
                    // println!("Refilling arrays");
                    let mut new_arr = vec![0.0; array_size];
                    Self::fill_with_random_numbers(&mut new_arr, &rng);
                    {
                        let mut arrays_guard = arrays.lock().unwrap();
                        arrays_guard[i] = Arc::new(new_arr);
                    }

                    {
                        let mut available_guard = available_arrays.lock().unwrap();
                        available_guard.push_back(i);
                    }

                    available_cv.notify_one();
                    // println!("Refilled array {}", i);
                }
            }
            println!("Refiller thread finished");
        });
    }

    pub fn get_array(&self) -> Result<(usize, Arc<Vec<f64>>), &'static str> {
        let mut available_guard = self.available_arrays.lock().unwrap();

        loop {
            if self.stop.load(Ordering::Relaxed) {
                // return Err("Pool is being destroyed");
            }

            if let Some(i) = available_guard.pop_front() {
                let mut in_use_guard = self.in_use_arrays.lock().unwrap();
                in_use_guard.push_back(i);
                // println!("Locking array {}", i);
                let array = self.arrays.lock().unwrap();
                return Ok((i, array[i].clone()));
            }

            available_guard = self.available_cv.wait(available_guard).unwrap();
        }
    }

    pub fn release_array(&self, idx: usize) {
        let mut in_use_guard = self.in_use_arrays.lock().unwrap();
        if let Some(pos) = in_use_guard.iter().position(|&x| x == idx) {
            in_use_guard.remove(pos);
        }

        let mut refilling_guard = self.refilling_arrays.lock().unwrap();
        refilling_guard.push_back(idx);
        drop(refilling_guard);

        self.refilling_cv.notify_one();
        // println!("Releasing array {}", idx);
    }
}

impl Drop for RandomNumberPool {
    fn drop(&mut self) {
        // println!("Destroying pool");
        self.stop.store(true, Ordering::Relaxed);
        self.available_cv.notify_all();
        // println!("Notified available_cv");
        self.refilling_cv.notify_one();
        // println!("Notified refilling_cv");
        // println!("Pool destroyed");
    }
}

impl Default for RandomNumberPool {
    fn default() -> Self {
        Self::new(10, 10, 0)
    }
}
