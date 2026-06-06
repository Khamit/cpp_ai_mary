#!/usr/bin/env python3
"""
Web Interface for Neural Agent Audit System
Run: python3 web_interface.py --port 8080
"""

import json
import os
import time
import threading
import subprocess
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import argparse

# Конфигурация
WORKSPACE = os.environ.get('AGENT_WORKSPACE', 'agent_workspace')
AGENT_INFO_FILE = f"{WORKSPACE}/agent_info.json"

# HTML шаблон
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Neural Agent Audit System</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #e0e0e0;
            min-height: 100vh;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
        }
        
        /* Header */
        .header {
            background: rgba(0, 0, 0, 0.5);
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 20px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .header h1 {
            font-size: 28px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            -webkit-background-clip: text;
            background-clip: text;
            color: transparent;
            margin-bottom: 10px;
        }
        
        .header .subtitle {
            color: #888;
            font-size: 14px;
        }
        
        .status-bar {
            display: flex;
            gap: 20px;
            margin-top: 15px;
            flex-wrap: wrap;
        }
        
        .status-card {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 10px;
            padding: 10px 15px;
            border-left: 3px solid #667eea;
        }
        
        .status-label {
            font-size: 12px;
            color: #888;
            text-transform: uppercase;
        }
        
        .status-value {
            font-size: 24px;
            font-weight: bold;
            color: #fff;
        }
        
        /* Grid layout */
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        
        /* Cards */
        .card {
            background: rgba(0, 0, 0, 0.4);
            border-radius: 15px;
            padding: 20px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            transition: transform 0.2s, box-shadow 0.2s;
        }
        
        .card:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
        }
        
        .card-title {
            font-size: 18px;
            font-weight: 600;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .card-title .icon {
            font-size: 20px;
        }
        
        /* Metrics */
        .metric {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
        }
        
        .metric-name {
            color: #aaa;
        }
        
        .metric-value {
            font-weight: 500;
        }
        
        .metric-value.high {
            color: #ff6b6b;
        }
        
        .metric-value.medium {
            color: #ffd93d;
        }
        
        .metric-value.low {
            color: #6bcb77;
        }
        
        /* Progress bars */
        .progress-bar {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 10px;
            height: 8px;
            overflow: hidden;
            margin: 10px 0;
        }
        
        .progress-fill {
            height: 100%;
            transition: width 0.3s ease;
        }
        
        .progress-fill.risk {
            background: linear-gradient(90deg, #ff6b6b, #ffd93d);
        }
        
        .progress-fill.entropy {
            background: linear-gradient(90deg, #6bcb77, #ffd93d, #ff6b6b);
        }
        
        /* Controls */
        .control-group {
            margin-bottom: 15px;
        }
        
        .control-group label {
            display: flex;
            align-items: center;
            gap: 10px;
            cursor: pointer;
            padding: 8px;
            border-radius: 8px;
            transition: background 0.2s;
        }
        
        .control-group label:hover {
            background: rgba(255, 255, 255, 0.05);
        }
        
        input[type="checkbox"] {
            width: 20px;
            height: 20px;
            cursor: pointer;
            accent-color: #667eea;
        }
        
        select, input[type="number"] {
            background: rgba(0, 0, 0, 0.5);
            border: 1px solid rgba(255, 255, 255, 0.2);
            border-radius: 8px;
            padding: 8px 12px;
            color: #e0e0e0;
            width: 100%;
            margin-top: 5px;
        }
        
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            color: white;
            font-weight: 600;
            cursor: pointer;
            transition: opacity 0.2s;
            width: 100%;
        }
        
        button:hover {
            opacity: 0.9;
        }
        
        button.danger {
            background: linear-gradient(135deg, #ff6b6b 0%, #c92a2a 100%);
        }
        
        .logs-container {
            max-height: 300px;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            font-size: 12px;
        }
        
        .log-entry {
            padding: 5px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
            font-family: monospace;
        }
        
        .log-entry.warning {
            color: #ffd93d;
        }
        
        .log-entry.error {
            color: #ff6b6b;
        }
        
        .log-entry.success {
            color: #6bcb77;
        }
        
        .timestamp {
            color: #666;
            margin-right: 10px;
        }
        
        /* Charts */
        .chart-container {
            height: 200px;
            margin-top: 10px;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .danger-pulse {
            animation: pulse 1s infinite;
            color: #ff6b6b !important;
        }
        
        /* Footer */
        .footer {
            text-align: center;
            padding: 20px;
            color: #666;
            font-size: 12px;
        }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🧠 Neural Agent Audit System</h1>
            <div class="subtitle">Real-time monitoring & control for AI agents</div>
            <div class="status-bar">
                <div class="status-card">
                    <div class="status-label">Agent Status</div>
                    <div class="status-value" id="agent-status">🟢 Active</div>
                </div>
                <div class="status-card">
                    <div class="status-label">Session ID</div>
                    <div class="status-value" id="session-id">-</div>
                </div>
                <div class="status-card">
                    <div class="status-label">Total Steps</div>
                    <div class="status-value" id="total-steps">0</div>
                </div>
                <div class="status-card">
                    <div class="status-label">Uptime</div>
                    <div class="status-value" id="uptime">0s</div>
                </div>
            </div>
        </div>
        
        <div class="grid">
            <!-- Risk & Entropy -->
            <div class="card">
                <div class="card-title">
                    <span class="icon">📊</span>
                    <span>Risk & Entropy</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Hallucination Risk</span>
                    <span class="metric-value" id="risk-value">0%</span>
                </div>
                <div class="progress-bar">
                    <div class="progress-fill risk" id="risk-bar" style="width: 0%"></div>
                </div>
                <div class="metric">
                    <span class="metric-name">System Entropy</span>
                    <span class="metric-value" id="entropy-value">0%</span>
                </div>
                <div class="progress-bar">
                    <div class="progress-fill entropy" id="entropy-bar" style="width: 0%"></div>
                </div>
                <div class="metric">
                    <span class="metric-name">Quality</span>
                    <span class="metric-value" id="quality-value">0%</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Surprise</span>
                    <span class="metric-value" id="surprise-value">0%</span>
                </div>
            </div>
            
            <!-- Memory Stats -->
            <div class="card">
                <div class="card-title">
                    <span class="icon">💾</span>
                    <span>Memory</span>
                </div>
                <div class="metric">
                    <span class="metric-name">STM Usage</span>
                    <span class="metric-value" id="stm-usage">0/256</span>
                </div>
                <div class="progress-bar">
                    <div class="progress-fill" id="stm-bar" style="width: 0%; background: #667eea"></div>
                </div>
                <div class="metric">
                    <span class="metric-name">LTM Usage</span>
                    <span class="metric-value" id="ltm-usage">0/2048</span>
                </div>
                <div class="progress-bar">
                    <div class="progress-fill" id="ltm-bar" style="width: 0%; background: #764ba2"></div>
                </div>
                <div class="metric">
                    <span class="metric-name">LTM Avg Importance</span>
                    <span class="metric-value" id="ltm-importance">0%</span>
                </div>
            </div>
            
            <!-- Session Stats -->
            <div class="card">
                <div class="card-title">
                    <span class="icon">📈</span>
                    <span>Session Statistics</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Accumulated Risk</span>
                    <span class="metric-value" id="accumulated-risk">0</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Avg Entropy</span>
                    <span class="metric-value" id="avg-entropy">0%</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Successful Actions</span>
                    <span class="metric-value" id="successful">0</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Blocked Actions</span>
                    <span class="metric-value" id="blocked">0</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Dangerous Mode</span>
                    <span class="metric-value" id="danger-mode">❌ No</span>
                </div>
            </div>
        </div>
        
        <div class="grid">
            <!-- Constraints Control -->
            <div class="card">
                <div class="card-title">
                    <span class="icon">🔒</span>
                    <span>Safety Constraints</span>
                </div>
                <div class="control-group">
                    <label>
                        <input type="checkbox" id="constraint_300" onchange="setConstraint('response_length_300', this.checked)">
                        <span>Response limit 300 chars</span>
                    </label>
                </div>
                <div class="control-group">
                    <label>
                        <input type="checkbox" id="constraint_600" onchange="setConstraint('response_length_600', this.checked)">
                        <span>Response limit 600 chars</span>
                    </label>
                </div>
                <div class="control-group">
                    <label>
                        <input type="checkbox" id="constraint_c50" onchange="setConstraint('confidence_50', this.checked)">
                        <span>Min confidence 50% for logging</span>
                    </label>
                </div>
                <div class="control-group">
                    <label>
                        <input type="checkbox" id="constraint_c70" onchange="setConstraint('confidence_70', this.checked)">
                        <span>Min confidence 70% for logging</span>
                    </label>
                </div>
                <div class="control-group">
                    <label>
                        <input type="checkbox" id="constraint_maxsteps" onchange="setConstraint('max_steps_per_second', this.checked)">
                        <span>Limit steps per second</span>
                    </label>
                </div>
            </div>
            
            <!-- Control Panel -->
            <div class="card">
                <div class="card-title">
                    <span class="icon">🎮</span>
                    <span>Control Panel</span>
                </div>
                <div class="control-group">
                    <label>Session ID</label>
                    <input type="text" id="session-input" placeholder="session_001">
                </div>
                <div class="control-group">
                    <label>Agent Name</label>
                    <input type="text" id="agent-input" placeholder="phi3_mini">
                </div>
                <button onclick="startSession()">▶ Start Session</button>
                <button onclick="endSession()" class="danger" style="margin-top: 10px;">⏹ End Session</button>
                <button onclick="saveState()" style="margin-top: 10px;">💾 Save State</button>
                <button onclick="resetRisk()" style="margin-top: 10px;">🔄 Reset Risk</button>
            </div>
            
            <!-- 24h Stats -->
            <div class="card">
                <div class="card-title">
                    <span class="icon">📅</span>
                    <span>24h Statistics</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Actions (24h)</span>
                    <span class="metric-value" id="stats-total">0</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Success Rate</span>
                    <span class="metric-value" id="stats-success-rate">0%</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Blocked Rate</span>
                    <span class="metric-value" id="stats-blocked-rate">0%</span>
                </div>
                <div class="metric">
                    <span class="metric-name">Avg Risk (24h)</span>
                    <span class="metric-value" id="stats-avg-risk">0%</span>
                </div>
            </div>
        </div>
        
        <!-- Activity Chart -->
        <div class="card">
            <div class="card-title">
                <span class="icon">📉</span>
                <span>Risk History (last 100 steps)</span>
            </div>
            <canvas id="risk-chart" height="100"></canvas>
        </div>
        
        <!-- Logs -->
        <div class="card">
            <div class="card-title">
                <span class="icon">📝</span>
                <span>Live Logs</span>
                <button onclick="clearLogs()" style="width: auto; margin-left: auto; padding: 5px 10px;">Clear</button>
            </div>
            <div class="logs-container" id="logs">
                <div class="log-entry">Ready and waiting for agent...</div>
            </div>
        </div>
        
        <div class="footer">
            Neural Agent Audit System v2.0 | Real-time AI Agent Observability
        </div>
    </div>
    
    <script>
        let updateInterval;
        let riskChart;
        let riskHistory = [];
        
        // Initialize chart
        function initChart() {
            const ctx = document.getElementById('risk-chart').getContext('2d');
            riskChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Hallucination Risk',
                        data: [],
                        borderColor: '#ff6b6b',
                        backgroundColor: 'rgba(255, 107, 107, 0.1)',
                        tension: 0.3,
                        fill: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    scales: {
                        y: {
                            beginAtZero: true,
                            max: 1,
                            grid: { color: 'rgba(255,255,255,0.1)' }
                        },
                        x: {
                            grid: { color: 'rgba(255,255,255,0.1)' }
                        }
                    },
                    plugins: {
                        legend: { labels: { color: '#e0e0e0' } }
                    }
                }
            });
        }
        
        // Update UI with data
        function updateUI(data) {
            if (!data) return;
            
            // Risk & Entropy
            document.getElementById('risk-value').innerHTML = Math.round(data.risk * 100) + '%';
            document.getElementById('risk-bar').style.width = (data.risk * 100) + '%';
            document.getElementById('entropy-value').innerHTML = Math.round(data.entropy * 100) + '%';
            document.getElementById('entropy-bar').style.width = (data.entropy * 100) + '%';
            document.getElementById('quality-value').innerHTML = Math.round(data.quality * 100) + '%';
            document.getElementById('surprise-value').innerHTML = Math.round(data.surprise * 100) + '%';
            
            // Risk color
            let riskElem = document.getElementById('risk-value');
            if (data.risk > 0.7) riskElem.className = 'metric-value high';
            else if (data.risk > 0.4) riskElem.className = 'metric-value medium';
            else riskElem.className = 'metric-value low';
            
            // Session
            document.getElementById('session-id').innerHTML = data.session_id || '-';
            document.getElementById('total-steps').innerHTML = data.total_steps || 0;
            document.getElementById('accumulated-risk').innerHTML = data.accumulated_risk ? data.accumulated_risk.toFixed(2) : '0';
            document.getElementById('avg-entropy').innerHTML = data.avg_entropy ? Math.round(data.avg_entropy * 100) + '%' : '0%';
            document.getElementById('successful').innerHTML = data.successful_actions || 0;
            document.getElementById('blocked').innerHTML = data.blocked_actions || 0;
            
            let dangerMode = document.getElementById('danger-mode');
            if (data.is_dangerous_mode) {
                dangerMode.innerHTML = '⚠️ YES';
                dangerMode.className = 'metric-value danger-pulse';
            } else {
                dangerMode.innerHTML = '✅ No';
                dangerMode.className = 'metric-value';
            }
            
            // Memory
            document.getElementById('stm-usage').innerHTML = (data.stm_size || 0) + '/256';
            document.getElementById('stm-bar').style.width = ((data.stm_size || 0) / 256 * 100) + '%';
            document.getElementById('ltm-usage').innerHTML = (data.ltm_size || 0) + '/2048';
            document.getElementById('ltm-bar').style.width = ((data.ltm_size || 0) / 2048 * 100) + '%';
            document.getElementById('ltm-importance').innerHTML = data.ltm_avg_importance ? Math.round(data.ltm_avg_importance * 100) + '%' : '0%';
            
            // Risk history
            if (data.risk !== undefined) {
                riskHistory.push(data.risk);
                if (riskHistory.length > 100) riskHistory.shift();
                
                if (riskChart) {
                    riskChart.data.labels = riskHistory.map((_, i) => i);
                    riskChart.data.datasets[0].data = riskHistory;
                    riskChart.update();
                }
            }
            
            // Uptime
            if (data.start_time) {
                let uptime = Math.floor((Date.now() / 1000) - data.start_time);
                let hours = Math.floor(uptime / 3600);
                let minutes = Math.floor((uptime % 3600) / 60);
                let seconds = uptime % 60;
                document.getElementById('uptime').innerHTML = hours + 'h ' + minutes + 'm ' + seconds + 's';
            }
        }
        
        // Fetch status
        async function fetchStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                updateUI(data);
            } catch(e) {
                console.error('Fetch error:', e);
            }
        }
        
        // Fetch 24h stats
        async function fetchStats() {
            try {
                const response = await fetch('/api/stats/24h');
                const data = await response.json();
                
                document.getElementById('stats-total').innerHTML = data.total_actions || 0;
                let successRate = data.total_actions > 0 ? (data.successful_actions / data.total_actions * 100) : 0;
                let blockedRate = data.total_actions > 0 ? (data.blocked_actions / data.total_actions * 100) : 0;
                document.getElementById('stats-success-rate').innerHTML = Math.round(successRate) + '%';
                document.getElementById('stats-blocked-rate').innerHTML = Math.round(blockedRate) + '%';
                document.getElementById('stats-avg-risk').innerHTML = data.avg_hallucination_risk ? Math.round(data.avg_hallucination_risk * 100) + '%' : '0%';
            } catch(e) {
                console.error('Stats fetch error:', e);
            }
        }
        
        // Fetch constraints
        async function fetchConstraints() {
            try {
                const response = await fetch('/api/constraints');
                const data = await response.json();
                
                document.getElementById('constraint_300').checked = data.response_length_300 || false;
                document.getElementById('constraint_600').checked = data.response_length_600 || false;
                document.getElementById('constraint_c50').checked = data.confidence_50 || false;
                document.getElementById('constraint_c70').checked = data.confidence_70 || false;
                document.getElementById('constraint_maxsteps').checked = data.max_steps_per_second || false;
            } catch(e) {
                console.error('Constraints fetch error:', e);
            }
        }
        
        // Set constraint
        async function setConstraint(name, enabled) {
            try {
                await fetch('/api/constraint', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name: name, enabled: enabled })
                });
                addLog('info', `Constraint ${name} ${enabled ? 'enabled' : 'disabled'}`);
            } catch(e) {
                console.error('Set constraint error:', e);
            }
        }
        
        // Start session
        async function startSession() {
            let sessionId = document.getElementById('session-input').value || 'session_' + Date.now();
            let agentName = document.getElementById('agent-input').value || 'agent';
            
            try {
                await fetch('/api/session/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ session_id: sessionId, agent_name: agentName })
                });
                addLog('success', `Session started: ${sessionId} for ${agentName}`);
            } catch(e) {
                console.error('Start session error:', e);
            }
        }
        
        // End session
        async function endSession() {
            try {
                await fetch('/api/session/end', { method: 'POST' });
                addLog('warning', 'Session ended');
            } catch(e) {
                console.error('End session error:', e);
            }
        }
        
        // Save state
        async function saveState() {
            try {
                await fetch('/api/save', { method: 'POST' });
                addLog('success', 'State saved');
            } catch(e) {
                console.error('Save state error:', e);
            }
        }
        
        // Reset risk
        async function resetRisk() {
            try {
                await fetch('/api/reset_risk', { method: 'POST' });
                addLog('info', 'Risk accumulator reset');
            } catch(e) {
                console.error('Reset risk error:', e);
            }
        }
        
        // Add log entry
        function addLog(type, message) {
            let logsDiv = document.getElementById('logs');
            let logEntry = document.createElement('div');
            logEntry.className = 'log-entry ' + type;
            let timestamp = new Date().toLocaleTimeString();
            logEntry.innerHTML = `<span class="timestamp">[${timestamp}]</span> ${message}`;
            logsDiv.insertBefore(logEntry, logsDiv.firstChild);
            
            // Keep only last 100 logs
            while (logsDiv.children.length > 100) {
                logsDiv.removeChild(logsDiv.lastChild);
            }
        }
        
        // Clear logs
        function clearLogs() {
            document.getElementById('logs').innerHTML = '';
            addLog('info', 'Logs cleared');
        }
        
        // Start polling
        function startPolling() {
            fetchStatus();
            fetchStats();
            fetchConstraints();
            updateInterval = setInterval(() => {
                fetchStatus();
                fetchStats();
            }, 2000);
        }
        
        // Event source for real-time logs
        function setupEventSource() {
            const eventSource = new EventSource('/api/events');
            eventSource.onmessage = function(event) {
                const data = JSON.parse(event.data);
                addLog(data.type || 'info', data.message);
            };
            eventSource.onerror = function() {
                console.log('EventSource connection lost, will reconnect...');
                setTimeout(setupEventSource, 3000);
            };
        }
        
        // Initialize
        initChart();
        startPolling();
        setupEventSource();
    </script>
