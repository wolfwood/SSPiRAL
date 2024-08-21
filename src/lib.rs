use nauty_Traces_sys::*;
use std::collections::HashSet;
use std::io::{self, Write};
use std::os::raw::c_int;

// this is the type required by Nauty vertices
pub type Node = i32;
// M must be a usize to set array bounds until generic_const_exprs stabilizes
// we'll have to downcast to Node in places instead of upcasting to usize
pub type ConstT = usize;

// we don't define N and M in the library but callers need at least one, which can generate the other
// having the type of N and M be different helps prevent mistakes
#[allow(non_snake_case)]
pub const fn NtoM<const N: Node>() -> ConstT {
    2_usize.pow(N as u32) - 1
}

#[allow(non_snake_case)]
pub const fn MtoN<const M: ConstT>() -> Node {
    M.count_ones() as Node
}

pub fn invert<const M: ConstT>(node: Node) -> Node {
    !node & M as Node
    //node -1
}

pub fn revert<const M: ConstT>(node: Node) -> Node {
    !node & M as Node
    //node + 1
}

pub struct Nauty<const M: ConstT> {
    g: [graph; M],
    lab: [c_int; M],
    ptn: [c_int; M],
    orbits: [c_int; M],
    options: optionblk,
    stats: statsblk,
}

impl<const M: ConstT> Nauty<M> {
    // SETWORDSNEEDED(m) is nonconst so do it ourselves
    const WORDS: usize = M / WORDSIZE as usize + if M % WORDSIZE as usize == 0 { 0 } else { 1 };

    pub fn new() -> Nauty<M> {
        unsafe {
            nauty_check(
                WORDSIZE as c_int,
                Self::WORDS as c_int,
                M as c_int,
                NAUTYVERSIONID as c_int,
            );
        }

        Nauty {
            g: Nauty::<M>::full_graph().try_into().unwrap(),
            lab: [0 as c_int; M],
            ptn: [0 as c_int; M],
            orbits: [0 as c_int; M],
            options: optionblk::default(),
            stats: statsblk::default(),
        }
    }

    fn full_graph() -> Vec<graph> {
        let mut g = empty_graph(Self::WORDS, M);

        // this will be a const when we have generic_const_exprs
        #[allow(non_snake_case)]
        let N = MtoN::<M>();

        for i in 1..=N {
            let v = 1 << (i - 1);
            for u in (v + 1)..=M as Node {
                if (v) & (u) == (v) {
                    ADDONEEDGE(
                        &mut g,
                        invert::<M>(v) as usize,
                        invert::<M>(u) as usize,
                        Self::WORDS,
                    );
                    //println!("{} -> {}", v, u);
                }
            }
        }

        g
    }

    pub fn compute(&mut self) {
        unsafe {
            densenauty(
                self.g.as_mut_ptr(),
                self.lab.as_mut_ptr(),
                self.ptn.as_mut_ptr(),
                self.orbits.as_mut_ptr(),
                &mut self.options,
                &mut self.stats,
                Self::WORDS as c_int,
                M as c_int,
                std::ptr::null_mut(),
            );
        }
    }

    pub fn print(&self) {
        if true {
            print!("[");
            for &l in self.lab.iter() {
                print!("{} ", revert::<M>(l));
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
            print!("{} ", revert::<M>(orbit));
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

    pub fn recurse(&mut self) {
        self.options.defaultptn = 0;

        self.ptn.fill(1);
        *self.ptn.last_mut().unwrap() = 0;

        self._recurse(0);
    }

    fn _recurse(&mut self, prev: usize) {
        self.ptn[prev] = 0;

        let unique_orbits = self.orbits.into_iter().collect::<HashSet<_>>().into_iter();
        // for debugging
        //let mut unique_orbits = unique_orbits.collect::<Vec<_>>();
        //unique_orbits.sort();

        for o in unique_orbits {
            for i in 0..M {
                if self.lab[i] == o {
                    self.lab[i] = self.lab[0];
                    self.lab[0] = o;
                    break;
                }
            }

            self.compute();

            self.print();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_invert_revert() {
        const M: ConstT = NtoM::<5>();

        for i in 1..=M as Node {
            assert_eq!(i, revert::<M>(invert::<M>(i)))
        }
    }

    #[test]
    fn test_words() {
        seq_macro::seq!(N in 1..=31 {
            assert_eq!(SETWORDSNEEDED(NtoM::<N>()), Nauty::<{NtoM::<N>()}>::WORDS);
        });
    }

    #[test]
    #[allow(non_snake_case)]
    fn test_NtoMtoN() {
        seq_macro::seq!(N in 1..=31 {
            assert_eq!(N as i32, MtoN::<{NtoM::<N>()}>() );
        });
    }
}
