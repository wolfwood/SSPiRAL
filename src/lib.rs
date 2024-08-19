use nauty_Traces_sys::*;
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
    pub lab: [c_int; M],
    pub ptn: [c_int; M],
    pub orbits: [c_int; M],
    pub options: optionblk,
    pub stats: statsblk,
}

impl<const M: ConstT> Nauty<M> {
    pub fn new() -> Nauty<M> {
        let o = SETWORDSNEEDED(M);

        unsafe {
            nauty_check(
                WORDSIZE as c_int,
                o as c_int,
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
        let o = SETWORDSNEEDED(M);

        let mut g = empty_graph(o, M);

        // this will be a const when we have generic_const_exprs
        #[allow(non_snake_case)]
        let N = MtoN::<M>();

        for i in 1..=N {
            let v = 1 << (i - 1);
            for u in (v + 1)..=M as Node {
                if (v) & (u) == (v) {
                    ADDONEEDGE(&mut g, invert::<M>(v) as usize, invert::<M>(u) as usize, o);
                    //println!("{} -> {}", v, u);
                }
            }
        }

        g
    }

    pub fn compute(&mut self) {
        let o = SETWORDSNEEDED(M);

        unsafe {
            densenauty(
                self.g.as_mut_ptr(),
                self.lab.as_mut_ptr(),
                self.ptn.as_mut_ptr(),
                self.orbits.as_mut_ptr(),
                &mut self.options,
                &mut self.stats,
                o as c_int,
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
}
