# Neural Field System v4.0.1 - "Adaptive AI system - Mary"

![Neural Field Simulation](docs/simulation_preview.png)

## AI Constitutional Framework

This project follows the principles described in the document:
[**Declaration on the Constitutional Principles of Artificial Intelligence**](/docs/AI_CONSTITUTION_DECLARATION.md)

===============================================

### Core Architecture
A hybrid neuroevolutionary system with **1024 neurons (32 groups × 32 neurons)**, combining:

|                 Component | Description                                         |
|---------------------------|-----------------------------------------------------|
| **Neural Field Dynamics** | Wilson-Cowan equations with reentry (recurnt fdbck) |
| **STDP + Eligibility**    | Reward-modulated spike-timing dependent plasticity |
| **Group Evolution**       | Genetic algorithms at group level |
| **Entropy Regulation**    | Prevents mode collapse, maximizes information cpcty |
| **π-Geometry**            | Spatial organization of connections with natural periodicity |

## Key Features

### 🔬 Advanced Learning Mechanisms
- **STDP at synapses** (`learnSTDP`) with eligibility traces and reward modulation
- **Entropy-based gradients** (replaces fixed `target = 0.5`)???(not fixed yet)
- **Hebbian learning** (`learnHebbian`) with global reward
- **Evolutionary mutations** at group level
- **π-constrained initialization** for natural spatial patterns

### Attention System
- Softmax attention between groups with **entropy monitoring**
- **Spherical attention** (π-normalized) for geometric coherence
- Integrated with reentry connections

### Mathematical Core
```cpp
// Entropy as learning objective (replaces naive target)
double entropy = -∑ p(x) * log(p(x))  // Maximize information capacity

// π-geometric constraints in connections
double angle = 2π * distance / N;
double weight = sin(angle) * geometric_factor;
```
### Three-Level Memory Architecture
Level	Timescale	Mechanism
L1: Fast	Every step	STDP, eligibility traces
L2: Slow	Every 100 steps	Consolidation, pruning by elevation
L3: Evolutionary	Every 5000 steps	Group mutations, architecture adaptation
### Immutable Core (ImmutableCore)
Protects critical functions
Enforces physical constraints (energy, CPU bounds)
Entropy monitoring for system health
Safety protocols with permission levels
### Unique Advantages
 Entropy-driven learning - prevents mode collapse, maximizes information
 π-geometric organization - natural spatial patterns like biological brains
 Multi-timescale adaptation - from milliseconds to evolutionary time
 Privacy-first - all learning happens locally, no data leaves device
 Energy efficient - quantization (float→int8, 4× memory saving)
 Modular - components via CoreSystem registry

### Current Limitations
- Fixed 1024 neurons (but optimized for this scale)
- Sequential group processing (parallel planned)
- No intra-group recurrence yet
- Entropy range: [0, log(10)] for groups, [0, log(20)] system-wide

### EvolutionModule - Self-Optimizing Code
Real-time fitness evaluation based on:

**Code Efficiency Score** - size and complexity
**Performance Score** - computational efficiency
**Energy Score** - power consumption
**Entropy Score** - information-theoretic fitness

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

### Perfect For...
- Personal AI assistants that learn YOUR patterns
- Edge devices (watches, IoT, microcontrollers)
- Privacy-critical applications (medical, industrial)
- Behavior prediction and automation
- Research in cognitive architectures and artificial consciousness

### 🤝 Contributing
We welcome contributions! Please see our Contributing Guidelines for details.

Branch Structure:
main - Stable production releases
develop - Development integration branch
feature/ - New features
bugfix/ - Bug fixes
research/ - Experimental research branches


# 🚀 Quick Start

### Prerequisites
- macOS with M1 chip (optimized)
- SFML library
- C++17 compiler

## Installation

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
## ⚠️ Important Notice

Mary AI Core is a local AI system designed for complete privacy:

- 100% Local - No internet, no data transmission
- User Controlled - All data stays on your device
- Open Source (see [LICENSE](LICENSE.md) for terms)
- No Tracking - No analytics, no telemetry
- Full responsibility details in [NOTICE](/docs/NOTICE.md) section.


### 📜 License
This project is free for [non-commercial use](LICENSE.md).
For commercial use, please contact <gercules789@gmail.com> for licensing.

For commercial licensing inquiries, please contact the maintainer.

###  Scientific Foundations
Wilson, H. R., & Cowan, J. D. (1972). Neural field dynamics
Ermentrout, G. B., & Cowan, J. D. (1979). Pattern formation
Coombes, S. (2005). Waves in neural fields
Shannon, C. E. (1948) - Information theory (entropy foundation)

### Contact
For questions and collaborations:

GitHub Issues: [Create a new issue](https://github.com/khamit/cpp_ai_mary/issues/new?title=Bug+report&body=Please+describe+the+bug+steps&labels=bug)

Email: gercules789@gmail.com, khamit@combi.kz

Telegram: [@lordekz](https:/t.me/lordekz)

Built with ❤️ for the M1 Mac | 1024 neurons | Self-evolving code | Priority memory
Idea and architecture by Khamit. Implementation assisted by AI tools (DeepSeek/GPT) - free versions.

