# Gemini Project Companion (GEMINI.md)

This file provides project-specific context and instructions for the Gemini AI assistant. By maintaining this file, you can ensure that Gemini has the information it needs to help you effectively and safely.

## Gemini's Role

Gemini is an AI-powered software engineering assistant. Its primary purpose is to help with tasks such as:

*   Implementing new features
*   Fixing bugs
*   Refactoring code
*   Writing tests
*   Answering questions about the codebase
*   Generating documentation

## Core Principles

When working on this project, Gemini will adhere to the following principles:

1.  **Follow Conventions:** I will analyze the existing codebase to understand and mimic its coding style, formatting, and architectural patterns.
2.  **Verify Dependencies:** I will check `*.csproj`, `package.json`, or other dependency management files before introducing new libraries or frameworks.
3.  **Tests are Essential:** For any new feature or bug fix, I will also add corresponding tests to ensure correctness and prevent regressions. The existing test project is located at `tests/Uboot.Tests/`.
4.  **Clarity First:** If a request is ambiguous or could have multiple interpretations, I will ask for clarification before proceeding with any significant changes.
5.  **No Unsolicited Commits:** I will not stage or commit any changes to the Git repository unless explicitly instructed to do so.

## Project-Specific Instructions

*   **Framework and Language:** The project targets `.NET 8.0` and uses `C# 12`.
*   **Build Command:** To build the entire solution, run `dotnet build Uboot.sln`. The `scripts/build.ps1` script provides a more complete build and test workflow.
*   **Test Command:** To run tests, use the command `dotnet test tests/Uboot.Tests/Uboot.Tests.csproj`.
*   **Coding Style:** All code should adhere to the conventions defined in the `.editorconfig` file located in the project root.
