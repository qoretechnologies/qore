# Callgrind rows

The callgrind "rows" are the instruction counts per loop iteration that accept or reject a performance change: each
row is one loop of a bench script in `bench/`, run in one execution mode, and its value is the number of instructions
(callgrind `Ir`) that one iteration of the loop takes.  The counts do not depend on the load of the host, so a row
measures a change of a few instructions per iteration that no timing can show.

- `cgmin.sh` measures one row of one build: the minimum over 5 allocation layouts.
- `rows.sh` measures a list of rows of a build, or of a build and a base build with the delta.
- `bench/*.q` are the bench scripts.

The tool was kept outside the repository until qore#7193 (as `cgmin2.sh` and the same bench scripts); its row names,
scripts and output format are unchanged.  Its values are the same except where the old layouts were sensitive to the
addresses of early strings (see [Layouts](#layouts)), and except that a value is now rounded instead of truncated, so
that a jit value such as 3489.98 is printed as 3490 where the old tool printed 3489 (see [Method](#method)).

## Usage

```sh
# one row: the instructions per iteration of the loop "fc" of bench-func.q in ir mode, the minimum of 5 layouts
tools/perf/cgrows/cgmin.sh build tools/perf/cgrows/bench/bench-func.q fc ir
# the per-layout values on stderr, with two decimals
CGROW_VERBOSE=1 CGROW_DECIMALS=2 tools/perf/cgrows/cgmin.sh build tools/perf/cgrows/bench/bench-member.q mr ast

# the standard rows of a build in ast, ir and jit
tools/perf/cgrows/rows.sh ~/tmp/qa/build
# every row of the list below, of a build and a base build, with the deltas, 8 cells at a time in count slots
ROWS=all P=8 CGROW_WRAP="qbuild --count --" tools/perf/cgrows/rows.sh ~/tmp/qb/build ~/tmp/qa/build
# chosen rows and modes
ROWS="bench-member.q:mr bench-member.q:pi" MODES="ir jit" tools/perf/cgrows/rows.sh ~/tmp/qb/build ~/tmp/qa/build
```

`cgmin.sh <build> <script> <row> <mode>` prints one number.  `<build>` is a build directory with `qore` and `libqore`
(and `qcc` for the aot modes); `<mode>` is `ast`, `ir`, `jit` or `tiered` (`qore --exec-mode`), or `aot0` .. `aot3`
(an executable that the build's `qcc -O<n>` built from each layout of the script).

`rows.sh <build> [<base build>]` prints `<script>:<row> <mode> <value>` for one build, and
`<script>:<row> <mode> <base value> <value> <delta>` for two, one line per row and mode in the order of `ROWS` and
`MODES`; a cell that fails prints `-` and its error, and `rows.sh` then exits with 1.

Run the tool on a host where valgrind runs the build (Linux; valgrind 3.25 or later), with `ulimit -c 0`, and on the
build hosts in their count slots (`qbuild --count`), which run work whose results do not depend on the load.

## Method

A row's value is computed from two runs of the script under `valgrind --tool=callgrind`: one with `N=2000` iterations
of the loop and one with `N=22000`.  The difference of their total instruction counts over 20000 is the count of one
iteration: parsing, startup, compilation (`QORE_JIT_SYNC_COMPILE=1` makes the jit mode compile before the loop runs)
and exit cancel out.  The value is printed rounded to an integer (`CGROW_DECIMALS` for decimals).  The old tool truncated it; the
values of the jit mode differ from an integer by a few hundredths between layouts and builds (ex: 825.99 and 826.00),
which truncation printed as a difference of 1.  The values that are integers are the same as before; awk rounds a
value of exactly .5 to the even integer (ex: 4191.5 to 4192, 4190.5 to 4190).

The two runs get `N` as numbers of the same width (`N=02000` and `N=22000`; `ENV.N.toInt()` reads `02000` as 2000), so
that their environments have the same size, as the environment's size moves the heap (see [Layouts](#layouts)).

### Layouts

The interpreter's costs depend on the addresses of its objects: its pointer-keyed hash tables collide or not, and
glibc's `strcmp()` (`__strcmp_avx2`) takes a slower path when the bitwise OR of the offsets in their pages of the two
strings it compares is within 128 bytes of the end of a page, which is often the case for two strings that are not
(each of bits 7 to 11 of the OR is set in 3 of 4 address pairs).  One build can thus measure some instructions more or
less than another for the same code, only because a change elsewhere moved its allocations, and two hosts (whose
environments, and so heap layouts, differ) can measure one build differently.  To measure the code, not the layout,
the tool runs each row in 5 layouts and takes the minimum: the cheapest layout is the one without such penalties.

Layout `k` (0 .. 4) changes the run in four ways:

- the **environment pad**: the runs get the variable `CGROW_ENV_PAD` of `k * 816` characters (empty in layout 0); the
  environment is copied to the heap at startup, so this moves the allocations made before the script is parsed.  The
  rest of the environment is the caller's, the same for every layout and for both builds of a comparison;
- the **script pad**: after the first line (`%modern`), before anything else of the script is parsed, a constant
  string of `k * 816` characters (`const CGROW_PAD = "ppp...";`), which moves the allocations of the script, the
  classes it declares first and their member names among them (816 bytes is about a fifth of a page);
- the **dead statements**: `k` statements `if (False) { f(<i>); }` after each `int t = 0;` (the start of each loop
  function), which change what the parse allocates between the objects of the loop's function;
- the **loop pad** (`k` > 0): one more dead statement there, `if (False) { f("lll...".size()); }` with a string of
  `k * 1232` characters, which moves the strings of the loop (ex: the member name in `o.x`) relative to those of the
  classes declared before it (the member name in the class), so that the two strings that `strcmp()` compares change
  their relative offsets too, not only their common one.

None of them changes what the loop executes: the pads are data that no code reads, and the dead statements are not
executed (the parse removes them, or they branch on a constant).  With `CGROW_PAD=0` there are no pads, and layout 0
is the script as it is: the layouts of the old tool, to compare with values measured before.

The old tool had only the dead statements, so the strings allocated before the loop functions, such as member names,
had the same addresses in all 5 layouts.  The size of the environment is enough to move them: with an extra
environment variable of 704 or 3968 bytes on one host, the old layouts measured every member row (`bench-member.q:mr`,
`mw`, `pr`, `pi`, in ast, ir and jit) 17 to 19 instructions higher for one build than for its base in all 5 layouts,
which is no change in the code (the member rows of batch AS measured on another host, qore#7193).  With the script
pad alone, `bench-member.q:mw` in ir still missed its minimum in all 5 layouts under some environments (by 2 to 11
instructions, all of them in `__strcmp_avx2`), because the two strings it compares moved together; with all four,
every member row measured the same minimum for both builds under each of 16 environment sizes from 0 to 3840 bytes.
The rows whose values do not depend on such addresses measure the same with and without the pads.  This validation
covered the ast, ir and jit modes; in the aot modes, each layout's pads and dead statements are compiled into its
executable (and the environment pad is in the executable's environment), but the aot modes' layouts have not been
validated (the validated trees had no qcc).

When a delta of a row in one mode decides something, check its per-layout values with `CGROW_VERBOSE=1`.

### Comparing two builds

- Build both trees with the same compiler, flags and build type (`-DCMAKE_BUILD_TYPE=Release`; the rows of the
  performance work also use `-DSINGLE_COMPILATION_UNIT=1 -DQORE_QCC_TEST_HOOKS=ON`), in the same environment.
- Put the trees at **paths of equal length** (ex: `~/tmp/q6126p0` and `~/tmp/q6126p1`): libqore embeds the paths of
  its source files (assertion and location strings), so a longer path shifts the library's string data, and with it
  the alignment of the strings that `strcmp()` and the hash functions read; trees at paths of different lengths can
  differ by a few instructions per row that come from no change in the code.  The build path also moves the heap:
  the runs' `LD_LIBRARY_PATH` (the build directory) is in the environment, which is copied to the heap (`ENV`), and
  so is the path of the `qore` binary (`argv[0]`, in `QORE_ARGV`), so a longer path shifts the early heap
  allocations too.
- Run both builds' rows on the same host, in the same session, with the same environment (`rows.sh` with a base
  build does so).
- A delta of a row is a change of the code only if it persists: a delta of a few instructions in one row and mode
  that the others do not show is checked with `CGROW_VERBOSE=1` (the per-layout values) before it is reported.

## Environment

`cgmin.sh`:

|Variable|Default|Meaning|
|---|---|---|
|`CGROW_LAYOUTS`|5|the number of layouts|
|`CGROW_PAD`|1|1: the pads grow with the layout; 0: no pads (the layouts of the old `cgmin2.sh`)|
|`CGROW_PAD_STEP`|816|the script pad's growth per layout, in characters|
|`CGROW_ENV_PAD_STEP`|816|the environment pad's growth per layout, in characters|
|`CGROW_LOOP_PAD_STEP`|1232|the loop pad's growth per layout, in characters (0: no loop pad)|
|`CGROW_N0`, `CGROW_N1`|2000, 22000|the iterations of the two runs|
|`CGROW_DECIMALS`|0|0: the value rounded to an integer; n: n decimals|
|`CGROW_VERBOSE`|unset|1: also print `layout <k> <value>` for each layout on stderr|
|`CGROW_VALGRIND`|`valgrind`|the valgrind command|
|`CGROW_KEEP`|unset|a directory to copy each layout's script to (`layout-<k>.q`)|

`rows.sh` (and those of `cgmin.sh`, which it passes on):

|Variable|Default|Meaning|
|---|---|---|
|`ROWS`|`standard`|the rows as `<script>:<row>` words, or `standard` (the standard rows) or `all` (the standard rows and those of the batch rows: `mse mmse pv pl pt mr mw pr pi` and `bench-cf.q`'s `cf cfa`)|
|`MODES`|`ast ir jit`|the modes|
|`P`|4|the number of cells run at a time|
|`CGROW_WRAP`|unset|a command prefix for each `cgmin.sh` run, ex: `qbuild --count --`|

## Rows

A row is named by its script and loop (`bench-func.q:fc`); names repeat between scripts (`bench-s.q:mr` is not
`bench-member.q:mr`).  The **standard rows**, which every performance change reports, are `bench.q:fs`,
`bench-func.q:fc`, `bench-func.q:sc`, `bench-method.q:m`, `bench-map.q:mp` and `bench-map.q:mm`, in ast, ir and jit.

| Script | Rows | Loops |
|---|---|---|
| `bench.q` | `fs` `fp` `ff` `cp` `cf` | self calls in a final class (`self.m(i)`, `m(i)`, `m(f(i))`) and in a class (`m(i)`, `m(f(i))`) |
| `bench-func.q` | `fc` `sc` `mf` | a function call, a static method call, `map` calling a function over 4 elements |
| `bench-method.q` | `m` `cr` `mse` `ma` | a method call, a call reference call, a method call with a call as argument, with an untyped local as argument |
| `bench-map.q` | `mp` `mm` `mmse` | `map` of an expression, of a method call, of a method call with a call as argument, over 4 elements |
| `bench-pseudo.q` | `pv` `pl` `pt` | pseudo-methods: without an intrinsic, `<string>::length()`, `<int>::typeCode()` |
| `bench-member.q` | `mr` `mw` `pr` `pi` | a public member read and write, a private member read in the class's method, a base class's private member read on a subclass object (qore#6760) |
| `bench-cf.q` | `cf` `cfa` | `call_function()` of a call reference, `call_function_args()` with one argument (qore#6052) |
| `bench-instanceof.q` | `io` `iom` | two, six `instanceof` checks of objects of as many classes; in aot modes a hit of the type cache of the AOT runtime (qore#7416) |
| `bench-enum.q` | `ok` `osz` `ofk` `oel` | an object's keys, `size()`, `firstKey()`, `elements` (qore#7048) |
| `bench-lvalue.q` | `rl` `rh` `dh` `ri` `la` | `remove` of a list element, of a hash key, `delete` of a hash key, `remove` of a large int local, int assignment |
| `bench-s.q` | `cv` `ci` `cs` `up` `um` `ua` `cc` `lp` `lr` `mp` `mr` `gp` `gr` `hp` `hr` | float to int conversions (qore#6181), unary plus and minus (qore#5712, qore#6132), a class cast, post and pre increment of a local, member, global and hash key (qore#6199) |
| `bench-hasheq.q` | `hes` `her` `hsw` `hfv` `hfk` `hhe` `hhr` | `==` of two equal 26-key hashes with the keys in the same order, in reverse order, with the first two keys swapped; with the first value different, the first key different; `===` with the keys in the same order, in reverse order (qore#7266) |
| `bench-gen.q` | `gr` `gc` `cr` `cc` `gn` `gl` `gh` `gk` `gm` `gx` `gv` | generic code at runtime (qore#6126): generic locals, closures, calls, classes, casts, with concrete controls (`cr`, `cc`) |
| `bench-ref.q` | `rf` `ru` `rw` `rh` `rm` `rs` | calls with reference parameters (qore#7271): a function with a `reference<int>` parameter, an untyped `reference` parameter, one that assigns through its `reference<int>` parameter, one with a `reference<hash<auto>>` parameter, a method and a static method with a `reference<int>` parameter |

A new bench script follows the same form, which `examples/test/runner/test_cgrows_tool.py` checks: its first line is
`%modern`; it defines `int sub f(int a)` (which the dead statements call); each loop function starts with
`int t = 0;` (at most once per line); it runs the loop that `ENV.L` names `ENV.N.toInt()` times (`switch (ENV.L)` with
a `case` per row); and its rows are added to the list above.  An existing script is never changed, so that its rows'
values stay comparable with those measured before; a changed loop is a new row.
