use criterion::{criterion_group, criterion_main, Criterion};
use nautust::*;

fn single_benchmark(c: &mut Criterion) {
    let mut nau = Nauty::<{ NtoM::<3>() }>::new();

    c.bench_function("compute orbits", |b| b.iter(|| nau.compute()));
}

criterion_group!(benches, single_benchmark,);
criterion_main!(benches);
