# Contributing to Voice-to-Todo Thermal Printer

Thank you for your interest in contributing to the Voice-to-Todo Thermal Printer project! This document provides guidelines for contributing.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/your-username/voice-todo-thermal-printer.git
   cd voice-todo-thermal-printer
   ```
3. **Set up your development environment** following the setup guide in `docs/setup_guide.md`
4. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

### Hardware Setup
- Follow the circuit diagram in `docs/circuit_diagram.md`
- Test your hardware setup before making code changes
- Document any hardware variations you're using

### Software Setup
1. **Arduino Environment**:
   - Install Arduino IDE 2.0+
   - Install ESP32 board package
   - Install required libraries (ArduinoJson)

2. **Python Environment**:
   ```bash
   cd audio_server
   pip install -r requirements.txt
   ```

3. **Credentials Configuration**:
   ```bash
   cp credentials.h.example credentials.h
   # Edit credentials.h with your WiFi settings
   ```

## Making Changes

### Code Style Guidelines

#### Arduino C++ Code
- Use consistent indentation (2 spaces)
- Follow camelCase for variables and functions
- Use descriptive variable names
- Add comments for complex logic
- Keep functions focused and small

#### Python Code
- Follow PEP 8 style guidelines
- Use type hints where appropriate
- Add docstrings to functions and classes
- Use meaningful variable names
- Keep functions under 50 lines when possible

### Testing Your Changes

#### Before Submitting
1. **Test hardware functionality**:
   - Verify microphone recording works
   - Test button input response
   - Confirm printer output quality

2. **Test audio processing**:
   ```bash
   cd examples
   python test_audio.py
   ```

3. **Test voice-to-todo pipeline**:
   - Record various speech patterns
   - Verify todo item parsing accuracy
   - Test with different Whisper models

4. **Check for security issues**:
   - Ensure no credentials are committed
   - Verify .gitignore is working properly
   - Test with fresh clone

### Documentation

When making changes:
- Update relevant documentation in `docs/`
- Update README.md if adding new features
- Add inline comments for complex code
- Update circuit diagrams if hardware changes

## Submitting Changes

### Pull Request Process

1. **Commit your changes** with descriptive messages:
   ```bash
   git add .
   git commit -m "feat: add support for multiple Whisper models"
   ```

2. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

3. **Create a Pull Request** on GitHub with:
   - Clear title describing the change
   - Detailed description of what was changed
   - Reference any related issues
   - Include testing steps
   - Add screenshots/videos if relevant

### Commit Message Format
Use conventional commits format:
- `feat:` for new features
- `fix:` for bug fixes
- `docs:` for documentation changes
- `style:` for formatting changes
- `refactor:` for code refactoring
- `test:` for adding tests
- `chore:` for maintenance tasks

## Types of Contributions

### Bug Fixes
- Check existing issues first
- Include reproduction steps
- Test your fix thoroughly
- Add regression tests if possible

### New Features
- Discuss major features in an issue first
- Keep changes focused and atomic
- Update documentation
- Consider backward compatibility

### Documentation Improvements
- Fix typos and unclear explanations
- Add missing setup steps
- Improve circuit diagrams
- Add troubleshooting guides

### Hardware Variations
- Document alternative components
- Update circuit diagrams
- Test compatibility thoroughly
- Update pin configuration guides

## Code Review Process

### What We Look For
- **Functionality**: Does the code work as intended?
- **Security**: Are credentials properly protected?
- **Documentation**: Is the change well documented?
- **Testing**: Has the change been tested thoroughly?
- **Style**: Does the code follow project conventions?

### Review Timeline
- Initial review within 1-2 days
- Feedback and iteration as needed
- Final approval and merge

## Getting Help

### Questions and Support
- **GitHub Issues**: For bug reports and feature requests
- **Discussions**: For general questions and ideas
- **Documentation**: Check `docs/` folder first

### Development Issues
- Hardware problems: Check `docs/circuit_diagram.md`
- Software issues: Check `docs/setup_guide.md`
- Network problems: Run `examples/test_audio.py discover`

## Community Guidelines

### Be Respectful
- Use inclusive language
- Be patient with newcomers
- Provide constructive feedback
- Help others learn

### Focus on Quality
- Test your changes thoroughly
- Write clear documentation
- Follow established patterns
- Consider edge cases

## Recognition

Contributors will be acknowledged in:
- GitHub contributors list
- Project documentation
- Release notes for significant contributions

## License

By contributing, you agree that your contributions will be licensed under the same MIT License that covers the project.

---

Thank you for contributing to make voice-controlled todo printing accessible to everyone! 🎉