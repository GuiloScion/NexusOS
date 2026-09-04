---
title: 'Build Your Own Operating System: a from-scratch x86-64 kernel course with a working reference system'
tags:
  - operating systems
  - kernel development
  - x86-64
  - systems programming
  - bare metal
  - assembly language
  - C
authors:
  - name: Noah Parsons
    orcid: 0009-0000-7224-6040
    affiliation: 1
affiliations:
  - name: Independent Researcher, Newcastle, Wyoming, United States
    index: 1
date: 3 September 2026
bibliography: paper.bib
---

# Summary

*Build Your Own Operating System* is a thirteen-chapter course that takes a
reader from an empty repository to a functioning x86-64 operating system. It
covers the boot process from the firmware handoff through the transition from
real mode to protected mode to long mode; interrupt and exception dispatch;
physical frame allocation and four-level paging; a demand-grown kernel heap;
preemptive multitasking and the synchronisation primitives it requires; reading
from disk without a driver stack; a compositing window manager on a linear
framebuffer; and the differences a reader encounters when moving from an
emulator to real hardware.

The course ships alongside NexusOS, the system it builds. Every listing in the
book refers to a file in the same repository, and a reader who completes the
material arrives at a system equivalent to the reference implementation. The
manuscript is licensed CC BY 4.0; the kernel source and build pipeline are MIT.

# Statement of need

Operating systems is a subject where the gap between explanation and
construction is unusually wide. The standard textbooks — Tanenbaum, and
Silberschatz, Galvin and Gagne — explain mechanisms thoroughly but do not walk a
reader through building one [@tanenbaum; @silberschatz]. The material that does
walk a reader through construction tends to fall into two categories. Community
wikis such as the OSDev wiki are comprehensive as reference but are not
sequenced: they answer questions a reader already knows how to ask
[@osdev]. Tutorial series are sequenced but usually stop at a minimal kernel,
before memory management, scheduling, or graphics.

A second gap is that almost all available material stops at the emulator. QEMU
is forgiving in ways real firmware is not, and a reader who has only ever booted
under emulation has not encountered the class of problem that dominates real
systems work. This course carries the system onto physical hardware in Chapter
12 and treats the resulting failures as material rather than as an appendix.

The course is aimed at readers comfortable with C and willing to read x86-64
assembly, who want to understand operating systems by building one rather than
by reading about one. No prior operating systems knowledge is assumed. It suits
self-directed learners, and could support a project-based undergraduate systems
course where students build the system in stages across a term.

# Instructional design

The course is organised as a build: each chapter adds a subsystem, and the
reader's system is expected to run at the end of every chapter. Chapters close
with a summary of what the reader now has and a statement of what the next
chapter adds, so progress is legible.

Three pedagogical devices run through the material.

**Prediction before exposition.** Chapter 1 asks the reader to write down three
things they suspect could go wrong when an operating system starts on an unknown
machine, and to keep the list. Chapter 12 returns to it, after the system has
been moved onto real hardware, where firmware fails in ways a reader's
imagination generally does not invent. The exercise makes the reader commit to a
model before it is tested, so that the correction lands against something rather
than into a vacuum.

**A diagram built across the course.** Chapter 1 asks the reader to draw the
layers of an operating system as they currently understand them — hardware
through firmware, bootloader, kernel, drivers, system calls, libraries, to
application — leaving space between the layers. Each subsequent chapter's
contribution is written into the diagram as it is completed. By Chapter 13 the
reader has constructed their own map of the system, incrementally, rather than
being given one.

**Grounding in a running system the reader already has.** Exercises direct the
reader to inspect a real Linux boot log and identify the shape of the sequence —
processor detection, memory, controllers, then filesystem mount — before
building the equivalent themselves, with hints pointing at the specific lines
worth noticing. The pattern the reader observes in Chapter 1 is the one they
implement in Chapter 5.

Exercises carry hints rather than solutions, since most have no single correct
answer, and several are explicitly designed to be revisited rather than
completed.

# Verification of the material

The material's correctness is demonstrated by its output. A reader who follows
the chapters arrives at NexusOS, which is public, buildable, and runs both under
QEMU and on physical hardware. The reference implementation's scheduler and
synchronisation primitives were validated over a 5.4-hour run comprising 3.35
million context switches, with zero lost updates across 772,723 protected
concurrent increments. The claim that the course teaches a reader to build a
working operating system is therefore checkable rather than asserted: the system
it builds exists, and its behaviour has been measured.

Listing captions reference the reference source at the `v1.0-book` tag, so a
reader can compare their work against a known state at any point rather than
only at the end.

# Experience of use

The typeset edition was published on 29 June 2026 and has three purchasers to
date. The topic is a narrow one and the openly licensed version in the
repository is new, so recorded use is limited at the time of writing. Feedback
from readers is actively solicited through the repository issue tracker, and
reports that a chapter did not work as written are treated as defects in the
material.

The author's own use is the primary evidence available: the course was written
from the construction of NexusOS itself, and the sequence of chapters follows
the order in which the subsystems were actually built and debugged — including
the hardware failures of Chapter 12, which were encountered rather than
imagined.

# Story of the project

NexusOS was written without an external bootloader, kernel framework, or
tutorial scaffolding, in part because the available material thinned out
precisely where the work became interesting. The book was written afterwards
from the same construction, and exists because the sequence of decisions, dead
ends, and hardware surprises that produced a working system seemed more useful
to a learner than a finished description of one.

# Acknowledgements

[Anyone who read drafts, tested chapters, or reported problems. If nobody
has, remove this section.]

# References
