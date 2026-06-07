# Mary_NFS (Neural Field System) v4.1 - "Agent Auditor"

## Core Changes for Agent Auditing

Система переработана для аудита LLM-агентов в реальном времени.

### Новые компоненты

| Компонент | Назначение |
|-----------|-----------|
| **AgentAuditBridge** | Аудит действий агента, проверка ограничений |
| **SelfSignalSampler** | 32 сигнала самонаблюдения (активность, память, энтропия) |
| **NeuroMemoryRecord** | Память на основе синаптических сигнатур нейронов |
| **AggregatedLogger** | Логирование с агрегацией (5ч/12ч/24ч/неделя/месяц/год) |

### Сигналы для аудита (32 шт)

- Активность нейронов (16) - частоты по группам
- Память (4) - STM/LTM заполненность, важность
- Обучение (4) - surprise, quality, exploration pressure
- Гомеостаз (4) - энтропия, температура, трофины
- Диагностика (4) - апоптоз, нейрогенез

### Механизмы безопасности

- Блокировка опасных действий (delete_file, sudo, exec...)
- Детекция бесконечных циклов
- Оценка риска галлюцинаций
- Гибкие ограничения (длина ответа, уверенность, шаги/сек)

### Технические изменения

- RNG теперь `shared_ptr` (безопасное владение)
- LUT для STDP экспонент (оптимизация)
- Сбор активных спайков (O(N²)→O(N))
- RAII для мьютексов

### Сборка

```bash
make clean && make && make run
```
## ⚠️ Important Notice

Mary AI Core is a local AI system designed for complete privacy:

- 100% Local - No internet, no data transmission
- User Controlled - All data stays on your device
- Open Source (see [LICENSE](LICENSE) for terms)
- No Tracking - No analytics, no telemetry
- Full responsibility details in [NOTICE](/docs/NOTICE.md) section.


### 📜 License
This project is free for [non-commercial use](LICENSE).
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

