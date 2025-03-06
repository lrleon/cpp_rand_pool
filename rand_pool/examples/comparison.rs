// cargo run --release --example comparison 100000
use rand::prelude::*;
use rand_pool;
fn main() {
    let total = std::env::args()
        .nth(1)
        .unwrap_or("100000".to_string())
        .parse()
        .unwrap();
    println!("Generate 1_00_000 random numbers with rand::rngs::SmallRng");
    generate_with_small_rand(total);
    println!("Generate 1_00_000 random numbers with rand_pool::rand_pool::RandomNumberPool");
    generate_with_rand_pool(total);
}

fn generate_with_small_rand(total: usize) {
    let mut generator = rand::rngs::SmallRng::from_entropy();
    let mut sum = 0.0;
    let start = std::time::Instant::now();
    for _ in 0..total {
        sum = generator.gen::<f64>();
    }
    let elapsed = start.elapsed();
    println!("elapsed: {:?}", elapsed);
    println!("sum: {}", sum);
}

fn generate_with_rand_pool(total: usize) {
    let pool = rand_pool::rand_pool::RandomNumberPool::new(10, total, 0);
    let mut sum = 0.0;
    let (index, arr) = pool.get_array().unwrap();
    let start = std::time::Instant::now();
    for index in 0..total {
        sum = unsafe { *arr.get_unchecked(index) };
    }
    let elapsed = start.elapsed();
    println!("elapsed: {:?}", elapsed);
    println!("sum: {}", sum);
}
