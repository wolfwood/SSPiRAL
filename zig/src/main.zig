const std = @import("std");
const assert = @import("std").debug.assert;

const builtin = @import("builtin");
const cpu_arch = builtin.cpu.arch;
//const has_avx = if (cpu_arch == .x86_64) std.Target.x86.featureSetHas(builtin.cpu.features, .avx) else false;
const has_avx512f = if (cpu_arch == .x86_64) std.Target.x86.featureSetHas(builtin.cpu.features, .avx512f) else false;
const has_bmi1 = if (cpu_arch == .x86_64) std.Target.x86.featureSetHas(builtin.cpu.features, .bmi) else false;

// constants
const N: u3 = 5;
const M: Node = (1 << N) - 1;
const ScoreSize = ((@as(u32, M) + 1) / 2) - N;
const MlSize = 131 + 1;

// types
const Node = bitsToType(N);
// there's a 2% overhead for using the arbitrary sized type, also compiler segfault when building tests
const Layout = u32; //bitsToType(M);
const Score = u32;
const ScoreIdx = u8;
const Combinadic = u32;

const totalLayoutCount = (@as(usize, 1) << M);

const vectorize = false;
const vectorAlign = 8;
const vectorizeAll = true;
const scaleVectorSize = true;

// encoding helpers

inline fn node2layout(node: Node) Layout {
    assert(node > 0);
    return @as(Layout, 1) << (node - 1);
}

inline fn layout2node(l: Layout) Node {
    assert(@popCount(l) == 1);
    // compiler doesn't know l is non-zero so the max result of @ctz is M - 1
    // (which fits in N bits even when incremented)
    return @as(Node, @intCast(@ctz(l))) + 1;
}

inline fn largestInLayout(l: Layout) Layout {
    assert(l != 0);
    const result = @as(Layout, 1) << (@bitSizeOf(Layout) - 1 - @clz(l));

    assert(result != 0);

    return result;
}

inline fn smallestInLayoutSlow(l: Layout) Layout {
    assert(l != 0);
    const result = @as(Layout, 1) << @ctz(l);

    assert(result != 0);

    return result;
}

inline fn smallestInLayout(l: Layout) Layout {
    if (has_bmi1) {
        return asm (
            \\blsi %[ret], %[l]
            : [ret] "=r" (-> @TypeOf(l)),
            : [l] "r" (l),
        );
    } else {
        return smallestInLayoutSlow(l);
    }
}

inline fn noOp(x: anytype) @TypeOf(x) {
    return x;
}

inline fn noOpLayout(x: Layout) Layout {
    return x;
}

inline fn layout2nodes(comptime limit: Node, name: Layout) [limit]Node {
    return _decomposeLayout(limit, name, Node, layout2node);
}

inline fn decomposeLayout(comptime limit: Node, name: Layout) [limit]Layout {
    return _decomposeLayout(limit, name, Layout, noOpLayout);
}

inline fn _decomposeLayout(comptime limit: Node, _name: Layout, comptime T: type, comptime pred: fn (Layout) callconv(.Inline) T) [limit]T {
    var result: [limit]T = undefined;

    var name = _name;
    comptime var i: i32 = limit - 1;

    inline while (i >= 0) : (i -= 1) {
        const l = smallestInLayout(name);
        assert(name != 0);
        name ^= l;

        result[i] = pred(l);
    }

    assert(name == 0);

    return result;
}

inline fn nodes2layout(comptime limit: Node, name: *const [limit]Node) Layout {
    return _composeLayout(limit, Node, name, node2layout);
}
inline fn composeLayout(comptime limit: Node, name: *const [limit]Layout) Layout {
    return _composeLayout(limit, Layout, name, noOpLayout);
}

inline fn _composeLayout(comptime limit: Node, comptime T: anytype, name: *const [limit]T, comptime pred: fn (T) callconv(.Inline) Layout) Layout {
    var result: Layout = 0;

    comptime var i: Node = 0;
    // XXX benchmark inline for (name) |l| {result |= pred(l);}
    inline while (i < limit) : (i += 1) {
        result |= pred(name[i]);
    }

    return result;
}

