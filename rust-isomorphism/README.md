# nautust
An alternative approach to enumerating SSPiRAL Metalayouts, using the Nauty graph isomorphism solver.

## Running
tests can be run with `cargo test` and the benchmarks used to evaluate performance related code changes can be run with `cargo bench`.

I recommend using `cargo build --release` to build, the result will be `./target/release/nautust`.

N can be set and DEBUG toggled at compile time in `src/main.rs`.

# Status
I believe the recursive enumeration is exhaustive. I would like to add a test using the other rust codebase to also enumerate metalatouts in order to demonstrate that.

I was hoping to be able to simply reproduce the Scores table for the full layout as the other simulators do, but the way that I am preventing redundant child metalayout visitation seemingly prevents me from calculating the number of collapsed layouts properly. In the regular algorithm the influence of a layout on a grandparent's reliability gets counted multiple times, but a predictable number that is corrected for with the normalization step. The pruning that is the core of this approach for performance reasons means the multiple counting is no longer predictable.
