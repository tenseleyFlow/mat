# mat
cat/bat, but blazingly fast.

## Why?
My name is Matt. Would be nice to have a program named after me.

## Requirements
Parity with `cat`. Niceties of `bat`. Blazingly fast. The fastest of the three.

### Innovate, don't just mimic
cat and bat are not perfect. A core goal is to actively hunt for areas where we can
*improve* on them, not merely reproduce them — with **SPEED as the primary driver**.
Where the references leave performance on the table (per-byte scanning, fixed buffers,
missed zero-copy paths, heavy startup, single-threaded execution, paying for prettiness
when piped), mat should do better. See `.docs/audits/04-innovation-and-speed.md` for the
running catalogue of concrete opportunities; every optimization ships with a
before/after benchmark so "blazingly fast" is provable, not vibes.

## Stages
- Planning
    - whereby we have a back and forth about stack. I'm leaning C/C++, whichever will give us the fastest binary, but if another language will accomplish that we choose it. Even consider assembly, though that's risky because we want this binary to be portable.
        - I test on BSD, linux, and MacOS.
    - we discuss features to include/exclude
    - we clone both bat and some source code for cat to .docs/refs/ and do a first pass over those codebases to understand more of what our approach might need, understand areas we can improve their code, introduce performance, etc.
    - we generate an entire scaffolded sprint plan that outlines development from empty file to true bat clone
        - these files should live in .docs/sprints/{01,02,..0n}-<descriptor>.md
        - they should enumerate targets, list pitfalls to avoid, establish architectures, phases, etc. 
        - This is the first deliverable. The development scaffolding files keep us on track for the rest of the development cycle from 00 to 0n production and release.
- Implementation
    - once planning is done, we start with sprint 00 and begin implementing as outlined in the sprint docs.
- Implementation follows these guidelines:

## Guidelines
- We write senior level code
- Tests are first class. 
    - CI is included in that.
        - we should be confident on every push to trunk that we have excellent coverage and insurance
- commit often, in chunks. 
    - avoid monolothic commits and git add -a
    - keep commit messages terse, imperative, <250 chars unless a decision/implementation needs elaboration.
- the repo should live at tenseleyFlow/mat. you have gh auth and can create the remote when we have a first pass/it's grown.