inline fn composeLayoutVector(comptime limit: Node, name: *const [limit]Layout) Layout {
    const len = comptime roundToAlignment(Layout, limit);
    const zeros: [len]Layout = @splat(0);

    comptime var i: i32 = 0;
    comptime var mask: @Vector(len, i32) = undefined;

    inline while (i < limit) : (i += 1) {
        mask[i] = i;
    }
    inline while (i < len) : (i += 1) {
        mask[i] = limit - i - 1;
    }
    const enns = @shuffle(Layout, name.*, zeros, mask);

    return @reduce(.Or, enns);
}

fn composeLayoutVectorAsm(comptime limit: Node, name: *align(64) const [limit]Layout) u32 {
    comptime assert(has_avx512f);

    const len = comptime roundToAlignment(Layout, limit);
    var zmm0: @Vector(if (limit > 16) 16 else len, u32) = undefined;
    const mask = (@as(u17, 1) << if (limit > 16) limit % 16 else limit) - 1;

    if (limit <= 8) {
        zmm0 = asm (
            \\vmovdqa32 %[name], %[ret] {%[maskreg]} {z}
            : [ret] "={ymm0}" (-> @TypeOf(zmm0)),
            : [name] "m" (name),
              [maskreg] "{k1}" (mask),
        );
    } else if (limit <= 16) {
        zmm0 = asm (
        //\\kmovw %[mask], %%k1
            \\vmovdqa32 %[name], %[ret] {%[maskreg]} {z}
            : [ret] "={zmm0}" (-> @TypeOf(zmm0)),
            : [name] "m" (name),
              //[mask] "mr" (mask),
              [maskreg] "{k1}" (mask),
        );
    } else {
        zmm0 = asm (
            \\vmovdqa32 %[name], %%zmm1
            \\vmovdqa32 %[uppername], %[ret] {%[maskreg]} {z}
            \\vpord %%zmm1, %[ret], %[ret]
            : [ret] "=v" (-> @TypeOf(zmm0)),
            : [name] "m" (name),
              [uppername] "m" (&name[16]),
              [maskreg] "{k1}" (mask),
            : "zmm1"
        );
    }

    return @reduce(.Or, zmm0);
}

// liveness checking

inline fn recurseCheck(name: Layout, n: Node, _i: Node, depth: Node) bool {
    var i = _i;

    while (i > 0) : (i -= 1) {
        if (node2layout(i) & name != 0) {
            const temp = i ^ n;
            if (temp == 0 or (depth > 0 and recurseCheck(name, temp, i - 1, depth - 1))) {
                return true;
            }
        }
    }
    return false;
}

fn checkIfAlive(name: Layout) bool {
    var n: Node = @as(Node, 1) << (N - 1);
    while (n != 0) : (n >>= 1) {
        const l = node2layout(n);
        if ((l & name) == 0) {
            if (!recurseCheck(name, n, M, M)) {
                return false;
            }
        }
    }
    return true;
}

// structs

fn LayoutStats(comptime verify: bool) type {
    return struct {
        name: if (verify) Layout else void,
        score: ScoreIdx,
    };
}

fn roundToAlignment(comptime T: type, comptime len: u32) u32 {
    const raw = (len * @sizeOf(T));

    //if (raw <= (64 / 8)) {
    //    return 8 / @sizeOf(T);
    //} else
    if (raw <= (128 / 8)) {
        return 16 / @sizeOf(T);
    } else if (raw <= (256 / 8)) {
        return 32 / @sizeOf(T);
    } else if (raw <= 512 / 8) {
        return 64 / @sizeOf(T);
    }

    return ((raw / 64) + @intFromBool(raw % 64 != 0)) * 64 / @sizeOf(T);
}

