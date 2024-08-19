use nauty_Traces_sys::*;
use std::io::{self, Write};
use std::os::raw::c_int;

type Node = i32;
type ConstT = i32;

const fn n2m(n: ConstT) -> ConstT {
    2_i32.pow(n as u32) - 1
}

const N: ConstT = 3;
const M: ConstT = n2m(N);

pub fn invert(node: Node) -> Node {
    !node & M
    //node -1
}

pub fn revert(node: Node) -> Node {
    !node & M
    //node + 1
}

pub struct Nauty {
    g: [graph; M as usize],
    pub lab: [c_int; M as usize],
    pub ptn: [c_int; M as usize],
    pub orbits: [c_int; M as usize],
    pub options: optionblk,
    pub stats: statsblk,
}

impl Nauty {
    pub fn new() -> Nauty {
        let o = SETWORDSNEEDED(M as usize);

        unsafe {
            nauty_check(
                WORDSIZE as c_int,
                o as c_int,
                M as c_int,
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
        let o = SETWORDSNEEDED(M as usize);

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

    pub fn compute(&mut self) {
        let o = SETWORDSNEEDED(M as usize);
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

    pub fn print(&self) {
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
