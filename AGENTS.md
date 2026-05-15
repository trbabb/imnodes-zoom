This project is a fork of imnodes, a c++ library for creating node-based editors in ImGui. The intent of this fork is to:
- Add support for zooming the node editor canvas
- Add support for arbitrary widgets in the body of a node
- Improve performance for large node graphs

Never overwrite user notes in text files. If you are asked to record notes, create new files for new notes and version them if necessary. Ignore the contents of `prompts/`, these are current/previous prompts and in-progress notes that are not intended to be read by you. I will provide you with instructions directly.

Please follow existing code patterns and conventions, including file organization. Avoid duplicating code. If the intent or specification for the task is unclear, ask questions. Push back if I ask you to do anything clunky or dumb. Don't necessarily assume that because I ask about an alternative design that I automatically want you to implement it; push for it if it's better, push back if there is a good reason behind the current design.

Avoid spandrels. Aim for a small number of simple concepts that compose nicely rather than piles of disparate features. Can the same task be accomplished with fewer concepts? If so, prefer the simpler design. Look for clean abstractions and separation of concerns. Collect related functionality together, and try to avoid cross-entanglement. Do not make multiple systems that do similar things. If you are writing a system that is similar to another existing system, examine whether their functionality should be handled by a single system.

When fixing bugs, fix the root cause of the bug, including if the root cause is due to established code or a design flaw. If fixing the root cause has major or uncertain implications, surface the issue for discussion rather than silently applying a workaround or a kludge.

Avoid use of exceptions. Errors are better handled with explicit error types and return values, as it requires the caller to handle the problem.

Support narrow sanity check tests with stochastic tests that run many trials and many compositions of cases, where appropriate. Do not use Mersenne twister for randomness; use pcg64. If using an arbitrary constant seed for any randomness (especially hash nonces), generate the seed from a high-entropy source.

Document functions with docstrings. Say what the function does, what it is used for, and any invariants on its inputs and outputs. Comment major sections of code explaining the intent. Document corner cases. Clarify tricky logic. We humans have weak memory, and agents' memory gets reset on every session; save us all future cognitive load by writing down your thinking. This also helps expose where intent and code diverge. If you make changes to code, ensure that existing comments are concordant with your changes.

Do not respond with a multiple-choice selector. Present your questions for discussion as ordinary text/markup.

When opening PRs, be sure to open the PR against the FORKED repository, not the original upstream imnodes repository. `gh` commands will default to the latter, so you must specify.