fn MetaLayout(comptime limit: Node) type {
    return struct {
        //scores: [std.math.min(ScoreSize, limit - (N - 1))]Score,
        scores: if (vectorizeAll)
            if (scaleVectorSize) @Vector(roundToAlignment(Score, howLongIsScores(limit)), Score) else @Vector(roundToAlignment(Score, ScoreSize), Score)
        else if (!vectorize or limit <= M / 2) [howLongIsScores(limit)]Score else @Vector(if (vectorAlign != 0) (howLongIsScores(limit) / vectorAlign + @intFromBool(howLongIsScores(limit) % vectorAlign != 0)) * vectorAlign else howLongIsScores(limit), Score),
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

            // XXX test alternative collections
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

    const result = NamedWorkContext(limit){
        .layouts = layouts,
        .prev_scores = prev_scores,
        .unique = std.AutoHashMap(MetaLayout(limit), ScoreIdx).init(alloc),
    };

    return result;
}

fn getScoreIndex(comptime limit: Node, args: *NamedWorkContext(limit)) !ScoreIdx {
    const result = try args.unique.getOrPut(args.unique_scores[args.curr_score]);

    if (!result.found_existing) {
        result.value_ptr.* = args.curr_score;
        args.curr_score += 1;
    }

    return result.value_ptr.*;
}

// passes

fn namedFirstPass(name: Layout, args: *NamedWorkContext(N)) void {
    args.layouts[name] = LayoutStats(false){ .score = @intFromBool(!checkIfAlive(name)), .name = undefined };

    // currently not using name validation
    if (@TypeOf(args.layouts[name].name) != void) {
        args.layouts[name].name = name;
    }
}

fn namedIntermediateLayoutPass(comptime limit: Node, name: Layout, ells: *const [limit]Layout, args: *NamedWorkContext(limit)) !void {
    sumChildLayoutScoresLayout(limit, name, ells, args);

    if (limit <= M / 2) {
        if (args.unique_scores[args.curr_score].scores[limit - N - 1] == limit) {
            args.unique_scores[args.curr_score].scores[limit - N] = @intFromBool(!checkIfAlive(name));
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
    const theOtherWay = @min(ScoreSize, limit - (N - 1));

    assert(oneWay == theOtherWay);

    return oneWay;
}

inline fn getPrevScore(comptime limit: Node, prev: Layout, args: *NamedWorkContext(limit)) *MetaLayout(limit - 1) {
    return &args.prev_scores[args.layouts[prev].score];
}

inline fn getCurrScore(comptime limit: Node, args: *NamedWorkContext(limit)) *MetaLayout(limit) {
    return &args.unique_scores[args.curr_score];
}

inline fn assignScore(comptime limit: Node, curr: *MetaLayout(limit), prev: *const MetaLayout(limit - 1)) void {
    comptime var i: u32 = 0;
    inline while (i < howLongIsScores(limit)) : (i += 1) {
        curr.scores[i] = prev.scores[i];
    }
}

inline fn addToScore(comptime limit: Node, curr: *MetaLayout(limit), prev: *const MetaLayout(limit - 1)) void {
    comptime var i: u32 = 0;
    inline while (i < prev.scores.len) : (i += 1) {
        curr.scores[i] += prev.scores[i];
    }
}

inline fn initializeCurr(comptime limit: Node, prevName: Layout, args: *NamedWorkContext(limit)) void {
    var curr = getCurrScore(limit, args);
    const prev = getPrevScore(limit, prevName, args);

    if (vectorizeAll or (vectorize and @TypeOf(curr.scores) == @TypeOf(prev.scores))) {
        if (@TypeOf(curr.scores) == @TypeOf(prev.scores)) {
            curr.scores = prev.scores;
        } else {
            const oldlen: i32 = comptime roundToAlignment(Score, howLongIsScores(limit - 1));
            const newlen = comptime roundToAlignment(Score, howLongIsScores(limit));
            const zeroes: @Vector(oldlen, Score) = @splat(0);
            comptime var i: i32 = 0;
            comptime var mask: @Vector(newlen, i32) = undefined;

            inline while (i < oldlen) : (i += 1) {
                mask[i] = i;
            }

            inline while (i < newlen) : (i += 1) {
                mask[i] = oldlen - i - 1; //~(i - oldlen);
            }

            curr.scores = @shuffle(Score, prev.scores, zeroes, mask);
        }
    } else {
        assignScore(limit, curr, prev);
    }
}

inline fn addToCurr(comptime limit: Node, prevName: Layout, args: *NamedWorkContext(limit)) void {
    var curr = getCurrScore(limit, args);
    const prev = getPrevScore(limit, prevName, args);

    if (vectorizeAll or (vectorize and @TypeOf(curr.scores) == @TypeOf(prev.scores))) {
        if (@TypeOf(curr.scores) == @TypeOf(prev.scores)) {
            curr.scores += prev.scores;
        } else if (false) {
            const oldlen = comptime roundToAlignment(Score, howLongIsScores(limit - 1));
            comptime var i: u32 = 0;

            inline while (i < oldlen) : (i += 1) {
                curr.scores[i] += prev.scores[i];
            }
        } else {
            const oldlen: i32 = comptime roundToAlignment(Score, howLongIsScores(limit - 1));
            const newlen = comptime roundToAlignment(Score, howLongIsScores(limit));
            const zeroes: @Vector(oldlen, Score) = @splat(0);
            comptime var i: i32 = 0;
            comptime var mask: @Vector(newlen, i32) = undefined;

            inline while (i < oldlen) : (i += 1) {
                mask[i] = i;
            }

            inline while (i < newlen) : (i += 1) {
                mask[i] = oldlen - i - 1; //~(i - oldlen);
            }

            curr.scores += @shuffle(Score, prev.scores, zeroes, mask);
        }
    } else {
        addToScore(limit, curr, prev);
    }
    //addToScore(limit, getCurrScore(limit, args), getPrevScore(limit, prev, args));
}

inline fn sumChildLayoutScoresLayout(comptime limit: Node, name: Layout, ells: *const [limit]Layout, args: *NamedWorkContext(limit)) void {
    initializeCurr(limit, name ^ ells[0], args);
    comptime var i = 1;

    inline while (i < limit) : (i += 1) {
        addToCurr(limit, name ^ ells[i], args);
    }
}

fn sumChildLayoutScores(comptime limit: Node, _name: Layout, args: *NamedWorkContext(limit)) void {
    var name = _name;
    var l = smallestInLayout(name); //node2layout(M);

    name ^= l;
    initializeCurr(limit, name, args);

    while (name != 0) {
        l = smallestInLayout(name);
        name ^= l;
        addToCurr(limit, _name ^ l, args);
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
) @typeInfo(@TypeOf(work)).Fn.return_type.? {
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
) @typeInfo(@TypeOf(work)).Fn.return_type.? {
    nriHelper(limit, 1, node2layout(M), 0, args, work);
}

inline fn nrliHelper(
    comptime limit: Node,
    //    comptime argtype: type,
    comptime i: Node,
    _l: Layout,
    name: Layout,
    ells: *[limit]Layout,
    args: anytype,
    //    comptime work: fn(comptime Node, @TypeOf(args)) void,
) !void {
    var l = _l;

    while (l > if (i == limit) 0 else @as(Layout, 1) << (limit - 1 - i)) : (l >>= 1) {
        //name |= l;
        ells[i - 1] = l;

        if (i == limit) {
            //work(name | l, args);
            try namedIntermediateLayoutPass(limit, name | l, ells, args);
        } else {
            try nrliHelper(
                limit,
                i + 1,
                l >> 1,
                name | l,
                ells,
                args, //work
            );
        }
        //name ^= l;
    }
}

fn namedLayoutIterationTest3(
    comptime limit: Node,
    //comptime argtype: type,
    args: anytype,
    //comptime work: fn (limit: Node, name: Layout, ells: []Layout, args: @TypeOf(args)) anyerror!void,
) // @typeInfo(@TypeOf(work)).Fn.return_type.? {
!void {
    var ells: [limit]Layout = undefined;
    ells[0] = node2layout(M);
    var name: Layout = ells[0];

    var i: Node = 1;

    while (i < limit) : (i += 1) {
        ells[i] = ells[i - 1] >> 1;
        name |= ells[i];
    }

    i = limit - 1;

    while (true) {
        assert(i == limit - 1);
        try testLayoutWork(limit, name, &ells, args);

        while ((node2layout(limit - i) == ells[i]) and (i > 0)) : (i -= 1) {
            name ^= ells[i];
        }

        if ((i == 0) and (node2layout(limit - i) == ells[i])) {
            break;
        }

        name ^= ells[i];
        ells[i] >>= 1;
        name ^= ells[i];

        while (i < limit - 1) {
            i += 1;

            ells[i] = ells[i - 1] >> 1;
            name ^= ells[i];
        }
    }
}

fn namedLayoutIteration3(
    comptime limit: Node,
    //comptime argtype: type,
    args: *NamedWorkContext(limit), //anytype,
    //comptime work: fn (limit: Node, name: Layout, ells: []Layout, args: @TypeOf(args)) anyerror!void,
) // @typeInfo(@TypeOf(work)).Fn.return_type.? {
!void {
    var ells: [limit]Layout = undefined;
    ells[0] = node2layout(M);
    var name: Layout = ells[0];

    {
        comptime var i: Node = 1;

        inline while (i < limit) : (i += 1) {
            ells[i] = ells[i - 1] >> 1;
            name |= ells[i];
        }
    }

    var i = limit - 1;

    while (true) {
        assert(i == limit - 1);
        try namedIntermediateLayoutPass(limit, name, &ells, args);

        while ((node2layout(limit - i) == ells[i]) and (i > 0)) : (i -= 1) {
            name ^= ells[i];
        }

        if ((i == 0) and (node2layout(limit - i) == ells[i])) {
            break;
        }

        name ^= ells[i];
        ells[i] >>= 1;
        name ^= ells[i];

        while (i < limit - 1) {
            i += 1;

            ells[i] = ells[i - 1] >> 1;
            name ^= ells[i];
        }
    }
}

const Allocator = std.mem.Allocator;

inline fn unroll(
    comptime limit: Node,
    //comptime iterator: fn (comptime limit: Node, args: anytype, comptime work: anytype) void,
    alloc: Allocator,
    prev_ctx: *NamedWorkContext(limit - 1),
) !MetaLayout(M) {
    //const t = tracy.trace(@src(), null);
    //defer t.end();

    var ctx = MakeWorkContext(limit, alloc, prev_ctx);

    //try namedRecursiveLayoutIteration(limit, &ctx);
    try namedLayoutIteration3(limit, &ctx);

    normalize(limit, &ctx);

    if (limit < M) {
        return try unroll(limit + 1, alloc, &ctx);
    } else {
        assert(ctx.curr_score == 1);

        return ctx.unique_scores[0];
    }
}

const tracy = @import("tracy");

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();

    try stdout.print("{} of {}\n", .{ N, M });

    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();

    const allocator = arena.allocator();

    //var stats: [totalLayoutCount]LayoutStats(false) = undefined;
    const stats = try allocator.alloc(LayoutStats(false), totalLayoutCount);

    var ctx0 = NamedWorkContext(N){
        .layouts = stats,
        .unique_scores = [2]MetaLayout(N){
            MetaLayout(N){ .scores = if (vectorizeAll) @splat(0) else [1]Score{0} },
            MetaLayout(N){ .scores = if (vectorizeAll) @splat(0) else [1]Score{1} },
        },
    };
    if (vectorizeAll) {
        ctx0.unique_scores[1].scores[0] = 1;
    }

    //const t = tracy.trace(@src(), null);
    //defer t.end();
    //const frame = tracy.frame(null);
    namedRecursiveIteration(N, &ctx0, namedFirstPass);
    const result = try unroll(N + 1, allocator, &ctx0);

    // frame.end();

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
    comptime var co: [@as(u32, M) + 1][@as(u32, M) + 1]Combinadic = undefined;
    comptime var __i: u32 = 1;

    return inline while (__i <= M) : (__i += 1) {
        co[0][0] = 1;
        co[__i][0] = 1;
        co[__i][__i] = 1;

        comptime var j: u32 = 1;
        inline while (j < __i) : (j += 1) {
            co[__i][j] = co[__i - 1][j] + co[__i - 1][j - 1];
        }
    } else co;
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

test "fast nodes <=> layout conversion" {
    const nodes1 = [_]Node{ 3, 2, 1 };
    const limit1: Node = nodes1.len;
    const layout1: Layout = 7;

    try expectEqual(nodes2layout(limit1, &nodes1), layout1);
    try expectEqual(layout2nodes(limit1, layout1), nodes1);

    if (N >= 5) {
        const nodes2 = [_]Node{ 31, 30, 29, 27, 23, 16, 15, 8, 4, 2, 1 };
        const limit2: Node = nodes2.len;
        //const layout2: Layout = 7;

        //try expectEqual(composeLayout(limit2, nodes2), layout2);
        try expectEqual(layout2nodes(limit2, nodes2layout(limit2, &nodes2)), nodes2);
    }
}

inline fn map(comptime a: anytype, comptime T: type, comptime pred: fn (@typeInfo(@TypeOf(a)).Array.child) callconv(.Inline) T) [@typeInfo(@TypeOf(a)).Array.len]T {
    //comptime var i = 0;

    var result: [@typeInfo(@TypeOf(a)).Array.len]T = undefined;

    inline for (a, 0..) |e, i| {
        result[i] = pred(e);
    }

    return result;
}

test "fast de/composeLayout conversion" {
    const nodes1 = [_]Node{ 3, 2, 1 };
    const layouts1 = map(nodes1, Layout, node2layout);
    const limit1: Node = nodes1.len;
    const layout1: Layout = 7;

    try expectEqual(composeLayout(limit1, &layouts1), layout1);
    try expectEqual(decomposeLayout(limit1, layout1), layouts1);

    if (N >= 5) {
        const nodes2 = [_]Node{ 31, 30, 29, 27, 23, 16, 15, 8, 4, 2, 1 };
        const layouts2 = map(nodes2, Layout, node2layout);
        const limit2: Node = nodes2.len;

        try expectEqual(decomposeLayout(limit2, composeLayout(limit2, &layouts2)), layouts2);
    }
}

const TestWorkContext = struct {
    prev: u64 = @as(u64, node2layout(M)) << 1,
    fails: u64 = 0,
    count: u32 = 0,
    limit: Node,
};

test "vector alignment" {
    const stdout = std.io.getStdOut().writer();

    try stdout.print("Vector alignment: {}\n", .{@typeInfo(*@Vector(ScoreSize, Score)).Pointer.alignment});
}

fn testWork(name: Layout, args: *TestWorkContext) void {
    //try expect(name < args.prev);
    //try expectEqual(@popCount(Layout, name), args.limit);

    if (name >= args.prev) {
        args.fails += 1;
    }

    args.prev = name;
    args.count += 1;
}

fn testLayoutWork(comptime limit: Node, name: Layout, ells: *[limit]Layout, args: *TestWorkContext) !void {
    assert(@popCount(name) == limit);
    assert(name == composeLayout(limit, ells));
    assert(name < args.prev);

    testWork(name, args);
}

const iterators = .{namedRecursiveIteration};
const layoutIterators = .{namedLayoutIterationTest3};

test "fast Named Iteration Count" {
    const i = if (N >= 5) 13 else M / 2;

    inline for (iterators) |iter| {
        var args = TestWorkContext{ .limit = i };

        iter(i, &args, testWork);

        try expectEqual(Coeffs[M][i], args.count);
        try expectEqual(args.fails, 0);
    }

    inline for (layoutIterators) |iter| {
        var args = TestWorkContext{ .limit = i };

        //try iter(i, &args, testLayoutWork);
        try iter(i, &args);

        try expectEqual(Coeffs[M][i], args.count);
        try expectEqual(args.fails, 0);
    }
}

test "full Named Iteration Count" {
    comptime var i = M;

    inline while (i >= N) : (i -= 1) {
        var args = TestWorkContext{ .limit = i };

        iterators[iterators.len - 1](i, &args, testWork);

        try expectEqual(args.count, Coeffs[M][i]);
        try expectEqual(args.fails, 0);
    }
}
