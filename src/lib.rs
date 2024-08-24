use arrayvec::ArrayVec;
use nauty_Traces_sys::*;
use std::io::{self, Write};
use std::os::raw::c_int;

// this is the type required by Nauty vertices
pub type Node = i32;
// M must be a usize to set array bounds until generic_const_exprs stabilizes
// we'll have to downcast to Node in places instead of upcasting to usize
pub type ConstT = usize;

// for counting data losses
pub type Score = u32;

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

#[derive(Clone, Copy)]
pub struct Nauty<const M: ConstT, const DEBUG: bool = false> {
    g: [graph; M],
    ptn: [c_int; M],
    orbits: [c_int; M],
    scores: [Score; M],
    options: optionblk,
    stats: statsblk,
}

impl<const M: ConstT> Default for Nauty<M> {
    fn default() -> Self {
        Self::new()
    }
}

impl<const M: ConstT, const DEBUG: bool> Nauty<M, DEBUG> {
    // SETWORDSNEEDED(m) is nonconst so do it ourselves
    const WORDS: usize = M / WORDSIZE as usize + if M % WORDSIZE as usize == 0 { 0 } else { 1 };

    pub fn new() -> Nauty<M, DEBUG> {
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
            ptn: [0 as c_int; M],
            orbits: [0 as c_int; M],
            scores: [0 as Score; M],
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

    pub fn compute(&mut self, lab: &mut [Node; M]) {
        unsafe {
            densenauty(
                self.g.as_mut_ptr(), // read only
                lab.as_mut_ptr(),
                self.ptn.as_mut_ptr(),
                self.orbits.as_mut_ptr(), // write only
                &mut self.options,        // read only
                &mut self.stats,          // write only
                Self::WORDS as c_int,
                M as c_int,
                std::ptr::null_mut(),
            );
        }
    }

    pub fn print(&self, lab: &[Node; M]) {
        if true {
            print!("[");
            for &l in lab.iter() {
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

    pub fn print_scores(&self) {
        for i in 0..M {
            println!("{} {}", i + 1, self.scores[i]);
        }
    }

    pub fn recurse(&mut self) {
        let mut lab = [0 as Node; M];

        self.compute(&mut lab);

        self.scores[0] = 1;

        self.options.defaultptn = 0;

        self.ptn.fill(1);
        self.ptn[0] = 0;
        *self.ptn.last_mut().unwrap() = 0;

        self._recurse(0, &mut lab);
    }

    fn _recurse(&mut self, prev: usize, prev_lab: &mut [Node; M]) {
        let (unique_orbits, unique_counts) = self.count_orbits();

        if DEBUG {
            println!("=== Recurse {prev} ===");
        }

        let mut dead_nodes = prev_lab[..prev].iter().copied().collect::<ArrayVec<_, M>>();
        dead_nodes.sort();
        dead_nodes.reverse();

        let cap = if !dead_nodes.is_empty() {
            let mut i = 0;
            while unique_orbits[i] > dead_nodes[0] {
                i += 1;
            }
            i
        } else {
            unique_orbits.len()
        };

        for i in 0..cap {
            let mut found = false;

            for j in prev..M {
                if prev_lab[j] == unique_orbits[i] {
                    prev_lab[j] = prev_lab[prev];
                    prev_lab[prev] = unique_orbits[i];
                    found = true;
                    break;
                }
            }

            if !found {
                continue;
            }

            let mut lab = *prev_lab;

            self.compute(&mut lab);

            self.scores[prev + 1] += unique_counts[i];

            if prev + 1 < M - MtoN::<M>() as usize {
                self.ptn[prev] = 1;
                self.ptn[prev + 1] = 0;

                self._recurse(prev + 1, &mut lab);

                // if prev + 1 != M -1
                self.ptn[prev + 1] = 1;
                self.ptn[prev] = 0;
            } else {
                if DEBUG {
                    println!("=== No recurse {prev} ===");
                }
            }
        }
    }

    fn count_orbits(&mut self) -> (ArrayVec<Node, M>, ArrayVec<Score, M>) {
        self.orbits.sort();
        self.orbits.reverse();

        let mut uniq = ArrayVec::<Node, M>::new();
        let mut counts = ArrayVec::<Score, M>::new();

        let mut count = 0 as Score;
        let mut key = self.orbits[0];
        uniq.push(self.orbits[0]);

        for o in self.orbits {
            if o != key {
                uniq.push(o);
                counts.push(count);

                key = o;
                count = 1;
            } else {
                count += 1;
            }
        }

        counts.push(count);

        (uniq, counts)
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
