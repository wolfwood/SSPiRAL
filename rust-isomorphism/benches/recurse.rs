use criterion::{criterion_group, criterion_main, BenchmarkId, Criterion};
use nautust::*;
use seq_macro;

fn single_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("recurse");

    seq_macro::seq!(N in 3..=4 {
        let mut nau = Nauty::<{ NtoM::<N>() }>::new();

        group.bench_function(BenchmarkId::from_parameter(N), |b| {
            b.iter(|| nau.recurse())
        });
    });
}

criterion_group!(benches, single_benchmark,);

criterion_main!(benches);
