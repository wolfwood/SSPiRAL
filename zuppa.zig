const std = @import("std");
const assert = @import("std").debug.assert;

// constants
const N: u3 = 5;
const M: Node = (1 << N) - 1;
const ScoreSize = ((@as(u32, M) + 1) / 2) - N;
const MlSize = 131 + 1;

// types
const Node = bitsToType(N);
const Layout = bitsToType(M);
const Score = u32;
const ScoreIdx = u8;
const Combinadic = u32;

const totalLayoutCount = (@as(usize, 1) << M);

// encoding helpers

inline fn node2layout(node: Node) Layout {
    assert(node > 0);
    return @as(Layout, 1) << (node - 1);
}

inline fn layout2node(l: Layout) Node {
    assert(@popCount(Layout, l) == 1);
    return @ctz(Layout, l) + 1;
}

const bitCount = @import("std").meta.bitCount;

inline fn largestInLayout(l: Layout) Layout {
    assert(l != 0);
    var result = @as(Layout, 1) << (bitCount(Layout) - 1 - @clz(Layout, l));

    assert(result != 0);

    return result;
}

inline fn smallestInLayout(l: Layout) Layout {
    assert(l != 0);
    var result = @as(Layout, 1) << (@ctz(Layout, l));

    assert(result != 0);

    return result;
}

inline fn decomposeLayout(comptime limit: Node, name: Layout) [limit]Node {
    var result: [limit]Node = undefined;

    var i: u32 = 0;
    var l = largestInLayout(name);
    var stop = smallestInLayout(name);

    while (l >= stop) : (l >>= 1) {
        if (name & l != 0) {
            result[i] = layout2node(l);
            i += 1;
        }
    }

    return result;
}

inline fn composeLayout(comptime limit: Node, name: [limit]Node) Layout {
    var result: Layout = 0;

    var i: Node = 0;
    while (i < limit) : (i += 1) {
        result |= node2layout(name[i]);
    }

    return result;
}

// liveness checking

inline fn recurseCheck(name: Layout, n: Node, _i: Node, depth: Node) bool {
    var i = _i;

    while (i > 0) : (i -= 1) {
        if (node2layout(i) & name != 0) {
            var temp = i ^ n;
            if (temp == 0 or (depth > 0 and recurseCheck(name, temp, i - 1, depth - 1))) {
                return true;
            }
        }
    }
    return false;
}

inline fn recurseCheckLayout(name: Layout, n: Node, _i: Layout, depth: Node) bool {
    var i = _i;

    while (i > 0) : (i >>= 1) {
        if (i & name != 0) {
            var temp = layout2node(i) ^ n;
            if (temp == 0 or (depth > 0 and recurseCheckLayout(name, temp, i >> 1, depth - 1))) {
                return true;
            }
        }
    }
    return false;
}

fn checkIfAlive(name: Layout) bool {
    var n: Node = @as(Node, 1) << (N - 1);
    while (n != 0) : (n >>= 1) {
        var l = node2layout(n);
        if ((l & name) == 0) {
            if (!recurseCheck(name, n, M, M)) { //@popCount(name)
                //if (!recurseCheckLayout(name, n, node2layout(M), M)) {
                return false;
            }
        }
    }
    return true;
}

fn deadnessCheck(comptime limit: Node, name: Layout) bool {
    const nodes = decomposeLayout(limit, name);

    var j = 0;

    var n: Node = @as(Node, 1) << (N - 1);
    while (n != 0) : (n >>= 1) {
        while ((j < limit - 1) & &((nodes[j]) > n)) : (j -= 1) {}
    }
}

// structs

fn LayoutStats(comptime verify: bool) type {
    return struct {
        name: if (verify) Layout else void,
        score: ScoreIdx,
    };
}

// XXX parameterize by limit
fn MetaLayout(comptime limit: Node) type {
    return struct {
        scores: [std.math.min(ScoreSize, limit - (N - 1))]Score,
    };
}

