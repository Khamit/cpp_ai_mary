# Neural Field System v4.0.1 - "Adaptive AI system - Mary"

A sophisticated C++ simulation framework featuring a self-evolving neural field with **1024 neurons (32 groups × 32 neurons)**, adaptive memory management, and real-time code optimization capabilities.

![Neural Field Simulation](docs/simulation_preview.png)

### 🧠 Overview
The core is a hybrid neuroevolutionary system with 1024 neurons (32 groups × 32 neurons), combining:
Neural field dynamics with reentry (recurrent feedback)
STDP plasticity with eligibility traces
Group-level evolutionary algorithms
Modular architecture with explicit component separation

## 🎯 Key Features
Multilevel Learning:
STDP at synapses (learnSTDP)
Hebbian learning (learnHebbian)
Gradient descent (computeGradients / applyGradients)
Evolutionary mutations at group level

Attention Mechanism:
Softmax attention between groups
Integrated with reentry connections

Memory Management:
Short-term and long-term memory
Data quantization (float → int8, ×4 memory saving)
Threshold-based consolidation

Immutable Core (ImmutableCore):
Protects critical functions
Enforces physical constraints (energy, CPU, code safety)

💡 Strengths
Hybrid approach: combines biological plasticity (STDP) with gradient-based methods
Energy-efficient: quantization and pruning save memory
Modular: components can be added/removed without modifying the core
Safe: immutable core protects critical functionality
Multiscale memory: short-term to consolidated memory

⚠️ Limitations
Fixed size: 1024 neurons, cannot scale dynamically
Sequential processing: no parallel group execution
STDP synchronous: real STDP is asynchronous
No intra-group recurrence: only inter-group connections
Simplified gradients: target = 0.5 for all neurons

### EvolutionModule - Self-Optimizing Code
Real-time fitness evaluation based on three metrics:
- **Code Efficiency Score**: Evaluates code size and complexity
- **Performance Score**: Measures computational efficiency
- **Energy Score**: Tracks system energy consumption

Architectural Highlights:
```bash
// Unique combination:
- Neural field theory + STDP + Evolution
- 32 groups × 32 neurons = 1024 neurons
- Inter-group attention connections
- Eligibility trace for reward-modulated STDP
```

### Unique Advantage - The only system combining:
Neural field theory (spatial organization)
STDP with eligibility traces (temporal plasticity)
Group-level evolution (architectural adaptation)
Reentry mechanism (integration across timescales)
Ideal for exploring cognitive architectures and artificial consciousness, where multi-timescale learning integration is crucial.

### 🤝 Contributing
We welcome contributions! Please see our Contributing Guidelines for details.

Branch Structure:
main - Stable production releases
develop - Development integration branch
feature/ - New features
bugfix/ - Bug fixes
research/ - Experimental research branches


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