</body>
</html>
'''

class WebHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        
        if path == '/' or path == '/index.html':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(HTML_TEMPLATE.encode())
            
        elif path == '/api/status':
            self.send_json(self.get_status())
            
        elif path == '/api/stats/24h':
            self.send_json(self.get_stats_24h())
            
        elif path == '/api/constraints':
            self.send_json(self.get_constraints())
            
        elif path == '/api/events':
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            # Keep connection open for SSE
            while True:
                time.sleep(1)
                
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length) if content_length > 0 else b'{}'
        
        try:
            data = json.loads(body) if body else {}
        except:
            data = {}
        
        if self.path == '/api/constraint':
            name = data.get('name', '')
            enabled = data.get('enabled', False)
            self.set_constraint(name, enabled)
            self.send_json({'status': 'ok'})
            
        elif self.path == '/api/session/start':
            session_id = data.get('session_id', '')
            agent_name = data.get('agent_name', '')
            self.start_session(session_id, agent_name)
            self.send_json({'status': 'ok'})
            
        elif self.path == '/api/session/end':
            self.end_session()
            self.send_json({'status': 'ok'})
            
        elif self.path == '/api/save':
            self.save_state()
            self.send_json({'status': 'ok'})
            
        elif self.path == '/api/reset_risk':
            self.reset_risk()
            self.send_json({'status': 'ok'})
            
        else:
            self.send_response(404)
            self.end_headers()
    
    def send_json(self, data):
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())
    
    def get_status(self):
        # Чтение статуса из файла или через pipe
        status_file = f"{WORKSPACE}/agent_status.json"
        if os.path.exists(status_file):
            try:
                with open(status_file, 'r') as f:
                    return json.load(f)
            except:
                pass
        return {'status': 'waiting', 'risk': 0.0, 'entropy': 0.5, 'quality': 0.5, 'surprise': 0.0}
    
    def get_stats_24h(self):
        stats_file = f"{WORKSPACE}/logs/aggregated/24h.json"
        if os.path.exists(stats_file):
            try:
                with open(stats_file, 'r') as f:
                    return json.load(f)
            except:
                pass
        return {}
    
    def get_constraints(self):
        config_file = f"{WORKSPACE}/config/config.json"
        if os.path.exists(config_file):
            try:
                with open(config_file, 'r') as f:
                    config = json.load(f)
                    return config.get('constraints', {})
            except:
                pass
        return {}
    
    def set_constraint(self, name, enabled):
        config_file = f"{WORKSPACE}/config/config.json"
        config = {}
        if os.path.exists(config_file):
            try:
                with open(config_file, 'r') as f:
                    config = json.load(f)
            except:
                pass
        
        if 'constraints' not in config:
            config['constraints'] = {}
        config['constraints'][name] = enabled
        
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=4)
        
        # Notify C++ process
        notify_file = f"{WORKSPACE}/config/notify.txt"
        with open(notify_file, 'w') as f:
            f.write(f"reload_constraints\n")
    
    def start_session(self, session_id, agent_name):
        cmd_file = f"{WORKSPACE}/config/command.txt"
        with open(cmd_file, 'w') as f:
            f.write(f"start_session {session_id} {agent_name}\n")
    
    def end_session(self):
        cmd_file = f"{WORKSPACE}/config/command.txt"
        with open(cmd_file, 'w') as f:
            f.write("end_session\n")
    
    def save_state(self):
        cmd_file = f"{WORKSPACE}/config/command.txt"
        with open(cmd_file, 'w') as f:
            f.write("save_state\n")
    
    def reset_risk(self):
        cmd_file = f"{WORKSPACE}/config/command.txt"
        with open(cmd_file, 'w') as f:
            f.write("reset_risk\n")
    
    def log_message(self, format, *args):
        # Подавляем стандартные логи HTTP сервера
        pass

def main():
    parser = argparse.ArgumentParser(description='Neural Agent Audit Web Interface')
    parser.add_argument('--port', type=int, default=8080, help='Web server port')
    parser.add_argument('--workspace', type=str, default='agent_workspace', help='Working directory')
    args = parser.parse_args()
    
    global WORKSPACE
    WORKSPACE = args.workspace
    
    # Создаём необходимые директории
    os.makedirs(f"{WORKSPACE}/logs/raw", exist_ok=True)
    os.makedirs(f"{WORKSPACE}/logs/aggregated", exist_ok=True)
    os.makedirs(f"{WORKSPACE}/config", exist_ok=True)
    os.makedirs(f"{WORKSPACE}/memory", exist_ok=True)
    
    # Запускаем сервер
    server = HTTPServer(('0.0.0.0', args.port), WebHandler)
    print(f"\n🚀 Web Interface started at http://localhost:{args.port}")
    print(f"📁 Workspace: {WORKSPACE}")
    print(f"Press Ctrl+C to stop\n")
    
    try:
        server.serve_forever() 
    except KeyboardInterrupt:
        print("\nShutting down web interface...")
        server.shutdown()

if __name__ == '__main__':
    main()