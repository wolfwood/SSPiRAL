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
    g: Vec<graph>,
    lab: Vec<c_int>,
    ptn: Vec<c_int>,
    orbits: Vec<c_int>,
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
            g: Nauty::full_graph(),
            lab: vec![0 as c_int; M as usize],
            ptn: vec![0 as c_int; M as usize],
            orbits: vec![0 as c_int; M as usize],
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
                M as c_int,
                std::ptr::null_mut(),
            );
        }
    }

    fn print(&self) {
        if true {
            print!("[");
            for &l in self.lab.iter() {
                print!("{} ", revert(l.try_into().unwrap()));
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

/*
fn notmain() -> Result<(), Box<dyn std::error::Error>> {
    let mut options = optionblk::default();
    //options.writeautoms = TRUE;
    let mut stats = statsblk::default();

    //let n = 3;
    //let m = 2_usize.checked_pow(n as u32).unwrap() - 1;
    let o = SETWORDSNEEDED(N as usize);

    unsafe {
        nauty_check(
            WORDSIZE as c_int,
            o as c_int,
            N as c_int,
            NAUTYVERSIONID as c_int,
        );
    }

    let mut lab = vec![0; M as usize];
    let mut ptn = vec![0 as c_int; M as usize];
    let mut orbits = vec![0; M as usize];

    let mut g = empty_graph(o, M);
    for t in 1..=N {
        let v = 1 << (t - 1);
        for u in (v + 1)..=M {
            if (v) & (u) == (v) {
                ADDONEEDGE(&mut g, invert(v), invert(u), o);
                //println!("{} -> {}", v, u);
            }
        }
    }

    println!("Generators for Aut(C[{}]):", N);

    unsafe {
        densenauty(
            g.as_mut_ptr(),
            lab.as_mut_ptr(),
            ptn.as_mut_ptr(),
            orbits.as_mut_ptr(),
            &mut options,
            &mut stats,
            o as c_int,
            M as c_int,
            std::ptr::null_mut(),
        );
    }

    if true {
        print!("[");
        for &l in lab.iter() {
            print!("{} ", revert(l.try_into().unwrap()));
        }
        println!("]");

        print!("[");
        for p in ptn.iter() {
            print!("{} ", p);
        }
        println!("]");
    }

    print!("[");
    for &orbit in orbits.iter() {
        print!("{} ", revert(orbit.try_into().unwrap()));
    }
    println!("]");

    println!("num orbits = {}", stats.numorbits);
    print!("order = ");
    io::stdout().flush().unwrap();
    unsafe {
        writegroupsize(stderr, stats.grpsize1, stats.grpsize2);
    }
    println!();

    options.defaultptn = 0;

    let mut x = 0;
    for i in orbits.clone() {
        if i >= x {
            x = i + 1;

            ptn.fill(1);
            ptn[0] = 0;
            *ptn.last_mut().unwrap() = 0;

            lab[0] = i;

            let bet = revert(i as usize);
            for j in 1..=lab.len() {
                let k = invert(j);

                if j < bet {
                    lab[j] = k as i32;
                } else if j > bet {
                    lab[j - 1] = k as i32;
                }
            }

            unsafe {
                densenauty(
                    g.as_mut_ptr(),
                    lab.as_mut_ptr(),
                    ptn.as_mut_ptr(),
                    orbits.as_mut_ptr(),
                    &mut options,
                    &mut stats,
                    o as c_int,
                    M as c_int,
                    std::ptr::null_mut(),
                );
            }

            if true {
                print!("[");
                for &l in lab.iter() {
                    print!("{} ", revert(l.try_into().unwrap()));
                }
                println!("]");

                print!("[");
                for p in ptn.iter() {
                    print!("{} ", p);
                }
                println!("]");
            }

            print!("[");
            for orbit in orbits.iter() {
                print!("{} ", orbit);
            }
            println!("]");

            println!("num orbits = {}", stats.numorbits);
            print!("order = ");
            io::stdout().flush().unwrap();
            unsafe {
                writegroupsize(stderr, stats.grpsize1, stats.grpsize2);
            }
            println!();
        }
    }

    Ok(())
}
 */

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
