use nauty_Traces_sys::*;
use std::io::{self, Write};
use std::os::raw::c_int;

type Node = i32;

const N: i8 = 3;
const M: Node = 2_i32.pow(N as u32) - 1;

fn invert(node: Node) -> Node {
    !node & M
    //node -1
}

fn revert(node: Node) -> Node {
    !node & M
    //node + 1
}

struct Nauty {
    g: [graph; M as usize],
    lab: [c_int; M as usize],
    ptn: [c_int; M as usize],
    orbits: [c_int; M as usize],
    options: optionblk,
    stats: statsblk,
}

impl Nauty {
    fn new() -> Nauty {
        let o = SETWORDSNEEDED(N as usize);

        unsafe {
            nauty_check(
                WORDSIZE as c_int,
                o as c_int,
                N as c_int,
                NAUTYVERSIONID as c_int,
            );
        }

        Nauty {
            g: Nauty::full_graph().try_into().unwrap(),
            lab: [0 as c_int; M as usize],
            ptn: [0 as c_int; M as usize],
            orbits: [0 as c_int; M as usize],
            options: optionblk::default(),
            stats: statsblk::default(),
        }
    }

    fn full_graph() -> Vec<graph> {
        let o = SETWORDSNEEDED(N as usize);

        let mut g = empty_graph(o, M as usize);

        for i in 1..=N {
            let v = 1 << (i - 1);
            for u in (v + 1)..=M {
                if (v) & (u) == (v) {
                    ADDONEEDGE(&mut g, invert(v) as usize, invert(u) as usize, o);
                    //println!("{} -> {}", v, u);
                }
            }
        }

        g
    }

    fn compute(&mut self) {
        let o = SETWORDSNEEDED(N as usize);
        unsafe {
            densenauty(
                self.g.as_mut_ptr(),
                self.lab.as_mut_ptr(),
                self.ptn.as_mut_ptr(),
                self.orbits.as_mut_ptr(),
                &mut self.options,
                &mut self.stats,
                o as c_int,
                M,
                std::ptr::null_mut(),
            );
        }
    }

    fn print(&self) {
        if true {
            print!("[");
            for &l in self.lab.iter() {
                print!("{} ", revert(l));
            }
            println!("]");

            print!("[");
            for p in self.ptn.iter() {
                print!("{} ", p);
            }
            println!("]");
        }

        print!("[");
        for &orbit in self.orbits.iter() {
            print!("{} ", revert(orbit));
        }
        println!("]");

        println!("num orbits = {}", self.stats.numorbits);
        print!("order = ");
        io::stdout().flush().unwrap();
        unsafe {
            writegroupsize(stderr, self.stats.grpsize1, self.stats.grpsize2);
        }
        println!();
    }
}

fn main() {
    let mut nau = Nauty::new();

    nau.compute();

    nau.print();

    nau.options.defaultptn = 0;

    let mut x = 0;
    for i in nau.orbits.clone() {
        if i >= x {
            x = i + 1;

            nau.ptn.fill(1);
            nau.ptn[0] = 0;
            *nau.ptn.last_mut().unwrap() = 0;

            nau.lab[0] = i;

            let bet = revert(i);
            for j in 1..=nau.lab.len() {
                let k = invert(j as i32);

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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_invert_revert() {
        for i in 1..=M {
            assert_eq!(i, revert(invert(i)))
        }
    }
}
