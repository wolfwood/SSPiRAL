use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};
use sspiral::{check_if_alive, Layout, LayoutIter, N};

fn single_benchmark(c: &mut Criterion) {
    c.bench_function("check_if_alive 0xFF0", |b| {
        b.iter(|| check_if_alive(Layout::from(black_box(0xFF0))))
    });
}

fn sweep_benchmark(c: &mut Criterion) {
    if N <= 4 {
        c.bench_function("check_if_alive sweep", |b| {
            b.iter(|| {
                for i in LayoutIter::new() {
                    check_if_alive(black_box(i));
                }
            })
        });
    }
}

fn sweep_group_benchmark(c: &mut Criterion) {
    if N <= 3 {
        let mut group = c.benchmark_group("check_if_alive group sweep");

        for l in LayoutIter::new() {
            group.bench_with_input(BenchmarkId::from_parameter(l), &l, |b, &l| {
                b.iter(|| check_if_alive(l))
            });
        }
        group.finish();
    }
}

criterion_group!(
    benches,
    single_benchmark,
    sweep_benchmark,
    sweep_group_benchmark
);
criterion_main!(benches);
