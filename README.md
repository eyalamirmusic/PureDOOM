![](images/PureDOOM.png)

# DOOM on eacp

DOOM, ported to [eacp](https://github.com/eyalamirmusic/eacp)'s application, GPU
and input stack — and, increasingly, rewritten.

This began as a fork of [Daivuk/PureDOOM](https://github.com/Daivuk/PureDOOM),
the single-header DOOM source port. It no longer tracks upstream, and it is no
longer a single header: the engine is a library (`src/DOOM`, built as
`doom-engine`), this repository owns it, and it is being refactored into modern
C++ — physics and game logic included. See [REFACTOR.md](REFACTOR.md) for the
plan and where it has got to.

## What is here

- **A GPU renderer.** The level is drawn as real hardware 3D at the window's
  resolution rather than at 320x200 — walls, flats, sprites, sky, weapon, and an
  automap drawn as geometry. The shading is DOOM's own, not an imitation: the
  texture yields a palette index, the COLORMAP row chosen by sector light and
  distance remaps it, and the palette resolves the colour. Light banding,
  diminishing, fullbright frames and palette flashes all come out exact. The
  software renderer is still there behind **Shift+F8** and still draws the
  status bar.
- **The simulation, held still.** DOOM is exactly reproducible — fixed-point
  arithmetic and a 256-byte random table walked by an index — so a recorded demo
  *is* an assertion. The suite replays the shareware WAD's three attract-mode
  demos, 11,410 tics of real play, and hashes the world after every tic and the
  rendered frame every fourth. That is what makes rewriting the engine tractable
  at all.
- **No audio yet.** The engine produces it; nothing plays it.

## Build

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target PureDoomEACP

./build/examples/EACP/PureDoomEACP.app/Contents/MacOS/PureDoomEACP
```

The GPU paths currently need eacp features that only exist on a local eacp
branch; until they merge, configure with `-DCPM_eacp_SOURCE=$HOME/Code/eacp`.

The engine needs no GPU and no eacp, so `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF`
builds it and the tests alone.

## Test

```bash
ctest --test-dir build --output-on-failure
```

About two seconds. Run them through ctest rather than bare: the engine is several
hundred globals and a zone allocator that `doom_init` does not undo, so each test
needs a process of its own — which is exactly the thing the refactor is going to
fix.

## Using the engine

Link `doom-engine` and `#include <DOOM/DOOM.h>`. Call `doom_init` once,
`doom_update` every frame, hand it input as it arrives, and take the framebuffer
from `doom_get_framebuffer`:

```c
doom_init(argc, argv, 0);

while (running)
{
    doom_update();
    const unsigned char* rgba = doom_get_framebuffer(4);
    // ... display it
}
```

The engine reaches the outside world through function pointers — printing,
allocation, file I/O, time, exit, getenv — each with a default implementation
gated behind a `DOOM_IMPLEMENT_*` define, and each replaceable with
`doom_set_print`, `doom_set_malloc`, `doom_set_file_io` and friends. That is how
the test suite boots it headless and stops `I_Error` from taking the process
down.

`doom_set_default_int` sets a key binding *before* the config file is read, which
means a `~/.doomrc` left by an older build silently wins. `examples/EACP` applies
its bindings again after `doom_init` for exactly that reason.

There is no `-iwad` argument: WADs are found through `DOOMWADDIR`, falling back
to the working directory. Other classic arguments (`-warp`, `-skill`, `-episode`,
`-config`) pass straight through.

## Where things are

- `src/DOOM/` — the engine. The code we own and are rewriting.
- `Tests/` — the demo, frame, WAD and primitive tests, and their goldens.
- `examples/EACP/` — the port: the eacp platform layer and the GPU renderer.
- `CLAUDE.md` — how all of it actually works, at length.
- `REFACTOR.md` — the C++ refactor: the plan, the rules, and the progress.

# DOOM LICENSE
```
      LIMITED USE SOFTWARE LICENSE AGREEMENT

        This Limited Use Software License Agreement (the "Agreement")
is a legal agreement between you, the end-user, and Id Software, Inc.
("ID").  By downloading or purchasing the software material, which
includes source code (the "Source Code"), artwork data, music and
software tools (collectively, the "Software"), you are agreeing to
be bound by the terms of this Agreement.  If you do not agree to the
terms of this Agreement, promptly destroy the Software you may have
downloaded or copied.

ID SOFTWARE LICENSE

1.      Grant of License.  ID grants to you the right to use the
Software.  You have no ownership or proprietary rights in or to the 
Software, or the Trademark. For purposes of this section, "use" means 
loading the Software into RAM, as well as installation on a hard disk
or other storage device. The Software, together with any archive copy
thereof, shall be destroyed when no longer used in accordance with 
this Agreement, or when the right to use the Software is terminated.  
You agree that the Software will not be shipped, transferred or 
exported into any country in violation of the U.S. Export 
Administration Act (or any other law governing such matters) and that 
you will not utilize, in any other manner, the Software in violation 
of any applicable law.

2.      Permitted Uses.  For educational purposes only, you, the
end-user, may use portions of the Source Code, such as particular
routines, to develop your own software, but may not duplicate the
Source Code, except as noted in paragraph 4.  The limited right
referenced in the preceding sentence is hereinafter referred to as
"Educational Use."  By so exercising the Educational Use right you
shall not obtain any ownership, copyright, proprietary or other
interest in or to the Source Code, or any portion of the Source
Code.  You may dispose of your own software in your sole discretion.
With the exception of the Educational Use right, you may not
otherwise use the Software, or an portion of the Software, which
includes the Source Code, for commercial gain.

3.      Prohibited Uses:  Under no circumstances shall you, the
end-user, be permitted, allowed or authorized to commercially exploit
the Software. Neither you nor anyone at your direction shall do any
of the following acts with regard to the Software, or any portion
thereof:

        Rent;

        Sell;

        Lease;

        Offer on a pay-per-play basis;

        Distribute for money or any other consideration; or

        In any other manner and through any medium whatsoever
commercially exploit or use for any commercial purpose.

Notwithstanding the foregoing prohibitions, you may commercially
exploit the software you develop by exercising the Educational Use 
right, referenced in paragraph 2. hereinabove.

4.      Copyright.  The Software and all copyrights related thereto 
(including all characters and other images generated by the Software
or depicted in the Software) are owned by ID and is protected by
United States  copyright laws and international treaty provisions.  
Id shall retain exclusive ownership and copyright in and to the
Software and all portions of the Software and you shall have no 
ownership or other proprietary interest in such materials. You must
treat the Software like any other copyrighted material. You may not
otherwise reproduce, copy or disclose to others, in whole or in any
part, the Software.  You may not copy the written materials
accompanying the Software.  You agree to use your best efforts to
see that any user of the Software licensed hereunder complies with
this Agreement.

5.      NO WARRANTIES.  ID DISCLAIMS ALL WARRANTIES, BOTH EXPRESS
IMPLIED, INCLUDING BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE WITH RESPECT
TO THE SOFTWARE.  THIS LIMITED WARRANTY GIVES YOU SPECIFIC LEGAL
RIGHTS.  YOU MAY HAVE OTHER RIGHTS WHICH VARY FROM JURISDICTION TO
JURISDICTION.  ID DOES NOT WARRANT THAT THE OPERATION OF THE SOFTWARE
WILL BE UNINTERRUPTED, ERROR FREE OR MEET YOUR SPECIFIC REQUIREMENTS.
THE WARRANTY SET FORTH ABOVE IS IN LIEU OF ALL OTHER EXPRESS
WARRANTIES WHETHER ORAL OR WRITTEN.  THE AGENTS, EMPLOYEES, 
DISTRIBUTORS, AND DEALERS OF ID ARE NOT AUTHORIZED TO MAKE 
MODIFICATIONS TO THIS WARRANTY, OR ADDITIONAL WARRANTIES ON BEHALF
OF ID. 

        Exclusive Remedies.  The Software is being offered to you
free of any charge.  You agree that you have no remedy against ID, its
affiliates, contractors, suppliers, and agents for loss or damage 
caused by any defect or failure in the Software regardless of the form
of action, whether in contract, tort, includinegligence, strict
liability or otherwise, with regard to the Software.  This Agreement
shall be construed in accordance with and governed by the laws of the
State of Texas.  Copyright and other proprietary matters will be
governed by United States laws and international treaties.  IN ANY 
CASE, ID SHALL NOT BE LIABLE FOR LOSS OF DATA, LOSS OF PROFITS, LOST
SAVINGS, SPECIAL, INCIDENTAL, CONSEQUENTIAL, INDIRECT OR OTHER
SIMILAR DAMAGES ARISING FROM BREACH OF WARRANTY, BREACH OF CONTRACT,
NEGLIGENCE, OR OTHER LEGAL THEORY EVEN IF ID OR ITS AGENT HAS BEEN
ADVISED OF THE POSSIBILITY OF SUCH DAMAGES, OR FOR ANY CLAIM BY ANY
OTHER PARTY. Some jurisdictions do not allow the exclusion or
limitation of incidental or consequential damages, so the above
limitation or exclusion may not apply to you.
```
