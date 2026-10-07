import subprocess
import sys

binary = sys.argv[1]

def program(body, result="Nat", argument="Nat"):
    return f"language core; fn main(n:{argument})->{result} {{ return {body} }}"

errors = {
    "MISSING_MAIN": "language core;",
    "UNDEFINED_VARIABLE": program("unknown"),
    "UNEXPECTED_TYPE_FOR_EXPRESSION": program("true"),
    "NOT_A_FUNCTION": program("0(unknown)"),
    "NOT_A_TUPLE": program("0.1"),
    "NOT_A_RECORD": program("0.x"),
    "NOT_A_LIST": program("List::head(0)"),
    "UNEXPECTED_LAMBDA": program("fn(x:Nat) { return unknown }"),
    "UNEXPECTED_TYPE_FOR_PARAMETER": program("fn(x:Bool) { return unknown }", "fn(Nat)->Nat"),
    "UNEXPECTED_TUPLE": program("{unknown,0}"),
    "UNEXPECTED_RECORD": program("{x=unknown}"),
    "UNEXPECTED_VARIANT": program("<|x=unknown|>"),
    "UNEXPECTED_LIST": program("[unknown]"),
    "UNEXPECTED_INJECTION": program("inl(unknown)"),
    "MISSING_RECORD_FIELDS": program("{x=0}", "{x:Nat,y:Nat}"),
    "UNEXPECTED_RECORD_FIELDS": program("{x=0,y=0}", "{x:Nat}"),
    "UNEXPECTED_FIELD_ACCESS": program("({x=0}).y"),
    "UNEXPECTED_VARIANT_LABEL": program("<|y=0|>", "<|x:Nat|>"),
    "TUPLE_INDEX_OUT_OF_BOUNDS": program("{0,true}.3"),
    "UNEXPECTED_TUPLE_LENGTH": program("{unknown}", "{Nat,Bool}"),
    "AMBIGUOUS_SUM_TYPE": program("let x=inl(0) in 0"),
    "AMBIGUOUS_VARIANT_TYPE": program("let x=<|x=0|> in 0"),
    "AMBIGUOUS_LIST": program("let x=[] in 0"),
    "ILLEGAL_EMPTY_MATCHING": program("match n {}", argument="Nat+Bool"),
    "NONEXHAUSTIVE_MATCH_PATTERNS": program("match n { inl(x)=>x }", argument="Nat+Bool"),
    "UNEXPECTED_PATTERN_FOR_TYPE": program("match n { inl(x)=>0 }"),
    "DUPLICATE_RECORD_FIELDS": program("{x=0,x=0}", "{x:Nat}"),
    "DUPLICATE_RECORD_TYPE_FIELDS": program("0", argument="{x:Nat,x:Bool}"),
    "DUPLICATE_VARIANT_TYPE_FIELDS": program("0", argument="<|x:Nat,x:Bool|>"),
}
valid = [
    program("if Nat::iszero(n) then 0 else succ(Nat::pred(n))"),
    program("unit", "Unit"),
    program("fn(x:Nat) { return succ(x) }", "fn(Nat)->Nat"),
    program("let f=fn(x:Nat) { return succ(x) } in f(n)"),
    program("{n,true}.1"),
    program("{y=true,x=n}", "{x:Nat,y:Bool}"),
    program("({x=n}).x"),
    program("inl(n)", "Nat+Bool"),
    program("inr(true)", "Nat+Bool"),
    program("match n { inl(x)=>x | inr(y)=>0 }", argument="Nat+Bool"),
    program("<|x=n|>", "<|x:Nat,y:Bool|>"),
    program("match n { <|x=a|>=>a | <|y=b|>=>0 }", argument="<|x:Nat,y:Bool|>"),
    program("[]", "[Nat]"),
    program("cons(n,[])", "[Nat]"),
    program("List::head([n,0])"),
    program("List::tail([n,0])", "[Nat]"),
    program("List::isempty([n])", "Bool"),
    program("Nat::rec(n,0,fn(i:Nat) { return fn(a:Nat) { return succ(a) } })"),
    program("fix(fn(x:Nat) { return x })"),
    program("(inl(n) as Nat+Bool)", "Nat+Bool"),
    "language core; extend with #lists, #sum-types; fn main(n:Nat)->[Nat] { return [n] }",
    "language core; fn loop(n:Nat)->Nat { return loop(n) } fn main(n:Nat)->Nat { return loop(n) }",
]
for code, source in errors.items():
    r = subprocess.run([binary], input=source, text=True, capture_output=True)
    assert r.returncode != 0 and r.stderr.startswith("ERROR_" + code + ":"), (code, r.returncode, r.stderr)
for source in valid:
    r = subprocess.run([binary], input=source, text=True, capture_output=True)
    assert r.returncode == 0 and not r.stderr and not r.stdout, (source, r.returncode, r.stderr)
print(f"Passed {len(errors)} error cases and {len(valid)} valid programs")
