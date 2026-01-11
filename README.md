# gbtty

gbtty (pronounced: _gib-bit-EE_ / "gibbity") is a Game Boy (Color) hardware and
software project to turn the Game Boy into an ANSI-compatible terminal.

# Goals

The primary goal of this project is _funsies_. I wanted to work on a side
project where I could use Rust, play around with an RP2040, and do something
related to my current hyperfixation: the Game Boy.

Secondary goals include providing a useful-enough implementation of an ANSI
terminal, at least to be VT100-compatible to the extent that matters for modern
purposes.

Some stretch goals include supporting a broad set of ANSI escape codes, such as
text/background color on the Game Boy Color, some support for scrollback, and
perhaps even some support for some TUI elements depending on how wild I decide
to get with this.

## Anti-goals

I am not planning on turning this into an interactive terminal accepting input
to be passed back to the host. The only possible way to make this work would be
using some sort of on-screen keyboard control, and I am simply not masochistic
enough to use that.

# License

The firmware of the link adapter (contents of the root & `src/` directory) is
licensed under either the [Apache License Version 2.0](LICENSE-APACHE-2-0) or
[The MIT License](LICENSE-MIT)

The PCB (contents of `pcb/`) is licensed under
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-nc-sa/4.0/). **Commercial uses are strictly prohibited.**

The terminal emulator ROM itself (contents of the `gb/` directory) is derived
from orangeglo's Better Button Test, which is licensed under the GNU General
Public License Version 3, so the same license applies.

# Disclaimer

Game Boy™ is a trademark of Nintendo of America Inc.
