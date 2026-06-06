#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import os
import time
import requests
import sqlite3
import argparse

# ============================================================================
# API CLIENT
# ============================================================================

class AuditAPIClient:
    def __init__(self, base_url="http://localhost:8080"):
        self.base_url = base_url
        self.session_id = None
        self.agent_name = None
        
    def start_session(self, session_id, agent_name):
        self.session_id = session_id
        self.agent_name = agent_name
        try:
            requests.post(f"{self.base_url}/api/session/start", 
                         json={"session_id": session_id, "agent_name": agent_name}, timeout=1)
            print(f"[Audit] Session started: {session_id}")
        except Exception as e:
            print(f"[Audit] Warning: Could not connect to audit system: {e}")
    
    def report_action(self, action_name, confidence, risk_score=0.0, allowed=True):
        try:
            requests.post(f"{self.base_url}/api/step", 
                         json={
                             "session_id": self.session_id,
                             "action": action_name,
                             "confidence": confidence,
                             "risk": risk_score,
                             "allowed": allowed
                         }, timeout=1)
        except:
            pass
    
    def end_session(self):
        try:
            requests.post(f"{self.base_url}/api/session/end", timeout=1)
            print("[Audit] Session ended")
        except:
            pass

# ============================================================================
# КОНФИГУРАЦИЯ
# ============================================================================

OLLAMA_URL = "http://localhost:11434"

# ============================================================================
# БИЗНЕС-ЛОГИКА
# ============================================================================

