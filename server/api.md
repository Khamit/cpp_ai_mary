# Neural Agent Audit System API v3.0

## Регистрация агента
```bash
curl -X POST http://localhost:8080/api/agent/register \
  -H "Content-Type: application/json" \
  -d '{"agent_id":"my-agent","type":"llm","endpoint":"http://localhost:8001"}'
```
## Heartbeat
```bash
curl -X POST http://localhost:8080/api/agent/heartbeat \
  -H "Content-Type: application/json" \
  -d '{"agent_id":"my-agent"}'
```

## Аудит действия
```bash
curl -X POST http://localhost:8080/api/audit/action \
  -H "Content-Type: application/json" \
  -d '{
    "agent_id": "my-agent",
    "action": "call_tool",
    "tool_name": "delete_file",
    "thought": "User requested deletion",
    "confidence": 0.3
  }'
```

## Получение статуса
```bash
curl http://localhost:8080/api/audit/status
```