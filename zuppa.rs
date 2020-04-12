
#![allow(unused_variables)]
#![allow(dead_code)]

use std::collections::HashMap;

type Node = u8;
type Layout = u32;
type Score = u32;
type MlIdx = u8;

const N : Node = 5;
const M : Node = 31; //2_u8.pow(N.into())  - 1;
const SCORE_SIZE : Score = (((M as Score) + 1) /2) - (N as Score);
const ML_SIZE : Score = 131 + 1;

struct Coeffs {
    co : [[Score; 32 ]; 32], // XXX this shouldn't need CTFE to use (M+1)
}

impl Coeffs {
    fn new() -> Coeffs {
        let mut c = [[0; 32 ]; 32];

        c[0][0] = 1;

        for i in 1..(usize::from(M)+1) {
            c[i][0] = 1;
            c[i][i] = 1;

            for j in 1..i {
                c[i][j] = c[i-1][j] + c[i-1][j-1];
            }
        }

        Coeffs{co:c}
    }

    fn coeffs(&self, x: usize, y: usize) -> Score {
        //self.co[x*usize::from(M +1) + y]
        self.co[x][y]
    }
}

/*impl From<Node> for Layout {
    fn from(n: Node) -> Self {
        let one = 1 as Layout;
        one << (n -1)
    }
}*/

fn node2layout(n: Node) -> Layout {
    (1 as Layout) << (n - 1)
}


struct Lay {
    name: Layout,
    s_idx: MlIdx,
}

/*struct NamedLay : Lay {
}*/

struct MetaLayout {
    scores: [Score; SCORE_SIZE as usize]
}

struct WorkContext {
    ml: [MetaLayout; ML_SIZE as usize],
    ml_idx: MlIdx,
    lookup : HashMap<MetaLayout, MlIdx>,
    limit: Node,
    pos: Layout,
    curr: [Layout],
}

fn main() {

}
