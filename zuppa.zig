const std = @import("std");

// constants
const N: u3 = 5;
const M: Node = (1 << N) - 1;
const ScoreSize = ((M + 1) / 2) - N;
const MlSize = 131 + 1;

// types
const Node = bitsToType(N);
const LayoutName = bitsToType(M);
const Score = u32;
const MlIdx = u8;
const Combinadic = u32;

fn Layout(comptime verify: bool) type {
    return struct {
        name: if (verify) LayoutName else void,
        score: MlIdx,
    };
}

// XXX parameterize by limit
fn MetaLayout(comptime limit: Node) type {
    return struct {
        scores: [ScoreSize]Score,
    };
}

fn WorkContext(comptime verify: bool, comptime limit: Node) type {
    const layout_count = Coeffs(M, limit);
    return struct {
        layouts: [layout_count]Layout(verify),
        unique_scores: [MlSize]MetaLayout(limit),

        curr_layout: Combinadic,
        curr_score: ScoreIdx,

        // XXX some dynamic dictionary from MetaLayout -> ScoreIdx
        unique: std.AutoHashMap(MetaLayout, ScoreIdx),
    };
}

fn FirstBlushIterator(comptime verify: bool, comptime limit: Node) type {
    return struct {
        work: *WorkContext(verify, limit),
        Is: Node,
        name: if (verify) LayoutName else void,
    };
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();

    try stdout.print("Hello, {} {}\n", .{ "world", Coeffs[31][15] });
}

// lookup tables
const Coeffs = {
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
};

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
