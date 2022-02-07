const std = @import("std");
const assert = @import("std").debug.assert;

const builtin = @import("builtin");
const cpu_arch = builtin.cpu.arch;
//const has_avx = if (cpu_arch == .x86_64) std.Target.x86.featureSetHas(builtin.cpu.features, .avx) else false;
const has_avx512f = if (cpu_arch == .x86_64) std.Target.x86.featureSetHas(builtin.cpu.features, .avx512f) else false;
const has_bmi1 = if (cpu_arch == .x86_64) std.Target.x86.featureSetHas(builtin.cpu.features, .bmi1) else false;

// constants
const N: u3 = 5;
const M: Node = (1 << N) - 1;
const ScoreSize = ((@as(u32, M) + 1) / 2) - N;
const MlSize = 131 + 1;

// types
const Node = bitsToType(N);
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

inline fn smallestInLayoutSlow(l: Layout) Layout {
    assert(l != 0);
    var result = @as(Layout, 1) << (@ctz(Layout, l));

    assert(result != 0);

    return result;
}

inline fn smallestInLayout(l: u32) @TypeOf(l) {
    comptime assert(has_bmi1);

    return asm (
        \\blsi %[ret], %[l]
        : [ret] "=r" (-> @TypeOf(l)),
        : [l] "r" (l),
    );
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

inline fn _decomposeLayout2(comptime limit: Node, name: Layout, comptime T: type, comptime pred: fn (Layout) callconv(.Inline) T) [limit]T {
    var result: [limit]T = undefined;

    var i: u32 = 0;
    var l = largestInLayout(name);
    var stop = smallestInLayout(name);

    while (l >= stop) : (l >>= 1) {
        if (name & l != 0) {
            result[i] = pred(l);
            i += 1;
        }
    }

    return result;
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
    inline while (i < limit) : (i += 1) {
        result |= pred(name[i]);
        //    for (name) |l| {
        //        result |= pred(l);
    }

    return result;
}

inline fn composeLayoutVector(comptime limit: Node, name: *const [limit]Layout) Layout {
    const len = comptime roundToAlignment(Layout, limit);
    const zeros = @splat(len, @as(Layout, 0));

    comptime var i: i32 = 0;
    comptime var mask: @Vector(len, i32) = undefined;

    inline while (i < limit) : (i += 1) {
        mask[i] = i;
    }
    inline while (i < len) : (i += 1) {
        mask[i] = limit - i - 1;
    }
    const enns = @shuffle(Layout, name.*, zeros, mask);

    // result: Layout = 0;

    return @reduce(.Or, enns);
}

fn composeLayoutVectorAsm(comptime limit: Node, name: *align(64) const [limit]Layout) u32 {
    comptime assert(has_avx512f);

    const len = comptime roundToAlignment(Layout, limit);
    var zmm0: Vector(if (limit > 16) 16 else len, u32) = undefined;
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

const Vector = @import("std").meta.Vector;

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

    return
    //if (raw <= (256 / 8)) 32 / @sizeOf(T) else if (raw <= 512 / 8) 64 / @sizeOf(T) else
    ((raw / 64) + @boolToInt(raw % 64 != 0)) * 64 / @sizeOf(T);
}

// XXX parameterize by limit
fn MetaLayout(comptime limit: Node) type {
    return struct {
        //scores: [std.math.min(ScoreSize, limit - (N - 1))]Score,
        scores: if (vectorizeAll)
            if (scaleVectorSize) Vector(roundToAlignment(Score, howLongIsScores(limit)), Score) else Vector(roundToAlignment(Score, ScoreSize), Score)
        else if (!vectorize or limit <= M / 2) [howLongIsScores(limit)]Score else Vector(if (vectorAlign != 0) (howLongIsScores(limit) / vectorAlign + @boolToInt(howLongIsScores(limit) % vectorAlign != 0)) * vectorAlign else howLongIsScores(limit), Score),
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

fn namedIntermediateLayoutPass(comptime limit: Node, name: Layout, ells: *const [limit]Layout, args: *NamedWorkContext(limit)) !void {
    sumChildLayoutScoresLayout(limit, name, ells, args);

    if (limit <= M / 2) {
        if (args.unique_scores[args.curr_score].scores[limit - N - 1] == limit) {
            args.unique_scores[args.curr_score].scores[limit - N] = @boolToInt(!checkIfAlive(name));
        } else {
            args.unique_scores[args.curr_score].scores[limit - N] = 0;
        }
    }

    args.layouts[name].score = try getScoreIndex(limit, args);
}

fn namedIntermediateLayoutPtrPass(comptime limit: Node, name: Layout, ells: *[limit][*]const Layout, args: *NamedWorkContext(limit)) !void {
    sumChildLayoutScoresLayoutPtr(limit, name, ells, args);

    if (limit <= M / 2) {
        if (args.unique_scores[args.curr_score].scores[limit - N - 1] == limit) {
            args.unique_scores[args.curr_score].scores[limit - N] = @boolToInt(!checkIfAlive(name));
        } else {
            args.unique_scores[args.curr_score].scores[limit - N] = 0;
        }
    }

    args.layouts[name].score = try getScoreIndex(limit, args);
}

fn namedIntermediateLayoutOnlyPass(comptime limit: Node, ells: *const [limit]Layout, args: *NamedWorkContext(limit)) !void {
    const name = composeLayout(limit, ells);

    sumChildLayoutScoresLayout(limit, name, ells, args);

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
            const zeroes = @splat(oldlen, @as(Score, 0));
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
            const zeroes = @splat(oldlen, @as(Score, 0));
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

fn sumChildLayoutScoresSlow(comptime limit: Node, name: Layout, args: *NamedWorkContext(limit)) void {
    // we skip the last entry if we are in the intermediate phase because it gets filled in from liveness check
    //const length = if (limit - 1 <= M / 2) limit - N else ScoreSize;
    var l = largestInLayout(name); //node2layout(M);

    initializeCurr(limit, name ^ l, args);

    l >>= 1;

    while (l >= smallestInLayout(name)) : (l >>= 1) {
        if (name & l != 0) {
            addToCurr(limit, name ^ l, args);
        }
    }
}

fn sumChildLayoutScoresWrapper(comptime limit: Node, name: Layout, args: *NamedWorkContext(limit)) void {
    var ells = decomposeLayout(limit, name);
    sumChildLayoutScoresLayout(limit, name, &ells, args);
}

inline fn sumChildLayoutScoresLayout(comptime limit: Node, name: Layout, ells: *const [limit]Layout, args: *NamedWorkContext(limit)) void {
    initializeCurr(limit, name ^ ells[0], args);
    comptime var i = 1;

    inline while (i < limit) : (i += 1) {
        addToCurr(limit, name ^ ells[i], args);
    }
}

inline fn sumChildLayoutScoresLayoutPtr(comptime limit: Node, name: Layout, ells: *[limit][*]const Layout, args: *NamedWorkContext(limit)) void {
    initializeCurr(limit, name ^ ells[0][0], args);
    comptime var i = 1;

    inline while (i < limit) : (i += 1) {
        addToCurr(limit, name ^ ells[i][0], args);
    }
}

inline fn sumChildLayoutScoresLayoutOnly(comptime limit: Node, ells: *const [limit]Layout, args: *NamedWorkContext(limit)) void {
    initializeCurr(limit, ells[0], args);
    comptime var i = 1;

    inline while (i < limit) : (i += 1) {
        addToCurr(limit, ells[i], args);
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

fn sumChildLayoutScores2(comptime limit: Node, _name: Layout, args: *NamedWorkContext(limit)) void {
    //var curr = getCurrScore(limit, args);
    var name = _name;
    var l = smallestInLayout(name); //node2layout(M);

    name ^= l;
    {
        //var prev = getPrevScore(limit, _name ^ l, args);
        initializeCurr(limit, name, args);
    }
    while (name != 0) {
        l = smallestInLayout(name);
        name ^= l;

        //var prev = getPrevScore(limit, _name ^ l, args);
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

//fn NamedLayoutIterationWork(comptime argtype: type) type {
//    return fn (name: Layout, ells: []Layout, args: argtype) void;
//}

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

fn namedRecursiveLayoutIteration(
    comptime limit: Node,
    //comptime argtype: type,
    args: anytype,
    //comptime work: fn (name: Layout, args: @TypeOf(args)) void,
) // @typeInfo(@TypeOf(work)).Fn.return_type.? {
!void {
    var ells: [limit]Layout = undefined;

    try nrliHelper(
        limit,
        1,
        node2layout(M),
        0,
        &ells,
        args,
        //work
    );
}

fn namedLayoutIteration(
    comptime limit: Node,
    //comptime argtype: type,
    args: anytype,
    //comptime work: fn (name: Layout, args: @TypeOf(args)) void,
) // @typeInfo(@TypeOf(work)).Fn.return_type.? {
!void {
    var name: Layout = 0;
    var ells: [limit]Layout = undefined;

    var i: Node = 1;

    ells[i] = node2layout(M);

    while (i < limit) : (i += 1) {
        ells[i] = M - i + 1;
    }

    i = 0;

    while (ells[0] > comptime (@as(Layout, 1) << (limit - 1))) : (ells[0] >>= 1) {
        name |= ells[i];

        while (i > 0) {
            var stop = (@as(Layout, 1) << (limit - i - 1));

            if (ells[i] <= (@as(Layout, 1) << (limit - i - 1))) {
                ells[i] = ells[i - 1];
            } else {
                name ^= ells[i];
            }

            ells[i] >>= 1;
            name |= ells[i];

            if (i == comptime (limit - 2)) {
                ells[limit - 1] = ells[limit - 2] >> 1;

                while (ells[limit - 1] > 0) : (ells[limit - 1] >>= 1) {
                    try namedIntermediateLayoutPass(limit, ells[i] | name, &ells, args);
                }

                if (ells[i] <= stop) {
                    name ^= ells[i];
                    i -= 1;
                }
            } else if (ells[i] <= stop) {
                name ^= ells[i];
                i -= 1;
            } else {
                i += 1;
            }
        }
        name ^= ells[i];
    }
}

fn namedLayoutIterationTest(
    comptime limit: Node,
    //comptime argtype: type,
    args: anytype,
    comptime work: fn (limit: Node, name: Layout, ells: []Layout, args: @TypeOf(args)) void,
) // @typeInfo(@TypeOf(work)).Fn.return_type.? {
!void {
    var name: Layout = 0;
    var ells: [limit]Layout = undefined;

    var i: Node = 1;

    ells[0] = node2layout(M);

    while (i < limit) : (i += 1) {
        ells[i] = 0; //@as(Layout, 1) << (M - i);
    }

    i = 0;

    while (ells[0] > 1) : (ells[0] >>= 1) {
        name |= ells[i];

        while (0 < i) {
            //var stop = (@as(Layout, 1) << (limit - i - 2));

            if (0 == ells[i]) { // (@as(Layout, 1) << (limit - i - 1))) {
                ells[i] = ells[i - 1];
            } else {
                name ^= ells[i];
            }

            ells[i] >>= 1;
            name |= ells[i];

            if (i == comptime (limit - 1)) {
                work(limit, name, &ells, args);
            }

            if (ells[i] <= 1) {
                name ^= ells[i];
                ells[i] = 0;
                i -= 1;
            } else {
                if (i < (limit - 1)) {
                    i += 1;
                }
            }
        }
        name ^= ells[i];
    }
}

fn namedLayoutIterationTest2(
    comptime limit: Node,
    //comptime argtype: type,
    args: anytype,
    comptime work: fn (limit: Node, name: Layout, ells: []Layout, args: @TypeOf(args)) void,
) // @typeInfo(@TypeOf(work)).Fn.return_type.? {
!void {
    var name: Layout = 0;
    var ells: [limit]Layout = undefined;

    var i: Node = 1;

    ells[0] = node2layout(M);

    while (i < limit) : (i += 1) {
        ells[i] = ells[i - 1] >> 1;
    }

    while (true) {
        work(limit, ells[i] | name, &ells, args);

        name |= ells[i];

        while (i > 0) {
            var stop = (@as(Layout, 1) << (limit - i - 2));

            if (ells[i] <= (@as(Layout, 1) << (limit - i - 1))) {
                ells[i] = ells[i - 1];
            } else {
                name ^= ells[i];
            }

            ells[i] >>= 1;
            name |= ells[i];

            if (false) {
                if (i == comptime (limit - 2)) {
                    ells[limit - 1] = ells[i] >> 1;

                    while (ells[limit - 1] > 0) : (ells[limit - 1] >>= 1) {}

                    if (ells[i] <= stop) {
                        name ^= ells[i];
                        i -= 1;
                    }
                } else if (ells[i] <= stop) {
                    name ^= ells[i];
                    i -= 1;
                } else {
                    i += 1;
                }
            } else {
                if (i == comptime (limit - 1)) {
                    work(limit, ells[i] | name, &ells, args);
                }

                if (ells[i] <= stop) {
                    name ^= ells[i];
                    i -= 1;
                } else {
                    i += 1;
                }
            }
        }
        name ^= ells[i];
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

        while (i < limit - 1) { //(true) : (i += 1) {
            i += 1;

            //name ^= ells[i];
            ells[i] = ells[i - 1] >> 1;
            name ^= ells[i];

            //if (i == limit - 1) {
            //    break;
            //}
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
            //while ((node2layout(limit - i) == ells[i])) : (i -= 1) {
            //if (i == 0) {
            //    break;
            //}
            name ^= ells[i];
        }

        if ((i == 0) and (node2layout(limit - i) == ells[i])) {
            break;
        }

        name ^= ells[i];
        ells[i] >>= 1;
        name ^= ells[i];

        while (i < limit - 1) { //(true) : (i += 1) {
            i += 1;

            //name ^= ells[i];
            ells[i] = ells[i - 1] >> 1;
            name ^= ells[i];

            //if (i == limit - 1) {
            //    break;
            //}
        }
    }
}

fn namedLayoutIteration4(
    comptime limit: Node,
    args: *NamedWorkContext(limit),
) !void {
    var ells: [limit]Layout = undefined;
    ells[0] = node2layout(M);

    {
        comptime var i: Node = 1;

        inline while (i < limit) : (i += 1) {
            ells[i] = ells[i - 1] >> 1;
        }
    }

    var i = limit - 1;

    while (true) {
        assert(i == limit - 1);
        try namedIntermediateLayoutOnlyPass(limit, &ells, args);

        while ((node2layout(limit - i) == ells[i]) and (i > 0)) : (i -= 1) {}

        if ((i == 0) and (node2layout(limit - i) == ells[i])) {
            break;
        }

        ells[i] >>= 1;

        while (i < limit - 1) {
            i += 1;

            ells[i] = ells[i - 1] >> 1;
        }
    }
}

fn namedLayoutIteration5(
    comptime limit: Node,
    args: *NamedWorkContext(limit),
) !void {
    const len = M - limit + 1;

    comptime var j = 0;
    comptime var _values: [limit][len]Layout = undefined;

    const values: [limit][len]Layout = inline while (j < _values.len) : (j += 1) {
        comptime var k = 0;

        inline while (k < len) : (k += 1) {
            _values[j][len - 1 - k] = comptime node2layout(M - j - k);
        }
    } else _values;

    var m: u32 = 0;
    assert(values[0][len - 1] == node2layout(M));
    while (m < values.len) : (m += 1) {
        var n: u32 = 0;
        while (n < len) : (n += 1) {
            assert(values[m][n] != 0);
            if (m > 0) {
                assert(values[m][n] == (values[m - 1][n] >> 1));
            }
            if (n > 0) {
                assert(values[m][n - 1] == values[m][n] >> 1);
            }
        }
    }

    var name: Layout = 0;
    var ells: [limit][*]const Layout = undefined;
    {
        comptime var i: Node = 0;

        inline while (i < limit) : (i += 1) {
            ells[i] = &values[i]; //@ptrCast(*const [len]Layout, &values[i]);
            ells[i] += len - 1;
            assert(ells[i][0] == values[i][len - 1]);
            name |= ells[i][0];
        }
    }

    var i = limit - 1;

    while (true) {
        assert(i == limit - 1);
        assert(@popCount(Layout, name) == limit);
        try namedIntermediateLayoutPtrPass(limit, name, &ells, args);

        while ((&values[i] == ells[i]) and (i > 0)) : (i -= 1) {
            name ^= ells[i][0];
        }

        if ((i == 0) and (&values[i] == ells[i])) {
            break;
        }

        name ^= ells[i][0];
        ells[i] -= 1;
        name ^= ells[i][0];

        while (i < limit - 1) {
            i += 1;

            ells[i] = ells[i - 1] + len;
            assert(ells[i][0] == ells[i - 1][0] >> 1);
            name ^= ells[i][0];
        }
    }
}

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

inline fn unroll(
    comptime limit: Node,
    //comptime iterator: fn (comptime limit: Node, args: anytype, comptime work: anyty3pe) void,
    alloc: Allocator,
    prev_ctx: *NamedWorkContext(limit - 1),
) !MetaLayout(M) {
    //const t = tracy.trace(@src(), null);
    //defer t.end();

    var ctx = MakeWorkContext(limit, alloc, prev_ctx);

    //try namedRecursiveIteration3(limit, &ctx);
    //try namedRecursiveLayoutIteration(limit, &ctx);
    try namedLayoutIteration3(limit, &ctx);
    //try namedLayoutIteration4(limit, &ctx);
    //try namedLayoutIteration5(limit, &ctx);

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
            MetaLayout(N){ .scores = if (vectorizeAll) @splat(@typeInfo(@typeInfo(MetaLayout(N)).Struct.fields[0].field_type).Vector.len, @as(Score, 0)) else [1]Score{0} },
            MetaLayout(N){ .scores = if (vectorizeAll) @splat(@typeInfo(@typeInfo(MetaLayout(N)).Struct.fields[0].field_type).Vector.len, @as(Score, 0)) else [1]Score{1} },
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
    //    return co;
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

    inline for (a) |e, i| {
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
        //const layout2: Layout = 7;

        //try expectEqual(composeLayout(limit2, nodes2), layout2);
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

    try stdout.print("align {}\n", .{@typeInfo(*@Vector(ScoreSize, Score)).Pointer.alignment});
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
    assert(@popCount(Layout, name) == limit);
    assert(name == composeLayout(limit, ells));
    assert(name < args.prev);

    testWork(name, args);
}

const iterators = .{namedRecursiveIteration};
const layoutIterators = .{namedLayoutIterationTest3};

test "fast Named Iteration Count" {
    comptime var i = if (N >= 5) 13 else M / 2;

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

        //const i = M / 2;
        iterators[iterators.len - 1](i, &args, testWork);

        try expectEqual(args.count, Coeffs[M][i]);
        try expectEqual(args.fails, 0);
    }
}
