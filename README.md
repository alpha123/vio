What is Vio?
==========

	"vio is " { "fun" "expressive" "concise" "a work-in-progress" } ++

	inspiration: "J Perl6 Factor Processing" <s> split

Vio is an experimental new programming language designed to be fun to use. Its goal is to be quick to try out and to have easy ways to visualize your program, rather than minimal or flexible. In the long run, it's aimed at new programmers, particularly ones *learning to program on mobile devices*. But for now, it's for hard-core nerds.

***At the moment, Vio is very new. This document is as much of a design document as it is actual documentation.***

Features:

- Many useful built-in functions
- A nifty REPL web application
- Ability to load/save VM images (snapshots of the virtual machine state)
- Simple implementation (~2k semicolons)
- Not terribly slow
- Few dependencies

Getting Vio
----------------

Dependencies:

- A more-or-less POSIX-compliant OS, probably works under Cygwin also
- The GNU Multiprecision Library (GMP)
- OpenBLAS
- GNU Make
- A semi-recent GCC or Clang (Vio uses computed goto statements, so it's not strictly C11-compliant)

The rest of Vio's dependencies are bundled with the source (they're mostly portable single C files).

Once GMP and OpenBLAS are installed, clone and build:

	git clone https://github.com/alpha123/vio.git
	cd vio/src && make

Vio is known to build on FreeBSD, but should build fine on anything vaguely *nix-y.

***Why is GCC5 hard-coded in the Makefile?***

I have trouble with my build environment on FreeBSD. I'll fix it eventually.

What does Vio code look like?
------------------------------------------

Only the core VM and bytecode compiler are implemented; these examples may or may not work.

	-- Entirely tacit, no formal parameters ever
	square: dup *
	4 square		-- 16
	
	-- Vectors are the central data type
	factorial: iota 1 + 1 \*
	5 factorial		-- 120
	
	-- Higher-order functions
	mean: [0 \+] &len bi /
	{ 5 9 4 8 3 } mean		-- 5.8

	-- Lots of weird sigils
	qsort: [&small-vec? &all-eq? bi or] preserve [&median preserve ~/'[_ cmp] #qsort {} \vcat] unless
	-- Or just the built-in sort function, but whatever.

Other stuff:

	-- Parse infix calculator input
	maybe-op: over dup &one-of dip2 , , |
	parens: `(` swap ,`)` ,

	expr: <factor> "+-" maybe-op
	factor: <term> "*/" maybe-op
	term: <int> <expr> parens |
	int: <d> +

	math: <expr> parse
	"4 + 2 * 2" math		-- .expr{.int{"4"} "+" .factor{.int{"2"} "*" .int{"2"}}}

	-- Find prime numbers inefficiently but concisely
	primes-upto: 1 - iota 2 + $[dup *o] member? not ~id
	6 primes-upto		-- { 2 3 5 }
	
	-- *o is outer product, ~id is filter by identity function
	-- the rest is left as an exercise to the reader

On the horizon:

	-- Immediate-mode GUI
	.xywh{10 10 30 12} "Click me!" button [.xy{50 10} "Clicked" label] when

	-- Data visualization and whatever
	11 iota 5 - &tanh graph
	
	-- It's meant to be a visual language, sort of like Processing except entirely different.

Data Types
---------------

Strings  
`"spam"`

Integers  
`42`
	
Single-precision floats  
Unlike most languages, you don't use floats for decimals most of the time. They are only for writing performant code, since Vio can exploit the processor's vector units.  
`5.8f`

Arbitrary-precision floats  
`3.22`

Tagwords  
These are kind of like Ruby's symbols or Erlang's atoms, except they can have values attached (sort of named vectors).
```
.foo
.bar{20}
```
You could pattern match on them, if I'd implemented that.

Vectors of all of the above  
Row-major by default (todo: reconsider that?)  
Can contain any of the above types, and may be heterogeneous. Homogeneous float vectors are optimized to *GO FAST*.  
`{7 20 433}`

Matrices  
idk how these work yet, but eventually you'll be able to do useful vector-matrix and matrix-matrix things  
`{{ 1 2 3 ; 2 4 6 ; 3 6 9 }}`

Quotations  
Basically lambdas but without formal parameters.  
`[ 2 + ]`

Parsers  
Yeah parsers are their own type of value. It's about as weird as it sounds, but you can do cool stuff.
```
`foo`
digits: <d> +
float: <digits> ,`.` ,<digits>
```
