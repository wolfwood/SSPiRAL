#![allow(unused_variables)]
#![allow(dead_code)]

use bitintr::Blsi;
use std::collections::HashMap;

type NodeData = u8;
#[derive(Clone, Copy, PartialEq, Debug, Default)]
struct Node(NodeData);
type LayoutData = u32;
#[derive(Clone, Copy, PartialEq, PartialOrd, Debug, Default)]
struct Layout(LayoutData);
type Score = u32;
type MlIdx = u8;

const N: NodeData = 5;
const M: NodeData = 31; //2_u8.pow(N.into())  - 1;
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

impl From<Node> for Layout {
    fn from(n: Node) -> Self {
        //let one = 1 as Layout;
        //one << (n -1)
        node2layout(n)
    }
}

impl From<Layout> for Node {
    fn from(l: Layout) -> Self {
        layout2node(l)
    }
}

// node(s) <-> layout conversion and utility functions
fn node2layout(n: Node) -> Layout {
    let Node(n) = n;
    Layout((1 as LayoutData) << (n - 1))
}

fn layout2node(l: Layout) -> Node {
    let Layout(l) = l;
    assert_eq!(l.count_ones(), 1);
    Node(l.trailing_zeros() as NodeData + 1)
}

fn nodes2layout<const LIMIT: usize>(name: &[Node; LIMIT]) -> Layout {
    compose_layout(name)
}

fn compose_layout<const LIMIT: usize, T: Copy>(name: &[T; LIMIT]) -> Layout
where
    Layout: From<T>,
{
    let mut result = 0 as LayoutData;

    for i in 0..LIMIT {
        result |= Layout::from(name[i]).0;
    }

    Layout(result)
}

fn layout2nodes<const LIMIT: usize>(name: Layout) -> [Node; LIMIT] {
    decompose_layout(name)
}

fn decompose_layout<const LIMIT: usize, T: Default + Copy>(name: Layout) -> [T; LIMIT]
where
    T: From<Layout>,
{
    let mut result: [T; LIMIT] = [Default::default(); LIMIT];

    let Layout(mut name) = name;

    for i in (0..LIMIT).rev() {
        let Layout(l) = smallest_in_layout(Layout(name));
        assert_ne!(name, 0);
        assert_ne!(name & l, 0);
        name ^= l;

        result[i] = T::from(Layout(l));
    }

    assert_eq!(name, 0);

    result
}

fn largest_in_layout(l: Layout) -> Layout {
    assert_ne!(l, Layout(0));
    let Layout(l) = l;

    let result = Layout((1 as LayoutData) << (LayoutData::BITS - 1 - l.leading_zeros()));

    assert_ne!(result, Layout(0));

    result
}

fn smallest_in_layout(l: Layout) -> Layout {
    let Layout(l) = l;
    Layout(l.blsi())
}


//liveness

fn check_if_alive(name: Layout) -> bool {
    fn recurse_check(name: Layout, n: Node, i: Node, depth: NodeData) -> bool{
	let Node(i) = i;

	for i in (1..=i).rev(){
	    if Layout::from(Node(i)).0 & name.0  != 0 {
		let temp = Node(i ^ n.0);

		if temp == Node(0) || (depth > 0 && recurse_check(name, temp, Node(i-1), depth - 1)){
		    return true;
		}
	    }
	}

	false
    }

    for n in (1..=((1 as NodeData) << (N -1))).rev() {
	let n = Node(n);
	let l = Layout::from(n);

	if l.0 & name.0 != 0 {
	    if !recurse_check(name, n, Node(M), M) {
		return false;
	    }
	}

    }

    true
}

// data structures
struct LayoutStats {
    //name: Layout,
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
    curr: [LayoutStats],
}

fn main() {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn node2layout_conversion() {
        assert!(N >= 2);

        assert_eq!(node2layout(Node(1)), Layout(1));
        assert_eq!(node2layout(Node(2)), Layout(2));
        assert_eq!(node2layout(Node(3)), Layout(4));

        if N > 2 {
            assert_eq!(node2layout(Node(4)), Layout(8));
            assert_eq!(node2layout(Node(7)), Layout(64));
        }
    }

    #[test]
    fn layout2node_conversion() {
        assert_eq!(layout2node(Layout(2)), Node(2));
        assert_eq!(layout2node(Layout(16)), Node(5));
    }

    #[test]
    fn layout_node_conversion_symmetry() {
        {
            for i in M..=1 {
                assert_eq!(layout2node(node2layout(Node(i))), Node(i));
            }
        }
        {
            let mut i = node2layout(Node(M)).0;

            while i > 0 {
                assert_eq!(node2layout(layout2node(Layout(i))), Layout(i));
                i >>= 1;
            }
        }
    }

    #[test]
    fn largest_smallest_in_layout() {
        assert_eq!(smallest_in_layout(Layout(2)), Layout(2));
        assert_eq!(largest_in_layout(Layout(2)), Layout(2));

        assert_eq!(largest_in_layout(Layout(5)), node2layout(Node(3)));
        assert_eq!(smallest_in_layout(Layout(5)), node2layout(Node(1)));
    }

    #[test]
    fn nodes_layout_bidirectional_conversion() {
        const NODES1: [Node; 3] = [Node(3), Node(2), Node(1)];
        const LIMIT1: usize = NODES1.len();
        let layout1 = Layout(7);

        assert_eq!(nodes2layout(&NODES1), layout1);
        assert_eq!(layout2nodes(layout1), NODES1);
    }
}
