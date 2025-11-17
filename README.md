# Neural Field Dynamics Simulator

A sophisticated C++ simulation framework for studying neural field dynamics with applications in biomedical research and computational neuroscience.

![Neural Field Simulation](docs/simulation_preview.png)

## 🧠 Overview

This project implements a modular neural field system that simulates the dynamics of neural populations using Hamiltonian mechanics and synaptic plasticity rules. The system provides a flexible platform for investigating pattern formation, synchronization, and learning in neural networks.

## 🎯 Key Features

- **Hamiltonian Dynamics**: Symplectic integration for energy-conserving simulations
- **Synaptic Plasticity**: Hebbian and Oja learning rules with configurable parameters
- **Interactive Visualization**: Real-time SFML-based visualization with mouse interaction
- **Modular Architecture**: Separated core system, dynamics, learning, and visualization modules
- **Cross-Platform**: Compatible with macOS, Windows, and Linux
- **Real-time Monitoring**: Comprehensive statistics and energy tracking

## 🔬 Biomedical Research Applications

### Advantages in Neuroscience & Biomedicine

1. **Neurological Disorder Modeling**
   - Simulate pathological synchronization in epilepsy
   - Model cortical spreading depression in migraine
   - Investigate neural dynamics in Parkinson's disease

2. **Learning and Memory Research**
   - Study synaptic plasticity mechanisms
   - Explore pattern formation in neural networks
   - Investigate memory encoding and retrieval dynamics

3. **Drug Development**
   - Test computational models of drug effects on neural dynamics
   - Simulate neurotransmitter modulation
   - Predict system responses to pharmacological interventions

4. **Brain-Computer Interfaces**
   - Develop control algorithms for neural prosthetics
   - Optimize stimulation patterns for neurostimulation devices
   - Train machine learning models on synthetic neural data

## 🏗️ Project Structure

```
cpp_ai_mary/
├── core/ # Core simulation engine
│ ├── NeuralFieldSystem.hpp
│ └── NeuralFieldSystem.cpp
├── modules/ # Modular components
│ ├── DynamicsModule.hpp/.cpp
│ ├── LearningModule.hpp/.cpp
│ ├── VisualizationModule.hpp/.cpp
│ ├── InteractionModule.hpp/.cpp
│ ├── StatisticsModule.hpp/.cpp
│ └── UIModule.hpp/.cpp
├── config/ # Configuration files
│ └── system_config.json
├── docs/ # Documentation
├── scripts/ # Build and utility scripts
└── tests/ # Unit tests
```


## 🚀 Installation & Setup

### Prerequisites

- **C++17** compatible compiler
- **CMake** (version 3.15+)
- **SFML** (Simple and Fast Multimedia Library)

### macOS

```bash
# Install dependencies using Homebrew
brew install cmake sfml

# Clone and build
git clone https://github.com/Khamit/cpp_ai_mary.git
cd cpp_ai_mary
mkdir build && cd build
cmake ..
make -j4

# Run the simulation
./neural_field_sim
```

### Ubuntu/Debian Linux
```bash
# Install dependencies
sudo apt update
sudo apt install cmake libsfml-dev build-essential

# Clone and build
git clone https://github.com/Khamit/cpp_ai_mary.git
cd cpp_ai_mary
mkdir build && cd build
cmake ..
make -j4

# Run
./neural_field_sim
```
### Windows

# Using vcpkg for dependencies
vcpkg install sfml

# Or download SFML manually from https://www.sfml-dev.org/

# Build with CMake
```bash
git clone https://github.com/Khamit/cpp_ai_mary.git
cd cpp_ai_mary
mkdir build && cd build
cmake ..
cmake --build . --config Release
```
# Run
.\Release\neural_field_sim.exe

### 🎮 Usage
bash
# Basic simulation
./neural_field_sim

# With custom configuration
./neural_field_sim --config config/custom_config.json

# Headless mode (no visualization)
./neural_field_sim --headless

Controls
Click on grid: Perturb neural field at specific locations

Start/Stop: Control simulation execution

Reset: Reinitialize the system with random conditions

-------------------------------------------------------

### 🔧 Configuration
Edit config/system_config.json to customize:

Learning rates and rules

Dynamics parameters

Visualization settings

Interaction behavior

### 📊 Output
The simulation generates:

Real-time visualization of neural field states

CSV files with simulation statistics

Energy and state classification data

### 🧪 Research Extensions
Potential Biomedical Extensions:
Multi-layer Networks: Add hierarchical neural structures

Biophysical Models: Incorporate Hodgkin-Huxley type dynamics

Clinical Data Integration: Interface with EEG/fMRI data formats

Therapeutic Simulation: Model deep brain stimulation effects

Network Analysis: Add graph theory metrics for connectivity analysis

### 🤝 Contributing
We welcome contributions! Please see our Contributing Guidelines for details.

Branch Structure:
main - Stable production releases

develop - Development integration branch

feature/ - New features

bugfix/ - Bug fixes

research/ - Experimental research branches

### 📜 License
This project is licensed under the GPL v3 License - see the LICENSE file for details.

For commercial licensing inquiries, please contact the maintainer.

### 📚 References
Wilson, H. R., & Cowan, J. D. (1972). Excitatory and inhibitory interactions in localized populations of model neurons.

Ermentrout, G. B., & Cowan, J. D. (1979). A mathematical theory of visual hallucination patterns.

Coombes, S. (2005). Waves, bumps, and patterns in neural field theories.

## Contact
For questions and collaborations:

GitHub Issues: Create an issue

Email: gercules@gmail.com, khamit@combi.kz