fn NamedWorkContext(comptime limit: Node) type {
    if (limit == N) {
        return struct {
            layouts: []LayoutStats(false),
            unique_scores: [2]MetaLayout(limit),
        };
    } else {
        return struct {
            layouts: []LayoutStats(false),
            unique_scores: [MlSize]MetaLayout(limit) = undefined,
            prev_scores: []MetaLayout(limit - 1),

            curr_score: ScoreIdx = 0,

            // XXX some dynamic dictionary from MetaLayout -> ScoreIdx
            unique: std.AutoHashMap(MetaLayout(limit), ScoreIdx),
        };
    }
}

fn MakeWorkContext(comptime limit: Node, alloc: Allocator, prev_ctx: *NamedWorkContext(limit - 1)) NamedWorkContext(limit) {
    return _makeWorkContext(limit, alloc, prev_ctx.layouts, prev_ctx.unique_scores[0..]);
}

fn _makeWorkContext(
    comptime limit: Node,
    alloc: Allocator,
    layouts: []LayoutStats(false),
    prev_scores: []MetaLayout(limit - 1),
) NamedWorkContext(limit) {
    assert(limit > N);

    var result = NamedWorkContext(limit){
        .layouts = layouts,
        .prev_scores = prev_scores,
        .unique = std.AutoHashMap(MetaLayout(limit), ScoreIdx).init(alloc),
    };

    return result;
}

fn getScoreIndex(comptime limit: Node, args: *NamedWorkContext(limit)) !ScoreIdx {
    var result = try args.unique.getOrPut(args.unique_scores[args.curr_score]);

    if (!result.found_existing) {
        result.value_ptr.* = args.curr_score;
        args.curr_score += 1;
    }

    return result.value_ptr.*;
}

// passes

fn namedFirstPass(name: Layout, args: *NamedWorkContext(N)) void {
    args.layouts[name] = LayoutStats(false){ .score = @boolToInt(!checkIfAlive(name)), .name = undefined };
    //if (checkIfAlive(name)) {
    //    args.layouts[name] = LayoutStats(false){ .score = 0 };
    //} else {
    // XXX assume zero initialized?
    //    args.layouts[name] = LayoutStats(false){ .score = 1 };
    //}
    //if (@TypeOf(args.layouts[name].name) != void) {
    //    args.layouts[name].name = name;
    //}
}

fn namedIntermediatePass(comptime limit: Node, name: Layout, args: *NamedWorkContext(limit)) !void {
    sumChildLayoutScores(limit, name, args);

    if (limit <= M / 2) {
        if (args.unique_scores[args.curr_score].scores[limit - N - 1] == limit) {
            args.unique_scores[args.curr_score].scores[limit - N] = @boolToInt(!checkIfAlive(name));
        } else {
            args.unique_scores[args.curr_score].scores[limit - N] = 0;
        }
    }

    args.layouts[name].score = try getScoreIndex(limit, args);
}

fn namedTerminalPass(comptime limit: Node, name: Layout, args: *NamedWorkContext(limit)) !void {
    sumChildLayoutScores(limit, name, args);

    args.layouts[name].score = try getScoreIndex(limit, args);
}

// pass utility functions

inline fn howLongIsScores(comptime limit: Node) Node {
    const oneWay = if (limit <= M / 2) limit - (N - 1) else ScoreSize;
    const theOtherWay = std.math.min(ScoreSize, limit - (N - 1));

    assert(oneWay == theOtherWay);

    return theOtherWay;
}

fn sumChildLayoutScores(comptime limit: Node, name: Layout, args: *NamedWorkContext(limit)) void {
    // we skip the last entry if we are in the intermediate phase because it gets filled in from liveness check
    const length = if (limit - 1 <= M / 2) limit - N else ScoreSize;
    var l = largestInLayout(name); //node2layout(M);

    var i: u32 = 0;
    while (i < length) : (i += 1) {
        args.unique_scores[args.curr_score].scores[i] = args.prev_scores[args.layouts[name ^ l].score].scores[i];
    }
    l >>= 1;

    while (l >= smallestInLayout(name)) : (l >>= 1) {
        if (name & l == l) {
            i = 0;
            while (i < length) : (i += 1) {
                args.unique_scores[args.curr_score].scores[i] += args.prev_scores[args.layouts[name ^ l].score].scores[i];
            }
        }
    }
}

