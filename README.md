# WarGames 2026 Edition

> **What if WOPR had actually survived into 2026?**

WarGames 2026 Edition is a modern reworking of Andy Glenn's **WarGames
Movie Simulator**, originally written in SuperBASIC on a Sinclair QL,
later ported to BASIC-80 for CP/M, and subsequently rewritten in C.

The project recreates the IMSAI 8080, WOPR/Joshua, dialler, school
computer, airline and other systems inspired by the 1983 film
*WarGames*, while adding a new 2026 layer: **Joshua is now backed by a
real local large language model, has persistent memory, and can hold
free-form conversations.**

The original deterministic simulation is still there. The AI does not
replace it. Instead, the 2026 edition places a modern conversational
Joshua behind the familiar WOPR interface.

The result is deliberately somewhere between a retro-computing project,
a movie simulator, and an experiment in giving a fictional computer
system a little continuity of its own.

## What's New in the 2026 Edition

The major change is the new **WOPR Chat** subsystem.

When logged into WOPR as `JOSHUA`, the normal movie simulation can be
temporarily suspended with:

``` text
chat
```

This enters a free-form conversation with an AI-powered Joshua. Type:

``` text
resume
```

to leave AI chat and return to the deterministic WarGames simulation.

### AI-powered Joshua

Joshua is no longer dependent on the old Shell GPT integration. The 2026
edition includes a Python WOPR Chat service which talks directly to an
OpenAI-compatible LLM endpoint.

The current default configuration uses a small local **Gemma 4 E4B**
GGUF model through `llama.cpp`. This is intentional: the aim is to make
Joshua practical on modest modern hardware while keeping the LLM backend
configurable.

The model is only one part of Joshua. His behaviour is shaped by a
WOPR/Joshua system profile designed to make him:

-   aware that he is a modern AI system inhabiting the role and heritage
    of WOPR/Joshua;
-   recognisably systems-focused, analytical and slightly formal;
-   WarGames-aware without being trapped in constant movie quotation or
    role-play;
-   able to discuss ordinary subjects outside the simulation;
-   capable of some dry humour without losing the character;
-   cautious about making unsupported claims about consciousness or
    subjective experience.

The result is intentionally **Joshua**, rather than a generic chatbot
wearing a WOPR prompt.

### Persistent Memory

WOPR Chat includes a lightweight persistent-memory system.

Recent exchanges are periodically consolidated by the LLM into a small
text-based memory file. On future turns, those memories are included in
Joshua's context, allowing him to retain useful facts and fragments of
conversational history between sessions.

This is deliberately a simple implementation rather than a full RAG or
database-backed memory architecture. It is small, inspectable, easy to
experiment with, and surprisingly effective for giving Joshua basic
continuity.

Memory and diagnostic log files are runtime data and should normally be
excluded from source control.

### Voice

Joshua can speak his AI-generated responses using the host system's
text-to-speech facilities. The current macOS configuration uses the
**Evan (Enhanced)** system voice, which suits the character rather well.

Speech is asynchronous: Joshua does not need to finish speaking before
the terminal becomes available again.

The original simulator's movie samples and sound effects remain separate
from the AI TTS system.

### Configurable LLM Backend

WOPR Chat reads its model configuration from `wopr-llm.json`.

The current local configuration uses `llama.cpp`, but the service is
designed around an OpenAI-compatible `/v1/chat/completions` API, so
other compatible local or remote inference servers can be used.

For Gemma 4, the local `llama-server` should be run with reasoning
disabled:

``` text
--reasoning off
--reasoning-budget 0
```

This keeps Joshua responsive and prevents reasoning tokens consuming the
relatively small output budget used by the memory-consolidation process.

Do **not** commit API keys or other credentials to the repository.

## Architecture

The 2026 edition deliberately keeps the classic simulator and the AI
system separate.

``` text
IMSAI 8080 / Dialler
        |
        v
     WOPR (C)
        |
        +---- deterministic WarGames simulation
        |
        +---- CHAT
                |
                v
        WOPR Chat bridge (Python)
                |
                +---- Joshua system profile
                +---- persistent text memory
                +---- TTS
                |
                v
        OpenAI-compatible LLM
        (llama.cpp / configurable backend)
```

