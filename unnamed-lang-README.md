# unnamed-lang (`unnc`)

A small compiler written from scratch in C. It reads a program in a tiny
C-like language and generates **ARM64 assembly for Apple Silicon Macs**
(developed and run on an M2 MacBook). There are no parser generators or
compiler frameworks: the lexer, parser and code generator are all hand-written,
about 1,200 lines of C.

```
source (.unn) ──► Lexer ──► tokens ──► Parser ──► AST ──► Code generator ──► ARM64 assembly (.s)
```

## Example

`test.unn`:

```c
int n = 5;
int total = 0;

while (n > 0) {
    total = total + n;
    n = n - 1;
}

write(total);
```

Build the compiler, compile the program, then assemble and run it:

```bash
./build.sh                       # builds the compiler as ./main
./main                           # reads test.unn, writes output.s
clang output.s -o program        # assemble + link with Apple's toolchain
./program                        # prints 15
```

The generated assembly (excerpt):

```asm
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movz x0, #5                  // int n = 5
    str x0, [sp, #-16]!
    ...
Lwhile_0:
    ...
    cmp x1, x0
    cset x0, gt                  // n > 0
    cbz x0, Lendwhile_0
    ...
    b Lwhile_0
```

## The language

| Feature | Details |
| --- | --- |
| Variables | `int x = expr;` (64-bit signed). Assign with `x = expr;` |
| Arithmetic | `+ - * / %` and unary `-`, with normal precedence and parentheses |
| Comparisons | `== != < > <= >=` (give `1` or `0`) |
| Control flow | `if (...) { } else if (...) { } else { }` and `while (...) { }` |
| Blocks and scope | `{ ... }` blocks with their own scope; an inner block can shadow an outer variable |
| Output | `write(expr);` prints an integer followed by a newline |
| Exit | `exit(expr);` ends the program with that exit code |
| Comments | `// to end of line` |

Not supported yet: **user-defined functions**, types other than `int`,
strings, arrays and pointers. The whole program is a list of top-level
statements that the compiler wraps in `_main`.

## Compile-time errors

The compiler stops with a line number when it finds a problem:

```
Parse error (line 1): expected expression, but found ;
Error (line 2): variable already declared in this scope 'x'
Error (line 1): assignment to undeclared variable 'y'
Error (line 5): undeclared variable 'a'      // using a variable outside its block
```

## How it works

- **Lexer (`lexer.c`)** turns the source text into a list of tokens with line
  numbers, skipping whitespace and `//` comments.
- **Parser (`parser.c`)** is a hand-written recursive-descent parser. One
  function per precedence level (comparison → additive → term → factor) keeps
  operator precedence correct. It builds an AST and reports syntax errors with
  the line and what it expected.
- **Code generator (`codegen.c`)** walks the AST once and emits ARM64.
  Every expression leaves its result in `x0`. For a binary operation it
  evaluates the left side, pushes it on the stack, evaluates the right side,
  pops the left into `x1`, then emits the operation.

### Design decisions

- **Stack-based variables.** Each variable lives in a 16-byte stack slot,
  addressed relative to the frame pointer (`x29`). ARM64 requires the stack
  pointer to stay 16-byte aligned, so every push and pop moves it by 16.
- **Scopes at compile time.** The generator keeps a table of variables and
  where each scope started. Leaving a block drops its variables and releases
  their stack space, which is how shadowing and "variable not in scope"
  errors work with no runtime cost.
- **Printing through `printf`.** On Apple Silicon, variadic arguments such as
  the number passed to `printf` go on the **stack**, not in registers, so
  `write` stores the value at `[sp]` before calling `_printf`.
- **Comparisons with `cset`.** Comparisons produce `0` or `1` in `x0`, so
  `if` and `while` can share one branch instruction (`cbz`).
- **Modulo** is computed as `a - (a / b) * b` using `sdiv` and `msub`, because
  ARM64 has no remainder instruction.

## Project layout

```
main.c        reads test.unn, runs the pipeline, writes output.s
lexer.c/.h    tokens and the lexer
parser.c/.h   AST definition and recursive-descent parser
codegen.c/.h  AST → ARM64 assembly
build.sh      gcc build script
test.unn      the input program the compiler reads
```

## Limitations

- Apple Silicon (ARM64 macOS) only. The generated assembly uses Apple's
  conventions and won't assemble on Linux.
- No user-defined functions yet.
- The compiler always reads a file named `test.unn` and writes `output.s`;
  it doesn't take file names on the command line yet.
- Division and modulo by zero aren't detected.
- There is no automated test suite. `test.unn` is a single input, so right now it
  is checked by hand.
- Limits of 255 live variables and 256 nested blocks.

## Roadmap

- Functions with parameters and return values (the next big step).
- Take the input and output file names as command-line arguments.
- An automated test script that compiles sample programs and checks their output.
- Divide-by-zero checks and more helpful error messages.
