use nautust::*;

const N: Node = 3;
const M: ConstT = NtoM::<N>();

fn main() {
    const DEBUG: bool = false;
    let mut nau = Nauty::<M, DEBUG>::new();

    nau.recurse();

    nau.print_scores();
}