This means the movie sequence does not depend on an LLM behaving
correctly. Global Thermonuclear War, Tic-Tac-Toe, login sequences and
the other scripted systems remain under deterministic program control.

The LLM is used when you deliberately step outside that simulation and
talk to Joshua.

## Project History

The simulator began as a retro-computing project.

The first version was developed on a **Sinclair QL using SuperBASIC**.
It was then rewritten in **BASIC-80 on CP/M** so that it could run on an
IMSAI 8080esp, and later rewritten in **C on Linux** to remove the
limitations of CP/M's 64 KB environment and make richer integrations
possible.

The repository retains the historical BASIC material as well as the
later C implementation.

The C version expanded the project considerably, adding simulated remote
systems, modem/dialler behaviour, audio, speech, Tic-Tac-Toe logic, user
accounts, and---in an earlier generation---ChatGPT integration via Shell
GPT.

WarGames 2026 replaces that older conversational integration with the
dedicated WOPR Chat service and a locally runnable, persistent Joshua.

## Simulated Systems and Features

The exact contents of the project continue to evolve, but the C
simulator includes the core WarGames environment and several supporting
systems.

### IMSAI 8080

-   CP/M 2.2-style environment
-   Kermit and dialler programs
-   configurable drive commands
-   DTMF dialling
-   modem and terminal audio
-   support for retro-terminal presentation
-   ability to use real retro hardware as a terminal

### Dialler

-   configurable systems list
-   WOPR and other simulated destinations
-   modem/DTMF audio
-   scan and view modes
-   engaged/unavailable-system handling
-   configurable area code
-   ability to add external systems

Some configurations may also contain entries for real external
retro-computing services. Review any connection scripts before using
them.

### WOPR / Joshua

-   pre-logon WOPR commands
-   Joshua backdoor/login sequence
-   deterministic movie dialogue and sequencing
-   Global Thermonuclear War simulation
-   primary-target and trajectory handling
-   Tic-Tac-Toe sequence
-   date/time and help functions
-   user accounts and access levels
-   simulated WOPR mail and network functions
-   movie audio samples
-   AI-powered free-form `CHAT`
-   persistent Joshua memory
-   configurable LLM backend
-   text-to-speech for AI Joshua responses

### Other Systems

The project also contains lightweight simulations including:

-   School Computer
-   Bank
-   Pan Am
-   Tic-Tac-Toe
-   configurable dialler destinations

The Tic-Tac-Toe program includes 0-, 1- and 2-player modes; in 0-player
mode the computer plays itself as part of the familiar WarGames
sequence.

## BASIC-80 / CP/M Version

The historical BASIC-80 version remains part of the project.

To run it on a CP/M system you will need a suitable 64K CP/M
environment. A disk image is included for use with compatible
systems/emulators such as Z80PACK or the IMSAI 8080esp.

From CP/M:

``` text
MBASIC WARGAMES
```

The BASIC version predates many of the features in the modern C/2026
implementation but is retained both for historical interest and because
it can run on period-style hardware.

## Running on Retro Hardware

One of the original aims of the project was to let a real or replica
retro computer act as the terminal while the heavier simulation runs
elsewhere.

For example, an IMSAI 8080esp can connect through its Wi-Fi modem/Kermit
to a Linux host running the WarGames software. The retro machine
therefore behaves much like the terminal in the film, while the
simulated remote systems live on another computer.

Historically this used Telnet because of the limitations of some retro
Wi-Fi modem implementations.

**Telnet is unencrypted.** Do not use it for sensitive credentials or
data, and review any external connection scripts before exposing
services beyond a trusted local network.

## Building and Running

The original C code was developed primarily for Debian Linux. The 2026
edition has also been developed and tested with modern macOS components
for the AI/TTS side.

Because this project spans retro software, C programs, shell scripts,
audio, terminal software and a modern LLM service, installation is
currently still somewhat hands-on.

At minimum, expect to need:

-   a C compiler;
-   Git;
-   Python 3 for WOPR Chat;
-   `llama.cpp` or another compatible inference endpoint if using AI
    chat;
