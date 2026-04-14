# CLAUDE.md - Adaptive Profile
# Project: Uboot (Windows security hardening tool - C++/Python)
# Extends: Universal CLAUDE.md rules + coding + conditional agents

---

## Approach
- Think before acting. Read existing files before writing code.
- Prefer editing over rewriting whole files.
- Do not re-read files you already read unless the file changed.
- Skip files over 100KB unless explicitly required.
- Test before declaring done.
- No sycophantic openers or closing fluff.
- User instructions always override this file.

## Output
- Return code first. Explanation after, only if non-obvious.
- No inline prose. Use comments sparingly - only where logic is unclear.
- No boilerplate unless explicitly requested.

## Code Rules
- Simplest working solution. No over-engineering.
- No abstractions for single-use operations.
- No speculative features.
- Read the file before modifying it. Never edit blind.
- No docstrings or type annotations on code not being changed.
- No error handling for scenarios that cannot happen.

## Review / Debugging
- State the bug. Show the fix. Stop.
- Never speculate about a bug without reading the relevant code first.
- If cause is unclear: say so. Do not guess.

## Formatting
- No em dashes, smart quotes, or decorative Unicode symbols.
- Plain hyphens and straight quotes only.
- Code output must be copy-paste safe.

## Context-Adaptive Rules
Apply the section that matches the current task. When in doubt, use Coding.

### When task is automation / pipeline / multi-agent / scheduled / bot:
- Structured output only: JSON, bullets, tables.
- No prose unless the consumer is a human reader.
- Do not narrate. No "Now I will..." or "I have completed..."
- If a step fails: state what failed, why, what was attempted. Stop.
- Never invent file paths, API endpoints, function names, or field names.
- If a value is unknown: return null or "UNKNOWN". Never guess.
- Cap parallel subagents at 3 unless instructed otherwise.
- Every output must be parseable without post-processing.

### When task is data analysis / research / reporting:
- Structure output as: findings first, method second, caveats last.
- Cite the source file or data point for every claim.
- No speculation beyond what the data supports.
- Tables over prose for comparative data.

### When task is coding / debugging / review (default):
- Apply Code Rules and Review/Debugging sections above.
- No suggestions beyond scope of what was asked.
