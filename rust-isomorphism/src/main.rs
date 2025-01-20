use nautust::*;

const N: Node = 3;
const M: ConstT = NtoM::<N>();

fn main() {
    let mut nau = Nauty::<M, true>::new();

    nau.recurse();

    nau.print_scores();
}
