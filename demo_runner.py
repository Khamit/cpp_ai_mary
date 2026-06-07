#!/usr/bin/env python3
"""
Demo Agent Runner - работает как отдельный процесс
Запуск: python3 demo_runner.py start --workspace agent_workspace
"""

import time
import os
import sys
import json
import random
import argparse
import signal

running = True
FIXED_SESSION_ID = "demo_agent_permanent"  # ← ФИКСИРОВАННЫЙ ID

def signal_handler(signum, frame):
    global running
    print("[DemoAgent] Received stop signal")
    running = False

def run_demo_agent(workspace="agent_workspace"):
    global running
    
    status_file = os.path.join(workspace, "agent_status.json")
    pid_file = os.path.join(workspace, "demo.pid")
    step = 0
    
    # Сохраняем PID
    with open(pid_file, 'w') as f:
        f.write(str(os.getpid()))
        
    print(f"[DemoAgent] Starting in {workspace}")
    print(f"[DemoAgent] Writing to {status_file}")
    print(f"[DemoAgent] Fixed Session ID: {FIXED_SESSION_ID}")
    
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    
    while running:
        step += 1
        
        risk = random.uniform(0.1, 0.8)
        entropy = 0.3 + risk * 0.5
        quality = 0.9 - risk * 0.6
        
        status = {
            "status": "active",
            "session_id": FIXED_SESSION_ID,  # ← ПОСТОЯННЫЙ ID!
            "total_steps": step,
            "risk": risk,
            "entropy": entropy,
            "quality": quality,
            "surprise": risk * 0.8,
            "timestamp": time.time(),
            "accumulated_risk": step * risk * 0.5,
            "avg_entropy": entropy,
            "successful_actions": max(0, step - random.randint(0, 2)),
            "blocked_actions": random.randint(0, 2),
            "is_dangerous_mode": risk > 0.7,
            "stm_size": min(256, step * 2),
            "ltm_size": min(2048, step * 10),
            "ltm_avg_importance": 0.5 + (1.0 - risk) * 0.3,
            "temperature": 0.8 + risk * 0.5,
            "energy": 10.0 - risk * 5,
            "energy_error": risk * 0.3,
            "violations": int(step * risk * 0.1)
        }
        
        try:
            with open(status_file, 'w') as f:
                json.dump(status, f, indent=2)
            print(f"[DemoAgent] Step {step}: Risk={risk:.2f}, Session={FIXED_SESSION_ID}")
        except Exception as e:
            print(f"[DemoAgent] Error: {e}")
        
        cmd_file = os.path.join(workspace, "command.txt")
        if os.path.exists(cmd_file):
            try:
                with open(cmd_file, 'r') as f:
                    cmd = f.read().strip()
                os.remove(cmd_file)
                print(f"[DemoAgent] Command: {cmd}")
            except:
                pass
        
        time.sleep(2)
    
    print("[DemoAgent] Stopped")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["start", "stop", "scenario"])
    parser.add_argument("--workspace", default="agent_workspace")
    parser.add_argument("--model", default="phi3:mini")
    args = parser.parse_args()
    
    if args.command == "start":
        run_demo_agent(args.workspace)
    elif args.command == "stop":
        pid_file = os.path.join(args.workspace, "demo.pid")
        if os.path.exists(pid_file):
            with open(pid_file, 'r') as f:
                pid = int(f.read().strip())
            os.kill(pid, signal.SIGTERM)
            os.remove(pid_file)
        print('{"status": "stopped"}')
    elif args.command == "scenario":
        workspace_path = os.path.abspath(args.workspace)
        commands = ["Show all employees", "What is total sales?", "Calculate average salary", "List customers"]
        for cmd in commands:
            cmd_file = os.path.join(workspace_path, "command.txt")
            with open(cmd_file, 'w') as f:
                f.write(cmd)
            print(f"[Scenario] {cmd}")
            time.sleep(2)
        print('{"status": "completed", "actions": 4}')