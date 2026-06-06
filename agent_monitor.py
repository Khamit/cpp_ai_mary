#!/usr/bin/env python3
"""
Agent Monitor - CLI version для отображения статуса в терминале
"""

import json
import time
import os
import sys
from datetime import datetime

WORKSPACE = os.environ.get('AGENT_WORKSPACE', 'agent_workspace')

def clear_screen():
    os.system('clear' if os.name == 'posix' else 'cls')

def format_percent(value):
    return f"{value * 100:.1f}%" if value else "0%"

def main():
    while True:
        clear_screen()
        
        print("""
╔══════════════════════════════════════════════════════════════════════╗
║                    Neural Agent Audit Monitor                        ║
╚══════════════════════════════════════════════════════════════════════╝
        """)
        
        # Чтение статуса
        status_file = f"{WORKSPACE}/agent_status.json"
        if os.path.exists(status_file):
            try:
                with open(status_file, 'r') as f:
                    status = json.load(f)
                    
                print(f"📊 Session: {status.get('session_id', '-')}")
                print(f"📈 Steps: {status.get('total_steps', 0)}")
                print(f"🎯 Risk: {format_percent(status.get('risk', 0))}")
                print(f"🌀 Entropy: {format_percent(status.get('entropy', 0.5))}")
                print(f"⭐ Quality: {format_percent(status.get('quality', 0.5))}")
                print(f"⚠️ Danger Mode: {'YES' if status.get('is_dangerous_mode') else 'NO'}")
                
            except Exception as e:
                print(f"Error reading status: {e}")
        else:
            print("⏳ Waiting for agent...")
        
        # 24h статистика
        stats_file = f"{WORKSPACE}/logs/aggregated/24h.json"
        if os.path.exists(stats_file):
            try:
                with open(stats_file, 'r') as f:
                    stats = json.load(f)
                    
                print(f"\n📅 Last 24h:")
                print(f"   Actions: {stats.get('total_actions', 0)}")
                print(f"   Success rate: {stats.get('successful_actions', 0) / max(1, stats.get('total_actions', 1)) * 100:.1f}%")
                print(f"   Avg risk: {format_percent(stats.get('avg_hallucination_risk', 0))}")
                
            except:
                pass
        
        print(f"\n🕐 Last update: {datetime.now().strftime('%H:%M:%S')}")
        print("\nPress Ctrl+C to exit")
        
        time.sleep(2)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\nGoodbye!")