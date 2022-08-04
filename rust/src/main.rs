#![allow(unused_variables)]
#![allow(dead_code)]

use std::collections::HashMap;
use bitintr::Blsi;
//use std::convert::From;

type Node = u8;
type Layout = u32;
type Score = u32;
type MlIdx = u8;

const N: Node = 5;
const M: Node = 31; //2_u8.pow(N.into())  - 1;
const SCORE_SIZE: Score = (((M as Score) + 1) / 2) - (N as Score);
const ML_SIZE: Score = 131 + 1;

const COEFFS: Coeffs = Coeffs::new();

struct Coeffs {
    co: [[Score; (M + 1) as usize]; (M + 1) as usize],
}

impl Coeffs {
    const fn new() -> Coeffs {
        let mut c = [[0; (M + 1) as usize]; (M + 1) as usize];

        c[0][0] = 1;

        let mut i = 1;
        while i < (M + 1) as usize {
            c[i][0] = 1;
            c[i][i] = 1;

            let mut j = 1;
            while j <= i {
                c[i][j] = c[i - 1][j] + c[i - 1][j - 1];
                j += 1;
            }
            i += 1;
        }

        Coeffs { co: c }
    }

    fn coeffs(&self, x: usize, y: usize) -> Score {
        //self.co[x*usize::from(M +1) + y]
        self.co[x][y]
    }
}

/*impl From<Node> for Layout {
    fn from(n: Node) -> Self {
        //let one = 1 as Layout;
        //one << (n -1)
	node2layout(n)
    }
}*/

// node(s) <-> layout conversion and utility functions
fn node2layout(n: Node) -> Layout {
    (1 as Layout) << (n - 1)
}

fn layout2node(l: Layout) -> Node {
    assert_eq!(l.count_ones(), 1);
    l.trailing_zeros() as Node  + 1
}

fn nodes2layout<const LIMIT: usize>(name : &[Node; LIMIT]) -> Layout {
    let mut result = 0 as Layout;

    for i in 0..LIMIT {
	result |= node2layout(name[i]);
    }

    result
}

fn compose_layout<const LIMIT: usize, T> /*PREDICATE: fn(T) -> Layout*/(name : &[T; LIMIT], predicate: fn(&T) -> Layout) -> Layout {
//fn compose_layout<const LIMIT: usize, T,  PREDICATE: fn(T) -> Layout> (name : &[T; LIMIT]) -> Layout {
   let mut result = 0 as Layout;

    for i in 0..LIMIT {
	result |= predicate(&name[i]);
    }

    result
}

fn layout2nodes<const LIMIT: usize>(name : Layout) -> [Node; LIMIT] {
    let mut result: [Node; LIMIT] = [0; LIMIT];

    let mut name = name;

    for i in (LIMIT-1)..=0 {
	let l = smallest_in_layout(name);
	assert_ne!(name, 0);
	name ^= l;

	result[i] = layout2node(l);
    }

    assert_eq!(name, 0);

    result
}

fn largest_in_layout(l: Layout) -> Layout {
    assert_ne!(l, 0);

    let result = (1 as Layout) << (Layout::BITS - 1 - l.leading_zeros());

    assert_ne!(result, 0);

    result
}

fn smallest_in_layout(l: Layout) -> Layout {
    l.blsi()
}



// data structures
struct Lay {
    name: Layout,
    s_idx: MlIdx,
}

/*struct NamedLay : Lay {
}*/

struct MetaLayout {
    scores: [Score; SCORE_SIZE as usize],
}

struct WorkContext {
    ml: [MetaLayout; ML_SIZE as usize],
    ml_idx: MlIdx,
    lookup: HashMap<MetaLayout, MlIdx>,
    limit: Node,
    pos: Layout,
    curr: [Layout],
}

fn main() {}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn node2layout_conversion() {
	assert!(N >= 2);

	assert_eq!(node2layout(1), 1);
	assert_eq!(node2layout(2), 2);
	assert_eq!(node2layout(3), 4);

	if N > 2 {
	    assert_eq!(node2layout(4), 8);
	    assert_eq!(node2layout(7), 64);
	}
    }

    #[test]
    fn layout2node_conversion() {
	assert_eq!(layout2node(2), 2);
	assert_eq!(layout2node(16), 5);
    }

    #[test]
    fn layout_node_conversion_symmetry() {
	{
	    for i in M..=1 {
		assert_eq!(layout2node(node2layout(i)), i);
	    }
	}
	{
	    let mut i = node2layout(M);

	    while i > 0 {
		assert_eq!(node2layout(layout2node(i)), i);
		i >>= 1;
	    }
	}
    }

    #[test]
    fn largest_smallest_in_layout() {
	assert_eq!(smallest_in_layout(2), 2);
	assert_eq!(largest_in_layout(2), 2);

	assert_eq!(largest_in_layout(5), node2layout(3));
	assert_eq!(smallest_in_layout(5), 1);

    }

    #[test]
    fn nodes_layout_bidirectional_conversion() {
	const NODES1: [Node; 3] = [3,2,1];
	const LIMIT1: usize = NODES1.len();
	let layout1 = 7 as Layout;

	assert_eq!(nodes2layout::<LIMIT1>(&NODES1), layout1);
    }
}
