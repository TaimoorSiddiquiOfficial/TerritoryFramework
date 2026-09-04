```markdown
# TerritoryFramework Development Patterns

> Auto-generated skill from repository analysis

## Overview
This skill teaches the core development patterns and conventions used in the TerritoryFramework C# codebase. It covers file organization, naming conventions, import/export styles, and outlines how to write and run tests. While no explicit workflows were detected, this guide provides best practices and suggested commands for common development tasks.

## Coding Conventions

### File Naming
- Use **PascalCase** for all file names.
  - **Example:** `TerritoryManager.cs`, `UserProfileHandler.cs`

### Import Style
- Use **relative imports** to reference other files or namespaces within the project.
  - **Example:**
    ```csharp
    using TerritoryFramework.Models;
    using TerritoryFramework.Utils;
    ```

### Export Style
- Use **named exports** for classes, interfaces, and methods.
  - **Example:**
    ```csharp
    public class TerritoryManager
    {
        // Class implementation
    }
    ```

### Commit Patterns
- Commit messages are freeform and do not follow a strict prefixing convention.
- Average commit message length: ~58 characters.

## Workflows

### Adding a New Feature
**Trigger:** When implementing a new feature or module  
**Command:** `/add-feature`

1. Create a new file using PascalCase for the feature (e.g., `NewFeature.cs`).
2. Implement the feature as a named class or method.
3. Use relative imports to include necessary dependencies.
4. Write corresponding tests in a `*.test.*` file.
5. Commit changes with a clear, descriptive message.

### Fixing a Bug
**Trigger:** When addressing a bug or issue  
**Command:** `/fix-bug`

1. Locate the relevant file(s) using PascalCase naming.
2. Apply the fix, ensuring code style consistency.
3. Update or add tests in the appropriate `*.test.*` file.
4. Commit with a message describing the fix.

### Writing and Running Tests
**Trigger:** When validating new or existing functionality  
**Command:** `/run-tests`

1. Write tests in files matching the pattern `*.test.*` (e.g., `TerritoryManager.test.cs`).
2. Use the project's preferred (unknown) testing framework syntax.
3. Run tests using the project's test runner or build tool.
4. Review and address any failing tests.

## Testing Patterns

- Test files follow the pattern: `*.test.*` (e.g., `FeatureHandler.test.cs`)
- The specific testing framework is not detected; follow standard C# testing practices (e.g., MSTest, NUnit, or xUnit).
- Place tests alongside or in a dedicated test directory as per project structure.

## Commands
| Command       | Purpose                               |
|---------------|---------------------------------------|
| /add-feature  | Scaffold and implement a new feature  |
| /fix-bug      | Apply and commit a bug fix            |
| /run-tests    | Run all test files in the project     |
```