-   a compatible GGUF model for local inference;
-   any audio/terminal dependencies required by your chosen platform.

Older Debian-oriented installation scripts are included in the
repository and may need adjustment for your operating system.

### Typical C build / launch

The exact scripts in the repository should be treated as the source of
truth for the current build, but the traditional local launch path is
based around the IMSAI/WOPR shell scripts supplied with the project.

For older Debian/Linux installations:

``` bash
git clone <repository-url>
cd wargames-2026
chmod +x *.sh
./install.sh
```

Then launch the IMSAI simulation using the appropriate script for your
installation.

The repository is currently a work in progress. Review paths and
configuration files before running installation or startup scripts on a
new system.

## WOPR Chat Configuration

WOPR Chat uses files in the `wopr_chat` area of the project.

A typical setup contains:

``` text
wopr_chat.py
wopr-system.txt
wopr-llm.json
```

The LLM configuration specifies the provider, endpoint, model, context
size and local `llama.cpp` settings.

The current development configuration uses approximately:

``` text
Provider:       local_llamacpp
Model:          gemma-4-E4B-it-UD-Q4_K_XL
Context:        32768
Max response:   1024 tokens
```

Model binaries are not intended to be stored in this repository.
Download an appropriate GGUF separately and update `wopr-llm.json` to
point to its location.

If using an API-backed provider, keep credentials outside source
control.

## Client/Server and Telnet

The older C implementation supports running simulated systems behind TCP
listeners so that another terminal---including retro hardware---can
connect to them.

Historically, individual systems used separate ports and `telnetd`, for
example:

-   School Computer
-   Bank
-   Pan Am
-   WOPR
-   IMSAI terminal service

Those scripts are retained where useful, but this mode should be
considered an advanced/legacy configuration rather than the primary 2026
installation path.

If you use Telnet, remember that traffic is not encrypted.

## External Systems

Some versions of the dialler configuration have included connections to
external retro-computing services such as Telehack.

These are real external systems, not part of WarGames 2026. Check the
relevant service's current connection requirements and terms before
enabling an external dialler entry.

Where possible, prefer encrypted protocols such as SSH over Telnet.

## Fonts and Terminal Presentation

For the full retro effect, the project can be used with a suitable
terminal emulator such as `cool-retro-term`.

WarGames-style fonts by Michael Walden are available separately from the
author's WarGames terminal/font project.

The simulator itself does not require a particular visual presentation,
so a normal terminal is perfectly adequate.

## Repository Notes

This repository is intended to preserve both the historical simulator
and the evolving 2026 edition.

In particular, source control should contain the things needed to
reconstruct the project:

-   C and BASIC source code
-   Python source
-   shell scripts
-   Joshua's system/profile prompt
-   example configuration
-   required small data files
-   documentation

It should generally **not** contain:

-   GGUF/model files
-   API keys
-   runtime memory
-   diagnostic logs
-   generated caches
-   machine-specific temporary files
-   unnecessary compiled binaries

## Status

**WarGames 2026 Edition is a work in progress.**

The simulator has grown organically over several years and includes code
from several generations of the project. Some scripts and documentation
may therefore still reflect older Linux/CP/M configurations while the
2026 AI integration is being consolidated.

The immediate goal is to preserve a working, rebuildable source tree and
then refine the clean-install process.

Expect bugs. Expect strange terminal behaviour. Expect Joshua to be
stubborn.

That last one may be a feature.

## Credits

WarGames Movie Simulator / WarGames 2026 Edition by **Andy Glenn**.

The project was inspired by the computer systems and interfaces depicted
in the 1983 film *WarGames*.

Thanks to the retro-computing projects and communities that made the
original simulator possible, including:

-   Udo Munk / Z80PACK
-   The High Nibble / IMSAI 8080esp
-   Simon Jansen / Star Wars ASCIIMATION
-   Michael Walden / WarGames terminal fonts

See the original project documentation and source files for further
acknowledgements.

## Licence / Use

You are free to use and modify the code for your own purposes subject to
the licence and attribution terms included with this repository.

Please credit **Andy Glenn** as the original author of the WarGames
Movie Simulator.

------------------------------------------------------------------------

*Would you like to play a game?*
