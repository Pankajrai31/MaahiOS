# Contributing to MaahiOS

Thank you for your interest in contributing to MaahiOS! This document provides guidelines and information for contributors.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/MaahiOS.git
   cd MaahiOS
   ```
3. **Install prerequisites** (see README.md)
4. **Build the project**:
   ```bash
   make
   ```
5. **Test your changes**:
   ```bash
   make run
   ```

## Development Workflow

1. Create a new branch for your feature or bugfix:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. Make your changes and test them thoroughly

3. Build and test:
   ```bash
   make clean
   make
   make run
   ```

4. Commit your changes with a descriptive message:
   ```bash
   git commit -m "Add feature: description of your changes"
   ```

5. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

6. Create a Pull Request on GitHub

## Code Style

- **Assembly code**: Use consistent indentation (4 spaces or tabs)
- **C code**: 
  - Use 4 spaces for indentation
  - Follow K&R style for braces
  - Add comments for complex logic
  - Use descriptive variable and function names

## Areas for Contribution

Here are some ideas for contributions:

### Beginner-Friendly
- Improve documentation
- Add more comments to existing code
- Fix typos or formatting issues
- Add color support to terminal output

### Intermediate
- Implement keyboard input handler
- Add support for additional VGA text modes
- Create a simple command interpreter
- Implement basic string manipulation functions

### Advanced
- Set up Interrupt Descriptor Table (IDT)
- Implement Global Descriptor Table (GDT)
- Add memory management (paging)
- Create a basic scheduler
- Add file system support

## Testing

Always test your changes before submitting:

1. **Build test**: Ensure the code compiles without errors
   ```bash
   make clean && make
   ```

2. **Boot test**: Verify the kernel boots in QEMU
   ```bash
   make run
   ```

3. **Functionality test**: Test any new features you've added

## Commit Message Guidelines

Write clear, concise commit messages:

- Use the present tense ("Add feature" not "Added feature")
- Use the imperative mood ("Move cursor to..." not "Moves cursor to...")
- Limit the first line to 72 characters or less
- Reference issues and pull requests liberally after the first line

Example:
```
Add keyboard input handler

- Implement PS/2 keyboard driver
- Add scancode to ASCII conversion
- Handle special keys (Ctrl, Alt, Shift)

Fixes #123
```

## Pull Request Process

1. Ensure your code builds and runs without errors
2. Update the README.md if you're adding new features
3. Add comments to explain complex code
4. Ensure the PR description clearly describes the changes
5. Be responsive to feedback and code review comments

## Questions?

If you have questions, feel free to:
- Open an issue on GitHub
- Start a discussion in the repository

## Code of Conduct

Be respectful and constructive in all interactions. We're here to learn and build together!

## License

By contributing to MaahiOS, you agree that your contributions will be licensed under the same terms as the project.
