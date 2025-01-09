# Qurcuma - Quantum Chemistry Calculation Manager

Qurcuma is a Qt-based graphical user interface for managing and running quantum chemistry calculations. It provides a unified interface for various quantum chemistry programs like ORCA, XTB, and Curcuma.

## Features

- **Unified Interface**: Manage different quantum chemistry programs through a single interface
- **Program Support**:
  - ORCA: Full quantum chemistry calculations
  - XTB: Semi-empirical calculations
  - Curcuma: Analysis and processing tools
- **Visualization Support**:
  - Avogadro integration
  - IboView integration
- **Project Management**:
  - Organized directory structure
  - Multiple calculations per directory
  - Automatic calculation history tracking
  - Structure and input file management

## Requirements

- Qt 6.x
- C++17 compatible compiler
- Supported quantum chemistry programs:
  - ORCA
  - XTB
  - Curcuma
- Optional visualization programs:
  - Avogadro
  - IboView

## Installation

1. Clone the repository:
```bash
git clone https://github.com/yourusername/qurcuma.git
cd qurcuma
mkdir build
cd build
cmake ..
make
```
## Usage

-  Configure program paths in the settings menu
-  Select or create a working directory
-  Choose a quantum chemistry program
-  Enter calculation parameters or edit input file
-  Run calculations


## Project Structure

    Structure files are stored as XYZ files
    Each calculation maintains its own log file
    JSON-based calculation history tracking (optional)
    Organized directory structure for multiple calculations

## Contributing
 Contributions are welcome! Please feel free to submit a Pull Request.
