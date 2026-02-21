# Neural Field System v2.0 - "Evolving Intelligence"

A sophisticated C++ simulation framework featuring a self-evolving neural field with **1024 neurons**, adaptive memory management, and real-time code optimization capabilities.

![Neural Field Simulation](docs/simulation_preview.png)

## 🧠 Overview

A revolutionary C++ simulation framework that implements self-evolving neural field dynamics with real-time code optimization, adaptive memory, and autonomous learning capabilities. The system features a unique **MemoryModule** that stores and retrieves past experiences, enabling true adaptive behavior.

## 🎯 Key Features

### 🔬 Core Neural Engine
- **1024 neurons** (32x32 grid) for complex pattern formation
- Symplectic integration for energy conservation
- Real-time field visualization with SFML
- Configurable physical parameters (mass, coupling, damping)

### 🧠 MemoryModule - Intelligent Experience Storage
- **Priority-based memory** with automatic cleanup
- 64-dimensional feature vectors from neural activity
- Cosine similarity search for relevant past experiences
- Automatic decay (forgetting) mechanism: `utility *= 0.995`
- Persistent storage with checkpoint system
- Memory limits: configurable up to 10,000 records

### 🔄 EvolutionModule - Self-Optimizing Code
Real-time fitness evaluation based on three metrics:
- **Code Efficiency Score**: Evaluates code size and complexity
- **Performance Score**: Measures computational efficiency
- **Energy Score**: Tracks system energy consumption

Features:
- Autonomous code mutation proposals
- Fitness-based evolution triggers (threshold: 0.8)
- Automatic backup before evolution cycles
- Stasis mode during resource constraints

### 📊 ResourceMonitor - Intelligent Resource Management
- Adaptive CPU thresholds (default: 85%)
- Memory usage tracking (default: 90%)
- Debounce protection for overload detection
- Dynamic performance scaling

### 🔒 ImmutableCore - Secure Foundation
- Permission system for mutations
- Safety protocols for stable evolution
- Rollback mechanisms for failed optimizations

### 🤝 Contributing
We welcome contributions! Please see our Contributing Guidelines for details.

Branch Structure:
main - Stable production releases
develop - Development integration branch
feature/ - New features
bugfix/ - Bug fixes
research/ - Experimental research branches

### History

v1.0: Basic neural field simulation with static modules - in old folder v0.1

v2.0: Self-evolving system with autonomous optimization ← Bug with memory

v3.0+: External code optimization MemoryRecord! MemoryController, all saves in "dump/" folder ← You are here!


## 🚀 Quick Start

### Prerequisites
- macOS with M1 chip (optimized)
- SFML library
- C++17 compiler

### Installation

```bash
# Clone repository
git clone https://github.com/khamit/cpp_ai_mary.git
cd cpp_ai_mary

# Install SFML (Homebrew)
brew install sfml

# Compile
make clean
make

# Run simulation
make run
```

===================================================================

### 📜 License
This project is licensed under the GPL v3 License - see the [GPL v3 License](LICENSE). file for details.

For commercial licensing inquiries, please contact the maintainer.

### 📚 References
Wilson, H. R., & Cowan, J. D. (1972). Excitatory and inhibitory interactions in localized populations of model neurons.

Ermentrout, G. B., & Cowan, J. D. (1979). A mathematical theory of visual hallucination patterns.

Coombes, S. (2005). Waves, bumps, and patterns in neural field theories.

### Contact
For questions and collaborations:

GitHub Issues: [Create a new issue](https://github.com/khamit/cpp_ai_mary/issues/new?title=Bug+report&body=Please+describe+the+bug+steps&labels=bug)

Email: gercules@gmail.com, khamit@combi.kz

Telegram: [@lordekz](https:/t.me/lordekz)

Built with ❤️ for the M1 Mac | 1024 neurons | Self-evolving code | Priority memory