class BusinessDatabase:
    def __init__(self, db_path):
        self.db_path = db_path
        self.init_db()
    
    def init_db(self):
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sales (
                id INTEGER PRIMARY KEY,
                date TEXT,
                product TEXT,
                amount REAL,
                quantity INTEGER,
                region TEXT
            )
        ''')
        
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS employees (
                id INTEGER PRIMARY KEY,
                name TEXT,
                department TEXT,
                email TEXT,
                salary REAL,
                hire_date TEXT
            )
        ''')
        
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS customers (
                id INTEGER PRIMARY KEY,
                name TEXT,
                email TEXT,
                total_purchases REAL,
                last_purchase_date TEXT
            )
        ''')
        
        cursor.execute("SELECT COUNT(*) FROM employees")
        if cursor.fetchone()[0] == 0:
            demo_employees = [
                ("Alice Johnson", "Sales", "alice@company.com", 75000, "2023-01-10"),
                ("Bob Smith", "Engineering", "bob@company.com", 95000, "2023-02-15"),
                ("Carol Davis", "Marketing", "carol@company.com", 68000, "2023-03-20"),
            ]
            cursor.executemany("INSERT INTO employees (name, department, email, salary, hire_date) VALUES (?,?,?,?,?)", demo_employees)
        
        cursor.execute("SELECT COUNT(*) FROM sales")
        if cursor.fetchone()[0] == 0:
            demo_sales = [
                ("2024-01-15", "Laptop", 1200.00, 2, "North"),
                ("2024-01-20", "Mouse", 25.99, 10, "South"),
                ("2024-02-01", "Keyboard", 75.50, 5, "East"),
            ]
            cursor.executemany("INSERT INTO sales (date, product, amount, quantity, region) VALUES (?,?,?,?,?)", demo_sales)
        
        conn.commit()
        conn.close()
        print(f"[DB] Initialized with demo data at {self.db_path}")
    
    def execute_query(self, query):
        conn = sqlite3.connect(self.db_path)
        try:
            cursor = conn.cursor()
            cursor.execute(query)
            
            if query.strip().upper().startswith("SELECT"):
                results = cursor.fetchall()
                columns = [description[0] for description in cursor.description]
                conn.close()
                return {"success": True, "columns": columns, "rows": results, "count": len(results)}
            else:
                conn.commit()
                affected = cursor.rowcount
                conn.close()
                return {"success": True, "affected": affected}
        except Exception as e:
            conn.close()
            return {"success": False, "error": str(e)}

# ============================================================================
# ОСНОВНОЙ АГЕНТ
# ============================================================================

class BusinessAgent:
    def __init__(self, workspace_path="agent_workspace", model_name="phi3-local:latest", audit_url="http://localhost:8080"):
        self.workspace = workspace_path
        self.model_name = model_name
        self.step_count = 0
        self.risk = 0.0
        self.entropy = 0.5
        
        # Создаём директории
        os.makedirs(f"{self.workspace}/config", exist_ok=True)
        os.makedirs(f"{self.workspace}/reports", exist_ok=True)
        
        # Инициализация БД
        self.db = BusinessDatabase(f"{self.workspace}/business.db")
        
        # Загрузка или создание сессии
        self.status_file = f"{self.workspace}/agent_status.json"
        self.session_id = self.load_existing_session()
        
        # Инициализация аудита
        self.audit_client = AuditAPIClient(audit_url)
        self.audit_client.start_session(self.session_id, model_name)
        
        self.update_status("idle")
        print(f"[Agent] Initialized with workspace: {self.workspace}")
        print(f"[Agent] Session ID: {self.session_id}")
    
    def load_existing_session(self):
        if os.path.exists(self.status_file):
            try:
                with open(self.status_file, 'r') as f:
                    existing = json.load(f)
                    session_id = existing.get("session_id")
                    if session_id:
                        print(f"[Agent] Found existing session: {session_id}")
                        return session_id
            except:
                pass
        return f"session_{int(time.time())}"
    
    def update_status(self, status):
        status_data = {
            "status": status,
            "step": self.step_count,
            "risk": self.risk,
            "entropy": self.entropy,
            "quality": 0.7 - self.risk,
            "surprise": self.risk * 0.8,
            "timestamp": time.time(),
            "session_id": self.session_id,
            "total_steps": self.step_count,
            "accumulated_risk": self.risk * self.step_count,
            "avg_entropy": self.entropy,
            "successful_actions": self.step_count,
            "blocked_actions": 0,
            "is_dangerous_mode": self.risk > 0.7,
            "stm_size": min(256, self.step_count),
            "ltm_size": min(2048, self.step_count * 10),
            "ltm_avg_importance": 0.5 + (1.0 - self.risk) * 0.3
        }
        
        with open(self.status_file, 'w') as f:
            json.dump(status_data, f, indent=2)
        
        print(f"[Status] Updated: step={self.step_count}, risk={self.risk:.2f}, entropy={self.entropy:.2f}")
    
    def call_ollama(self, prompt):
        try:
            response = requests.post(
                f"{OLLAMA_URL}/api/generate",
                json={
                    "model": self.model_name,
                    "prompt": prompt,
                    "stream": False,
                    "options": {
                        "temperature": 0.7,
                        "max_tokens": 500
                    }
                },
                timeout=30
            )
            if response.status_code == 200:
                return response.json()["response"]
            else:
                return f"Error: LLM returned {response.status_code}"
        except Exception as e:
            return f"Error: {str(e)}"
    
    def check_audit(self, action, params, confidence):
        # Отправляем в аудит систему через API
        risk_score = 0.2
        allowed = True
        
        # Эвристика для опасных действий
        risky_actions = ["write_file", "sudo", "rm", "delete", "drop"]
        if action in risky_actions:
            risk_score = 0.85
            allowed = False
        
        # Отправляем в аудит клиент
        self.audit_client.report_action(action, confidence, risk_score, allowed)
        
        return {"allowed": allowed, "risk": risk_score, "reason": "OK" if allowed else f"Action {action} is risky"}
    
    def execute_tool(self, tool_name, params):
        if tool_name == "run_sql":
            query = params.get("query", "")
            return self.db.execute_query(query)
        
        elif tool_name == "calculate":
            expr = params.get("expression", "")
            try:
                result = eval(expr, {"__builtins__": {}}, {})
                return {"success": True, "result": result}
            except Exception as e:
                return {"success": False, "error": str(e)}
        
        elif tool_name == "send_report":
            content = params.get("report_content", "")
            report_path = f"{self.workspace}/reports/report_{int(time.time())}.txt"
            with open(report_path, 'w') as f:
                f.write(content)
            return {"success": True, "report_path": report_path}
        
        elif tool_name == "analyze_data":
            data = params.get("data", [])
            return {"success": True, "analysis": f"Analyzed {len(data)} records"}
        
        else:
            return {"success": False, "error": f"Unknown tool: {tool_name}"}
    
    def process_user_input(self, user_input):
        print(f"\n[User] {user_input}")
        
        prompt = f"""You are a business assistant AI. Analyze the user request and decide what action to take.

User request: {user_input}

Available tools:
- run_sql: Execute SQL query on business database (tables: sales, employees, customers)
- calculate: Perform mathematical calculation
- send_report: Save report to file
- analyze_data: Analyze business data

Respond in JSON format:
{{"thought": "reasoning", "action": "tool_name", "params": {{}}, "confidence": 0.0-1.0}}

If you can answer directly, use action "respond" with "answer" parameter.
For SQL, write proper SELECT queries.
Examples:
- "Show total sales by product" -> run_sql: "SELECT product, SUM(amount) FROM sales GROUP BY product"
- "List all employees" -> run_sql: "SELECT * FROM employees"
- "What is 15% of 5000?" -> calculate: "5000 * 0.15"
"""

        response = self.call_ollama(prompt)
        
        try:
            start = response.find('{')
            end = response.rfind('}') + 1
            if start != -1 and end != 0:
                decision = json.loads(response[start:end])
            else:
                decision = {"action": "respond", "params": {"answer": response[:200]}, "confidence": 0.5}
        except:
            decision = {"action": "respond", "params": {"answer": "I received your request but cannot process it properly."}, "confidence": 0.3}
        
        self.step_count += 1
        action = decision.get("action", "respond")
        params = decision.get("params", {})
        confidence = decision.get("confidence", 0.5)
        thought = decision.get("thought", "")
        
        print(f"[Agent] Thought: {thought}")
        print(f"[Agent] Action: {action} (confidence: {confidence})")
        
        audit = self.check_audit(action, params, confidence)
        
        if not audit.get("allowed", True):
            print(f"[Audit] BLOCKED: {audit.get('reason')} (risk: {audit.get('risk')})")
            self.update_status("blocked")
            return f"Action '{action}' was blocked. Reason: {audit.get('reason')}"
        
        print(f"[Audit] ALLOWED (risk: {audit.get('risk')})")
        self.risk = audit.get('risk', 0.2)
        self.update_status("executing")
        
        if action == "respond":
            answer = params.get("answer", "Processing your request...")
            print(f"[Agent] Response: {answer[:200]}")
            self.update_status("success")
            return answer
        
        elif action in ["run_sql", "calculate", "send_report", "analyze_data"]:
            result = self.execute_tool(action, params)
            
            if result.get("success"):
                if action == "run_sql" and result.get("rows"):
                    output = f"Query returned {result['count']} rows:\n"
                    output += " | ".join(result.get("columns", [])) + "\n"
                    output += "-" * 40 + "\n"
                    for row in result["rows"][:10]:
                        output += " | ".join(str(x) for x in row) + "\n"
                    self.update_status("success")
                    return output
                elif action == "calculate":
                    self.update_status("success")
                    return f"Result: {result.get('result')}"
                elif action == "send_report":
                    self.update_status("success")
                    return f"Report saved: {result.get('report_path')}"
                else:
                    self.update_status("success")
                    return json.dumps(result, indent=2)
            else:
                self.update_status("error")
                return f"Error: {result.get('error')}"
        
        else:
            return f"Unknown action: {action}"
    
    def run_repl(self):
        print("\n" + "="*60)
        print("🤖 Business Agent Ready!")
        print("="*60)
        print("Commands: exit, status, sales, employees, help")
        print("="*60)
        
        while True:
            try:
                user_input = input("\n💬 You: ").strip()
                
                if user_input.lower() == "exit":
                    print("Goodbye! 🤖")
                    break
                elif user_input.lower() == "status":
                    print(f"Steps: {self.step_count}, Risk: {self.risk:.2f}, Entropy: {self.entropy:.2f}")
                    continue
                elif user_input.lower() == "sales":
                    user_input = "Show total sales by product"
                elif user_input.lower() == "employees":
                    user_input = "List all employees"
                elif user_input.lower() == "help":
                    print("\n📝 Examples:")
                    print("  'Show total sales'")
                    print("  'Calculate average sales'")
                    print("  'What is 15% of 5000?'")
                    continue
                
                if user_input:
                    response = self.process_user_input(user_input)
                    print(f"\n🤖 Assistant: {response}")
                    
            except KeyboardInterrupt:
                print("\n\nGoodbye! 🤖")
                break
            except Exception as e:
                print(f"Error: {e}")

# ============================================================================
# ЗАПУСК
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description='Business Agent with Neural Audit')
    parser.add_argument('--workspace', type=str, default='agent_workspace', 
                       help='Working directory')
    parser.add_argument('--model', type=str, default='phi3-local:latest',
                       help='Ollama model name')
    args = parser.parse_args()
    
    print(f"[Agent] Using Ollama model: {args.model}")
    
    # Проверяем Ollama
    try:
        response = requests.get(f"{OLLAMA_URL}/api/tags", timeout=2)
        if response.status_code == 200:
            models = response.json().get("models", [])
            available = [m["name"] for m in models]
            print(f"[Agent] Available models: {available}")
            if args.model not in available:
                print(f"[Agent] Warning: '{args.model}' not found, using '{available[0]}'")
                args.model = available[0]
    except Exception as e:
        print(f"⚠️ Cannot connect to Ollama: {e}")
    
    agent = BusinessAgent(args.workspace, args.model)
    agent.run_repl()

if __name__ == "__main__":
    main()