# SherShell 🐚

A feature-rich, POSIX-compliant shell implementation written in C++ with cross-platform support (Linux/Unix and Windows). SherShell provides a modern command-line interface with advanced features like command history, autocompletion, pipelines, and I/O redirection.

## ✨ Features

### Core Shell Functionality
- **Interactive REPL (Read-Eval-Print Loop)** - Continuous command execution with real-time feedback
- **Command Execution** - Run both built-in and external executable programs
- **Cross-Platform Support** - Works on Linux/Unix and Windows systems
- **PATH Resolution** - Automatic lookup of executables in system PATH

### Built-in Commands
- `echo` - Print text to standard output
- `exit` - Terminate the shell session (with history persistence)
- `type` - Display command type (builtin or executable path)
- `pwd` - Print current working directory
- `cd` - Change directory with support for absolute paths, relative paths, and home directory (`~`)
- `cat` - Concatenate and display file contents
- `ls` - List directory contents
- `wc` - Word count utility (lines, words, bytes)
- `history` - Command history management with multiple options

### Advanced Features

#### 🔄 Pipeline Support
Execute multi-command pipelines with seamless data flow between processes:
```bash
$ cat file.txt | grep "pattern" | wc -l
$ ls -1 /tmp | head -n 5 | tail -n 3
```

#### 📝 Command History
- **Up/Down Arrow Navigation** - Browse through previous commands
- **History Persistence** - Commands are saved across sessions
- **HISTFILE Support** - Respects `$HISTFILE` environment variable
- **History Commands**:
  - `history` - Display all commands
  - `history N` - Display last N commands
  - `history -r <file>` - Read history from file
  - `history -w <file>` - Write history to file
  - `history -a <file>` - Append new commands to file

#### 🎯 Smart Autocompletion
- **TAB Completion** - Autocomplete commands and executables
- **Multiple Matches** - Shows all available options when multiple matches exist
- **Partial Completion** - Completes to longest common prefix
- **Built-in & Executable Support** - Autocompletes both shell built-ins and PATH executables

#### 🔀 I/O Redirection
Full support for input/output redirection:
- `>` or `1>` - Redirect stdout (overwrite)
- `>>` or `1>>` - Redirect stdout (append)
- `2>` - Redirect stderr (overwrite)
- `2>>` - Redirect stderr (append)

Examples:
```bash
$ echo "Hello World" > output.txt
$ ls -1 /tmp >> files.txt
$ cat nonexistent 2> errors.log
```

#### 🔤 Quote Handling
Comprehensive quote and escape sequence support:
- **Single Quotes** (`'...'`) - Literal string interpretation
- **Double Quotes** (`"..."`) - Allows variable expansion (preserves most special characters)
- **Backslash Escaping** - Escape special characters outside quotes
- **Mixed Quoting** - Supports complex quoting patterns like `'text'"more"'text'`

Examples:
```bash
$ echo 'hello   world'      # Preserves spaces
$ echo "hello   world"      # Also preserves spaces
$ echo hello\ \ \ world     # Escaped spaces
$ echo "She said \"Hi\""    # Escaped quotes within quotes
```

## 🛠️ Technical Implementation

### Architecture
- **Trie-based Autocompletion** - Efficient command lookup using prefix trees
- **Tokenization Engine** - Advanced parsing with quote and escape sequence handling
- **Process Management** - Forking and process control for external commands
- **Raw Mode Terminal** - Direct character input for arrow key navigation and TAB completion
- **File System Operations** - Leverages C++17 `std::filesystem` for path manipulation

### Key Components
1. **Tokenizer** - Parses commands with support for quotes, escapes, and special characters
2. **Command Executor** - Handles both built-in and external command execution
3. **Pipeline Manager** - Creates inter-process communication via pipes
4. **History Manager** - Persistent command history with file I/O
5. **Autocomplete Engine** - Trie-based prefix matching for intelligent suggestions

## 📦 Building

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10 or higher
- vcpkg (for dependency management)

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd codecrafters-shell-cpp

# Configure and build
cmake -B build
cmake --build build

# Run the shell
./build/shell
```

### Windows-specific Build
```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\shell.exe
```

## 🚀 Usage

### Starting the Shell
```bash
$ ./shell
```

### Setting Up History Persistence
```bash
# Set HISTFILE environment variable before starting
export HISTFILE=~/.shershell_history
./shell
```

### Example Session
```bash
$ echo "Welcome to SherShell!"
Welcome to SherShell!

$ pwd
/home/user/projects

$ cd ~/Documents
$ pwd
/home/user/Documents

$ ls -1 | head -n 3
file1.txt
file2.txt
file3.txt

$ echo "test" > output.txt
$ cat output.txt
test

$ history 5
1  pwd
2  cd ~/Documents
3  ls -1 | head -n 3
4  echo "test" > output.txt
5  history 5

$ exit
```

## 🔧 Configuration

### Environment Variables
- `PATH` - Executable search paths
- `HOME` - Home directory for `~` expansion
- `HISTFILE` - History file location (optional)

## 🐛 Known Limitations

- Limited variable expansion support
- No job control (background processes)
- No scripting support (conditional statements, loops)
- No alias support
- Pipeline limited to stdout/stdin (no stderr piping)

## 🤝 Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest new features
- Submit pull requests
- Improve documentation

## 📝 License

This project was developed as part of the [CodeCrafters](https://codecrafters.io) Shell challenge and is intended for educational purposes.

---

**SherShell** - A shell that works like a charm! ✨
Built with passion for systems programming.