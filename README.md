# SSPiRAL (Survivable Storage using Parity in Redundant Array Layouts) Simulator Collection

This is a collection of programs in various languages that simulate node failures for a data store using an erasure code I helped develop, called SSPiRAL. The primary output is the number of unique scenarios for each number of node failures, and the number of those scenarios that "survive", that is, do not lead to data loss.

Secondary outputs may include dot diagrams of Markov chains, which describe the behavior of the system at a more abstract level (i.e. ignoring node identies). Equally, one can generate Kolmogorov equations derived from these Markov chains, which can be solved to calculate the Mean Time To Data Loss (MTTDL), which is useful for comparing erasure codes, particularly non-optimal ones such as SSPiRAL.

The original purpose of this code was the exploration of behavior of the SSPiRAL erasure code. However, the challenge of writing efficient code to enumerate and categorize the failure scenarios has also been useful for my growth as a programmer, and has provided an opportunity to explore both optimization strategies at various levels and the affordances of various programming languages.

The collection of the most recent simulators resembles [The Compute Language Benchmark Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/index.html), as it pits similar algorithms against each other in C, D, Zig (Rust is in progress). These simulators currently do not produce the secondary outputs as these are of negligible computational complexity. The correct primary output (for several values of N) is sufficient to verify correctness of the implementation.


## A Brief Vocabulary

 * Node: a chunk of storage that fails as a unit. Depending on the system this could be as small as a data sector, or even a single bit, but I tend to think of a node as disk in a RAID-like storage array or a single storage server in a distributed system.
    * Data node: a node that represents information in the originally stored form. Some erasure codes do not have data nodes which complicates reads.
	* Parity node: a node where the information from multiple data nodes has been combined in a way that can be used to restore a failed data node, when recombined with other parity and/or data nodes. SSPiRAL creates parity nodes by XORing various combinations of data nodes.
 * **N**: the pre-established number of data nodes. This is the main parameter of a SSPiRAL encoding. (I'd consider it a implementation detail, but RAID arrays typically stripe data across data nodes in fixed size chunks, rather than filling the first node before moving on to the second. This is akin to having an array of parallel, independent encodings. Regardless, nodes can be arbitrarily large so **N** is not directly related to the size of the data stored.)
 * **M** = 2<sup>N</sup> - 1: the total number of unique nodes that may be used in a SSPiRAL encoding.
 * Node naming: rather than naming data nodes 1, 2, 3 I name them with powers of two 2<sup>0</sup> = 1, 2<sup>1</sup> = 2, 2<sup>2</sup> = 4... This means that data node names always have a single bit set, allowing parity nodes to be named as bitmap of the data nodes they combine, which is equivalent to the XOR, e.g. 1 ⊕ 2 ⊕ 8 = 11. 11 is the parity created by XORing data nodes 1, 2 and 8.
 * Layout: a collection of up to M nodes, representing the system state at a given point in time. The system transitions between layouts when nodes fail or are repaired.
 * Layout naming: layouts can also be named and represented as a bitmap of the constituent node names. The layout 13 contains nodes 1, encoded as 2<sup>1-1</sup> = 1; 3 encoded as 2<sup>3-1</sup> = 4; and 4 encoded as 2<sup>4-1</sup> = 8. While this compact form is useful for computation, for human readability I will use set notation instead: {1, 3, 4} or {1, 1⊕2, 4}.
 * Child/Parent Layout: there is a relationship between layouts that differ by the addition or removal of a single node. {1, 1⊕2}, {1, 4}, and {1⊕2, 4} are all children of {1, 1⊕2, 4}, as the are the possible results of a node failure. {1, 1⊕2, 4, 2⊕4} is a parent of {1, 1⊕2, 4}, produced by a node repair or addition. The relationship is transitive, so {1} is a decendant of {1, 1⊕2, 4, 2⊕4}.
 * Dead/Alive Layout: a 'dead' layout has suffered unrecoverable data loss; a layout that is 'alive' can recover all the original data. While we may need to know **N** to be sure in some cases, we can infer for layout {1, 4} that **N** is at least 3, and therefore this layout is dead because it only has 2 nodes.
 * Score: an expession of the reliability of a layout. Conceptually it is an array indexed by the number of node losses, and storing the number of live decendant layouts. In the simulators several optimizations are applied to reduce array length ([see Invariants](#invariants)), dead decendants are counted instead which allows a int type to be used, and the arrays are indexed in reverse order to facilitate construction of parents from children.
 * Meta Layout: if N = 4, the layouts {1, 2, 1⊕2, 1⊕4} and {1, 2, 1⊕2, 2⊕4} are both 'alive' because they can recover data node 4 using the parity, in once case XORing it with node 1, in the other node 2. If we chose to relabel data nodes 1 and 2 as 2 and 1 for one layout, it would become identical to the other. These layouts are effectively the same, in terms of both their scores and the scores of there potential child and parent layouts. A meta layout is collection of layouts that are functionally equivalent in terms of reliability, and whose parents and children all fall within the same sets of metalayouts as well. Not all the layouts in a meta layout are simple relabelings of each other, either. Studying this hidden structure is what drove me to create the subsequent versions of my initial simulator.
 * Degree: the number of data nodes combined to form a parity node. AKA cardinality, XORder.

## Why is SSPiRAL interesting as an Erasure Code?
An optimal erasure code can recover the original N data nodes using any N nodes, whether they are parity or data. This is also referred to as an MDS (maximum distance separable) code. With SSPiRAL, as the total number of nodes falls below M/2, the risk of data loss increases. Of all the possible layouts containing N nodes, a bit less than half can recover the original data.

Nonetheless, SSPiRAL has several interesting properties. SSPiRAL has much lower computational overheads than optimal erasure codes, as it is based solely on XOR. In addition, SSPiRAL parity generation can be distibuted. Many parities are generated from only a few data blocks and higher cardinality parities can be generated from a small number of other parities. In contrast, MDS code require N blocks to generate any particular parity. Finally, writes to data nodes can be distributed as deltas, the XOR of the old and new data, allowing parities involving a given data node to be updated without involving additional nodes. MDS codes would need to recalculate parities from scratch.

There is a well-known class of non-optimal erasure codes known as fountain codes that also employs XOR. These codes limit the degree of the parity nodes to ease implementation in fixed function hardware and use probabilistic selection of parity nodes to provide provable bounds. My work on SSPiRAL stands apart because I've been attempting to identify the rules that govern which parity nodes are optimal to use. SSPiRAL is also geared toward distributed storage systems rather than communication or read-only media. For N <= 5, I've demonstrated that with active reconfiguation of at most one parity in the wake of a failure, SSPiRAL can behave like an optimal code.

It's this idea of active reconfiguration that is both most unique about SSPiRAL and that directly drives this simulation work: the system needs to know if it's in a less reliable configuration, and needs to identify a more reliable peer layout to transition to.


## data structures

## historical approaches

## current code

## Optimization

### theoretic

### algorithmic

### language

### compiler


## Invariants
  * all layouts with fewer than **N** nodes are dead
  * all layouts with > (**M** + 1) / 2 nodes are alive
  * any layout that can recover a given data node can do so by XORing at most **N** nodes


## Notes on Organization
This code contains many redundant implementations of the same functions, which I would have removed from a production codebase. This is because comparing different strategies is a major goal of this work. Similarly the archive is maintained, rather than being relegated to the git history, to serve as reference.