fn normalize(comptime limit: Node, args: *NamedWorkContext(limit)) void {
    const length = comptime howLongIsScores(limit);
    const intermediate = limit <= M / 2;

    if (!intermediate) {
        assert(length == ScoreSize);
    }

    var k: u32 = 0;

    while (k < args.curr_score) : (k += 1) {
        if (intermediate) {
            var j: u32 = 2;
            while (j <= limit - N) : (j += 1) {
                assert(0 == args.unique_scores[k].scores[(limit - N) - j] % j);
                args.unique_scores[k].scores[(limit - N) - j] /= j;
            }
        } else {
            var j: u32 = 0;
            while (j < ScoreSize) : (j += 1) {
                const adjustment = limit - (M / 2);

                assert(0 == args.unique_scores[k].scores[ScoreSize - j - 1] % (j + adjustment));
                args.unique_scores[k].scores[ScoreSize - j - 1] /= j + adjustment;
            }
        }
    }
}

// iteration

fn NamedIterationWork(comptime argtype: type) type {
    return fn (name: Layout, args: argtype) void;
}

inline fn nriHelper(
    comptime limit: Node,
    //    comptime argtype: type,
    comptime i: Node,
    _l: Layout,
    name: Layout,
    args: anytype,
    comptime work: NamedIterationWork(@TypeOf(args)),
) void {
    var l = _l;

    while (l > if (i == limit) 0 else @as(Layout, 1) << (limit - 1 - i)) : (l >>= 1) {
        //name |= l;
        if (i == limit) {
            work(name | l, args);
        } else {
            nriHelper(limit, i + 1, l >> 1, name | l, args, work);
        }
        //name ^= l;
    }
}

fn namedRecursiveIteration(
    comptime limit: Node,
    //comptime argtype: type,
    args: anytype,
    comptime work: fn (name: Layout, args: @TypeOf(args)) void,
) void {
    nriHelper(limit, 1, node2layout(M), 0, args, work);
}

inline fn nriHelper2(
    comptime limit: Node,
    //    comptime argtype: type,
    comptime i: Node,
    _l: Layout,
    name: Layout,
    args: *NamedWorkContext(limit),
    comptime work: fn (comptime limit: Node, name: Layout, args: @TypeOf(args)) void,
) void {
    var l = _l;

    while (l > if (i == limit) 0 else @as(Layout, 1) << (limit - 1 - i)) : (l >>= 1) {
        //name |= l;
        if (i == limit) {
            work(limit, name | l, args);
        } else {
            nriHelper2(limit, i + 1, l >> 1, name | l, args, work);
        }
        //name ^= l;
    }
}

fn namedRecursiveIteration2(
    comptime limit: Node,
    args: *NamedWorkContext(limit),
    comptime work: fn (comptime limit: Node, name: Layout, args: *NamedWorkContext(limit)) void,
) void {
    nriHelper2(limit, 1, node2layout(M), 0, args, work);
}

inline fn nriHelper3(
    comptime limit: Node,
    //    comptime argtype: type,
    comptime i: Node,
    _l: Layout,
    name: Layout,
    args: *NamedWorkContext(limit),
) !void {
    var l = _l;

    while (l > if (i == limit) 0 else @as(Layout, 1) << (limit - 1 - i)) : (l >>= 1) {
        //name |= l;
        if (i == limit) {
            try namedIntermediatePass(limit, name | l, args);
        } else {
            try nriHelper3(limit, i + 1, l >> 1, name | l, args);
        }
        //name ^= l;
    }
}

fn namedRecursiveIteration3(
    comptime limit: Node,
    args: *NamedWorkContext(limit),
) !void {
    try nriHelper3(limit, 1, node2layout(M), 0, args);
}

// TODO

