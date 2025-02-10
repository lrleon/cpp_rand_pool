use std::sync::{Arc, Mutex, Condvar};
use std::thread;
use rand::Rng;

struct RandomNumberPool {
    pool: Vec<Vec<f64>>,
    available: Vec<bool>,
    pending_arrays: Vec<Vec<f64>>,
    mtx: Mutex<()>,
    cv: Condvar,
    stop: Arc<Mutex<bool>>,
    rng: rand::ThreadRng,
}

impl RandomNumberPool {
    fn new(pool_size: usize, array_size: usize, seed: u64) -> Self {
        let mut rng = rand::SeedableRng::from_seed(seed);
        let mut pool = Vec::with_capacity(pool_size);
        let mut available = Vec::with_capacity(pool_size);
        for _ in 0..pool_size {
            let mut array = Vec::with_capacity(array_size);
            for _ in 0..array_size {
                array.push(rng.gen::<f64>());
            }
            pool.push(array);
            available.push(true);
        }
        let stop = Arc::new(Mutex::new(false));
        let stop_clone = stop.clone();
        let cv = Condvar::new();
        let mtx = Mutex::new(());
        let pending_arrays = Vec::new();
        let rng = rand::thread_rng();
        let refiller = thread::spawn(move || {
            Self::refill_loop(&stop_clone, &cv, &mtx, &mut pool, &mut available, &mut pending_arrays, &rng);
        });
        Self {
            pool,
            available,
            pending_arrays,
            mtx,
            cv,
            stop,
            rng,
        }
    }

    fn refill_loop(stop: &Arc<Mutex<bool>>, cv: &Condvar, mtx: &Mutex<()>, pool: &mut Vec<Vec<f64>>, available: &mut Vec<bool>, pending_arrays: &mut Vec<Vec<f64>>, rng: &rand::ThreadRng) {
        loop {
            {
                let mut stop_lock = stop.lock().unwrap();
                if *stop_lock {
                    break;
                }
            }
            cv.wait(mtx.lock().unwrap()).unwrap();
            for i in 0..pool.len() {
                if !available[i] {
                    for j in 0..pool[i].len() {
                        pool[i][j] = rng.gen::<f64>();
                    }
                    available[i] = true;
                }
            }
        }
    }

    fn lock_array(&self) -> Vec<f64> {
        let mut mtx_lock = self.mtx.lock().unwrap();
        while self.available.iter().all(|&x| !x) {
            self.cv.wait(mtx_lock).unwrap();
        }
        for i in 0..self.available.len() {
            if self.available[i] {
                self.available[i] = false;
                return self.pool[i].clone();
            }
        }
        unreachable!();
    }

    fn release_array(&self, array: Vec<f64>) {
        let mut mtx_lock = self.mtx.lock().unwrap();
        for i in 0..self.pool.len() {
            if self.pool[i] == array {
                self.available[i] = true;
                self.cv.notify_one();
                return;
            }
        }
        panic!("Array not found in pool");
    }
}