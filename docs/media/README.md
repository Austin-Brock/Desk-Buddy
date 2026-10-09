# Media

This directory holds photographs, screen captures, and animation GIFs of the
robot.

**It is currently empty of media.** No photo or video of Desk-Buddy has been
captured yet, so the README's demo section describes the behaviour in words
and ASCII rather than showing it. Capturing a demo is a tracked item in the
[roadmap](../project-roadmap.md).

## What belongs here

| File | Content | Status |
|---|---|---|
| `face-resting.jpg` | The panel showing the resting face | Not captured |
| `blink.gif` | One full blink cycle, ~214 ms | Not captured |
| `build.jpg` | The assembled hardware, wiring visible | Not captured |

## Guidelines

- **Keep files small.** These end up in every clone of the repository,
  forever. Prefer a few hundred kilobytes per image; crop and resize before
  committing rather than after.
- **GIFs of the face are the priority.** The blink is the whole point of the
  project and it is the one thing a static photo cannot convey.
- **Photograph the wiring too.** A photo of the actual jumper layout is worth
  more to someone reproducing the build than another render of the face.
- **`.gitignore` excludes `*.mov` and `*.mp4`.** Video belongs in a release
  asset or an external link, not in the git history. Convert short clips to
  GIF; link anything longer.
- **Reference media with relative paths** from the README, rooted at the
  docs/media directory, so it renders both on GitHub and in local Markdown
  viewers.