fn NamedIteration(
    comptime limit: Node,
    comptime argtype: type,
    comptime work: NamedIterationWork(argtype),
    args: *argtype,
) void {
    var i: Node = 0;
    var name: Layout = 0;
    var ells: [limit]Layout = 0;
    ells[0] = node2layout(M);

    while (true) {
        if (ells[i] == 0) {
            ells[i] = ells[i - 1] >> 1;
        }

        name |= ells[i];

        if (limit - 1 == i) {}
    }

    _ = work;
    _ = args;
}

fn CombinadicWorkContext(comptime verify: bool, comptime limit: Node) type {
    const layout_count = Coeffs(M, limit);
    return struct {
        layouts: [layout_count]LayoutStats(verify),
        unique_scores: [MlSize]MetaLayout(limit),

        curr_layout: Combinadic,
        curr_score: ScoreIdx,

        // XXX some dynamic dictionary from MetaLayout -> ScoreIdx
        unique: std.AutoHashMap(MetaLayout, ScoreIdx),
    };
}

//fn CombinadicFirstBlushIterator(comptime verify: bool, comptime limit: Node) type {
//    return struct {
//       work: *WorkContext(verify, limit),
//       Is: Node,
//       name: if (verify) Layout else void,
//   };
//}

// main

//var stats: [totalLayoutCount]LayoutStats(false) = undefined;

const Allocator = std.mem.Allocator;

fn unroll(
    comptime limit: Node,
    //comptime iterator: fn (comptime limit: Node, args: anytype, comptime work: anytype) void,
    alloc: Allocator,
    prev_ctx: *NamedWorkContext(limit - 1),
) !MetaLayout(M) {
    var ctx = MakeWorkContext(limit, alloc, prev_ctx);

    try namedRecursiveIteration3(limit, &ctx);

    normalize(limit, &ctx);

    if (limit < M) {
        return try unroll(limit + 1, alloc, &ctx);
    } else {
        assert(ctx.curr_score == 1);

        return ctx.unique_scores[0];
    }
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();

    //try stdout.print("Hello, {s} {} {}\n", .{ "world", Coeffs[M][M / 2], totalLayoutCount });
    try stdout.print("{} of {}\n", .{ N, M });

    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();

    const allocator = arena.allocator();

    //var stats: [totalLayoutCount]LayoutStats(false) = undefined;
    var stats = try allocator.alloc(LayoutStats(false), totalLayoutCount);

    var ctx0 = NamedWorkContext(N){
        .layouts = stats,
        .unique_scores = [2]MetaLayout(N){
            MetaLayout(N){ .scores = [1]Score{0} },
            MetaLayout(N){ .scores = [1]Score{1} },
        },
    };

    namedRecursiveIteration(N, &ctx0, namedFirstPass);

    const result = try unroll(N + 1, allocator, &ctx0);

    var j: usize = 0;
    while (j < ((@as(usize, M) + 1) / 2)) : (j += 1) {
        const total = Coeffs[M][j];
        try stdout.print("{} {} {}\n", .{ j, total, total });
    }

    j = ScoreSize - 1;

    assert(result.scores[j] == M);

    while (j > 0) : (j -= 1) {
        const total = Coeffs[M][(M / 2) + ScoreSize - j];
        try stdout.print("{} {} {}\n", .{ (M / 2) + ScoreSize - j, total - result.scores[j], total });
    }

    const total = Coeffs[M][(M / 2) + ScoreSize];
    try stdout.print("{} {} {}\n", .{ (M / 2) + ScoreSize, total - result.scores[j], total });

    j = M - N + 1;
    while (j <= M) : (j += 1) {
        try stdout.print("{} {} {}\n", .{ j, 0, Coeffs[M][j] });
    }
}

// lookup tables

fn coeffs() [@as(u32, M) + 1][@as(u32, M) + 1]Combinadic {
    var co: [@as(u32, M) + 1][@as(u32, M) + 1]Combinadic = undefined;
    co[0][0] = 1;

    var i: u32 = 1;
    while (i <= M) : (i += 1) {
        co[i][0] = 1;
        co[i][i] = 1;

        var j: u32 = 1;
        while (j < i) : (j += 1) {
            co[i][j] = co[i - 1][j] + co[i - 1][j - 1];
        }
    }

    return co;
}

