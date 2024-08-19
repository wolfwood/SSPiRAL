use nautust::*;

const N: Node = 3;
const M: ConstT = NtoM::<N>();

fn main() {
    let mut nau = Nauty::<M>::new();

    nau.compute();

    nau.print();

    nau.options.defaultptn = 0;

    let mut x = 0;
    // implicit copy of orbits
    for i in nau.orbits {
        if i >= x {
            x = i + 1;

            nau.ptn.fill(1);
            nau.ptn[0] = 0;
            *nau.ptn.last_mut().unwrap() = 0;

            nau.lab[0] = i;

            let bet = revert::<M>(i);
            for j in 1..=nau.lab.len() {
                let k = invert::<M>(j as i32);

                match (j as i32).cmp(&bet) {
                    std::cmp::Ordering::Less => nau.lab[j] = k,
                    std::cmp::Ordering::Greater => nau.lab[j - 1] = k,
                    std::cmp::Ordering::Equal => (),
                }
            }

            nau.compute();

            nau.print();
        }
    }
}
