#abstract type Node <: UInt8 end
#abstract type Layout <: UInt32 end
#abstract type Score <: UInt32 end
#abstract type MlIdx <: UInt8 end

const Node = UInt8
const Layout = UInt32
const Score = UInt32
const MlIdx = UInt8

const N = Node(5)
const M = Layout(2^N - 1)
const ScoreSize = Score(((M+1) / 2) - N)
const MlSize = MlIdx(131+1)

_Coeffs = Array{Score, 2}(undef, M+1, M+1)

_Coeffs[1, 1] = 1

for i in 2:(M+1)
    _Coeffs[i,1] = 1
    _Coeffs[i,i] = 1

    for j in 2:(i-1)
        _Coeffs[i,j] = _Coeffs[i-1,j] +_Coeffs[i-1,j-1]
    end
end

const Coeffs = _Coeffs
#_Coeffs = nothing

function coeffs(i, j)
    Coeffs[i+1, j+1]
end

function node2layout(n::Node)::Layout
    one(Layout) << (n - 1)
end

struct Lay
    scoreIdx::MlIdx
end

struct NamedLay
    scoreIdx::MlIdx
    name::Layout
end

struct MetaLayout
    scores::Array{Score}
end

mutable struct WorkContext
    curr::Array{Lay}
    pos::Layout
    ml::Array{MetaLayout}
    ml_idx::MlIdx
    lookup::Dict{MetaLayout,MlIdx}
end

function contextForLimit(limit::Node)::WorkContext
   WorkContext(Array{Lay}(undef, coeffs(M,limit)), 1, Array{MetaLayout}(undef, ScoreSize), 1, Dict{MetaLayout,MlIdx}())
end

function contextStart()::WorkContext
   WorkContext(Array{Lay}(undef, coeffs(M,N)), 1, Array{MetaLayout}(undef, ScoreSize), 1, Dict{MetaLayout,MlIdx}(MetaLayout([1]) => 0, MetaLayout([0]) => 1))
end

function getScoreIdx!(work::WorkContext, ml::MetaLayout)::MlIdx


function deadnessCheck(Is::Array{Node}, limit::Node)::Bool
    j::Node = limit+1

    for n::Node in [1 << x for x=N-1:-1:1]
        while j > 2 && (Is[j]+1) < n
            j -= 1
        end

        if Is[j]+1 != n
            temp::Node = n

            SENTINEL = 3
            Js::Array{Node} = fill(SENTINEL, (limit+1))

            Js[1] = 0

            alive::Bool = false
            i = 2

            while 1 < i
                if 0 == Js[i]
                    Js[i] = SENTINEL
                    i -= 1
                else
                    if SENTINEL == Js[i]
                        Js[i] = 2
                    end

                    temp ⊻= Is[i] +1

                    if 0 == temp
                        alive = true
                        break
                    end

                    Js[i] -= 1
                    if limit >= i
                        i += 1
                    end
                end
            end

            if !alive
                return true
            end
        end
    end

    false
end

function buildDeltas(Is::Array{Node}, limit::Node)
    deltas::Array{UInt32} = Array{UInt32}(undef, limit-1)
    idx = 0::UInt32

    for i in 2:limit
        before = coeffs(Is[i], limit- (i-1))
        after = coeffs(Is[i+1], limit- (i) + 1)

        @assert before > after "bad deltas"
        deltas[i-2] = before - after
        idx += after
    end

    (idx, deltas)
end

function sumChildScores(prev:WorkContext, Is::Array{Node}, limit::Node)
    (idx, deltas) = buildDeltas(Is, limit)

    score = Array{Score}(undef, limit-N+1)

    score[1:limit-N] = prev.ml[prev.curr[idx].scoreIdx].scores

    for d in deltas
        idx += d

        score[1:limit-N] = prev.ml[prev.curr[idx].scoreIdx].scores
    end

    scores
end


function foundationalWork!(work::WorkContext, prev::WorkContext, Is::Array{Node}, limit::Node)
    work.curr[work.pos] = Lay(deadnessCheck(Is, limit))
    work.pos += 1
end

function intermediateWork!(work::WorkContext, prev::WorkContext, Is::Array{Node}, limit::Node)
    s = sumChildScores(prev, Is, limit)

    if limit == s[limit - N]
        s[limit - N + 1] = Score(deadnessCheck(Is, limit))
    else
        s[limit - N + 1] = 0
    end

    work.curr[work.pos] = Lay(lookupScoreIdx!(work, MetaLayout(s))

    work.pos += 1
end

function walkOrdered!(work::WorkContext, prev::WorkContext, limit::Node, oneStep::Function)
    i = 2
    SENTINEL = 1
    Is::Array{Node} = fill(SENTINEL, (limit+1))

    Is[1] = M+1

    while i > 1
        if SENTINEL == Is[i]
            Is[i] = Is[i-1]
        end

        Is[i] -= 1

        if limit+1 == i
            oneStep(work, prev, Is, limit)
        end

        if SENTINEL == Is[i]
            i -= 1
        elseif limit >= i
            i += 1
        end
    end

    @assert (work.pos-1 == coeffs(M,limit)) "didn't iterate enough"
end

function wrapper()
    println("$N of $M")

    prev::WorkContext = contextStart()

    walkOrdered!(prev, prev, N, foundationalWork!)

    for i::UInt8 in N+1:fld(M,2)
        println("-$i")

        curr::WorkContext = contextForLimit(i)

        walkOrdered!(curr, prev, i, foundationalWork!)
        #walkOrdered!(curr, prev, i, intermediateWork!)

        #normalize!(curr, i)

        prev = curr
    end

    for i::UInt8 in fld(M,2)+1:M
    #for i::UInt8 in fld(M,2):-1:1
       println("$i")

        curr::WorkContext = contextForLimit(i)

        walkOrdered!(curr, prev, i, foundationalWork!)
        #walkOrdered!(curr, prev, i, terminalWork!)

        #normalize!(curr, i)

        @assert (curr.pos-1 == coeffs(M,M-i)) "didn't iterate enough"

        prev = curr
    end

    # printScore
end

#import Profile.*

#Profile.clear()

#@profile wrapper()

wrapper()

#Profile.print()

#Profile.clear()