const Coeffs = coeffs();
// utility functions

fn bitsToType(comptime bits: u8) type {
    return switch (bits) {
        2 => u2,
        3 => u3,
        4 => u4,
        5 => u5,
        6 => u6,
        7 => u7,
        15 => u15,
        31 => u31,
        63 => u63,
        127 => u127,

        else => void,
    };
}

// testing

const expect = std.testing.expect;
const expectEqual = std.testing.expectEqual;

test "fast bitsToType() conversion" {
    const type1 = bitsToType(7);

    try expectEqual(type1, u7);

    const type2 = bitsToType(127);

    try expectEqual(type2, u127);
}

test "fast bitsToType() failure" {
    const type1 = bitsToType(53);

    try expectEqual(type1, void);
}

test "fast node2layout conversion" {
    try expect(N >= 2);

    try expectEqual(node2layout(1), 1);
    try expectEqual(node2layout(2), 2);
    try expectEqual(node2layout(3), 4);

    if (N > 2) {
        try expectEqual(node2layout(4), 8);
        try expectEqual(node2layout(7), 64);
    }
}

test "fast layout2node conversion" {
    try expectEqual(layout2node(2), 2);

    try expectEqual(layout2node(16), 5);
}

test "fast layout <-> node conversion symmetry" {
    {
        var i = M;
        while (i > 0) : (i -= 1) {
            try expectEqual(layout2node(node2layout(i)), i);
        }
    }
    {
        var i = node2layout(M);
        while (i > 0) : (i >>= 1) {
            try expectEqual(node2layout(layout2node(i)), i);
        }
    }
}

test "fast largest/smallest In Layout parsing" {
    try expectEqual(smallestInLayout(2), 2);
    try expectEqual(largestInLayout(2), 2);

    try expectEqual(largestInLayout(5), node2layout(3));
    try expectEqual(smallestInLayout(5), 1);

    try expectEqual(largestInLayout(24), 16);
    try expectEqual(smallestInLayout(24), 8);
}

test "fast de/composeLayout conversion" {
    const nodes1 = [_]Node{ 3, 2, 1 };
    const limit1: Node = nodes1.len;
    const layout1: Layout = 7;

    try expectEqual(composeLayout(limit1, nodes1), layout1);
    try expectEqual(decomposeLayout(limit1, layout1), nodes1);

    if (N >= 5) {
        const nodes2 = [_]Node{ 31, 30, 29, 27, 23, 16, 15, 8, 4, 2, 1 };
        const limit2: Node = nodes2.len;
        //const layout2: Layout = 7;

        //try expectEqual(composeLayout(limit2, nodes2), layout2);
        try expectEqual(decomposeLayout(limit2, composeLayout(limit2, nodes2)), nodes2);
    }
}

const TestWorkContext = struct {
    prev: u64 = @as(u64, node2layout(M)) << 1,
    fails: u64 = 0,
    count: u64 = 0,
    limit: Node,
};

fn testWork(name: Layout, args: *TestWorkContext) void {
    //try expect(name > args.prev);
    //try expectEqual(@popCount(Layout, name), args.limit);

    if (name >= args.prev) {
        args.fails += 1;
    }

    args.prev = name;
    args.count += 1;
}

const iterators = .{namedRecursiveIteration};

test "fast Named Iteration Count" {
    comptime var i = if (N >= 5) 13 else M / 2;

    comptime var j = 0;

    inline while (j < iterators.len) : (j += 1) {
        var args = TestWorkContext{ .limit = i };

        //const i = M / 2;
        iterators[j](i, &args, testWork);

        try expectEqual(args.count, Coeffs[M][i]);
        //try expectEqual(args.fails, 0);
    }
}

test "full Named Iteration Count" {
    comptime var i = M;

    inline while (i >= N) : (i -= 1) {
        var args = TestWorkContext{};

        //const i = M / 2;
        iterators[iterators.len - 1](i, &args, testWork);

        try expectEqual(args.count, Coeffs[M][i]);
        try expectEqual(args.fails, 0);
    }
}
