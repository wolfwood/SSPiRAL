use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};
use nautust::*;
use seq_macro;

fn single_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("compute orbits");

    seq_macro::seq!(N in 3..=6 {
        let mut nau = Nauty::<{ NtoM::<N>() }>::new();
        let mut lab = [0 as Node; NtoM::<N>()];

        group.bench_function(BenchmarkId::from_parameter(N), |b| {
            b.iter(|| nau.compute(black_box(&mut lab)))
        });
    });
}

criterion_group!(benches, single_benchmark,);
criterion_main!(benches);
