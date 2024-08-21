use nautust::*;

const N: Node = 3;
const M: ConstT = NtoM::<N>();

fn main() {
    let mut nau = Nauty::<M>::new();

    nau.recurse();
}
