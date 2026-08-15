#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <httpUpdate.h>
#include <Preferences.h>

// Firmware version for OTA auto-updates
const String FIRMWARE_VERSION = "1.0.0";

// ==========================================
// 1. WI-FI CONFIGURATION
// ==========================================
// Enter your home Wi-Fi credentials here:
// ==========================================
// 1. WI-FI CONFIGURATION (DYNAMIC VIA PORTAL)
// ==========================================
String wifi_ssid = "";
String wifi_password = "";

// Fallback AP settings for initial configuration
const char* setup_ap_ssid = "Smart-Home-Setup";

// Static IP Configuration (Optional fallback):
IPAddress local_IP(192, 168, 100, 200); 
IPAddress gateway(192, 168, 100, 1);    
IPAddress subnet(255, 255, 255, 0);      
IPAddress primaryDNS(192, 168, 100, 1); 
IPAddress secondaryDNS(8, 8, 8, 8);    

// Backup fallback AP for offline control if connection fails later
const char* ap_ssid     = "Smart-Home-Hub";
const char* ap_password = "password123";

// Create WebServer object on port 80
WebServer server(80);

// ==========================================
// 2. HARDWARE RELAY CONFIGURATION
// ==========================================
// 30-Pin ESP32 safe output pins:
// IN1 -> GPIO 25 (Relay 1)
// IN2 -> GPIO 26 (Relay 2)
// IN3 -> GPIO 27 (Relay 3)
// IN4 -> GPIO 32 (Relay 4)
const int RELAY_PINS[4] = {25, 26, 27, 32};

// Relay states (0 = OFF, 1 = ON)
// In a low-level trigger relay:
// State 1 (ON)  -> Write LOW to pin
// State 0 (OFF) -> Write HIGH to pin
int relayStates[4] = {0, 0, 0, 0};

// ==========================================
// 3. EMBEDDED DASHBOARD PAGE (HTML/CSS/JS)
// ==========================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>El-Morsy Smart Home</title>
    <style>
        :root {
            --bg-color: #0b0f19;
            --panel-bg: rgba(17, 24, 39, 0.7);
            --panel-border: rgba(255, 255, 255, 0.08);
            --text-primary: #f3f4f6;
            --text-secondary: #9ca3af;
            --accent-glow: rgba(99, 102, 241, 0.15);
            --accent-color: #6366f1;
            --accent-active: #818cf8;
            --danger-glow: rgba(239, 68, 68, 0.15);
            --danger-color: #ef4444;
            --success-color: #10b981;
            
            /* Card Glows */
            --glow-r1: 0 0 20px rgba(168, 85, 247, 0.2);
            --color-r1: #a855f7;
            --glow-r2: 0 0 20px rgba(236, 72, 153, 0.2);
            --color-r2: #ec4899;
            --glow-r3: 0 0 20px rgba(6, 182, 212, 0.2);
            --color-r3: #06b6d4;
            --glow-r4: 0 0 20px rgba(16, 185, 129, 0.2);
            --color-r4: #10b981;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 0% 0%, rgba(99, 102, 241, 0.1) 0px, transparent 50%),
                radial-gradient(at 100% 0%, rgba(236, 72, 153, 0.08) 0px, transparent 50%),
                radial-gradient(at 50% 100%, rgba(6, 182, 212, 0.08) 0px, transparent 50%);
            background-attachment: fixed;
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 2rem 1rem;
        }

        .container {
            width: 100%;
            max-width: 800px;
            display: flex;
            flex-direction: column;
            gap: 2rem;
        }

        /* Header Styles */
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1.5rem;
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            border-radius: 20px;
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.2);
            gap: 15px;
        }

        .logo-section h1 {
            font-size: 1.4rem;
            font-weight: 700;
            background: linear-gradient(135deg, #a855f7, #6366f1, #06b6d4);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            letter-spacing: -0.5px;
        }

        .logo-section p {
            font-size: 0.8rem;
            color: var(--text-secondary);
            margin-top: 2px;
        }

        /* Right Header Elements */
        .header-actions {
            display: flex;
            align-items: center;
            gap: 12px;
        }

        /* Voice Panel */
        .voice-section {
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .btn-mic {
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid var(--panel-border);
            color: var(--text-secondary);
            width: 40px;
            height: 40px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            outline: none;
        }

        .btn-mic:hover {
            background: rgba(255, 255, 255, 0.1);
            color: var(--text-primary);
            transform: scale(1.08);
        }

        /* Standby state: Listening for Wake Word */
        .btn-mic.standby {
            background: rgba(99, 102, 241, 0.15);
            border-color: var(--accent-color);
            color: var(--accent-color);
            animation: pulse-blue 2s infinite;
        }

        /* Active state: Processing Command */
        .btn-mic.listening {
            background: rgba(239, 68, 68, 0.2);
            border-color: var(--danger-color);
            color: var(--danger-color);
            animation: pulse-red 1s infinite;
            transform: scale(1.08);
        }

        @keyframes pulse-blue {
            0% { box-shadow: 0 0 0 0 rgba(99, 102, 241, 0.4); }
            70% { box-shadow: 0 0 0 8px rgba(99, 102, 241, 0); }
            100% { box-shadow: 0 0 0 0 rgba(99, 102, 241, 0); }
        }

        @keyframes pulse-red {
            0% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0.5); }
            70% { box-shadow: 0 0 0 10px rgba(239, 68, 68, 0); }
            100% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0); }
        }

        #voiceStatus {
            font-size: 0.75rem;
            font-weight: 600;
            color: var(--text-secondary);
            max-width: 150px;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            display: none;
        }

        .status-badge {
            display: flex;
            align-items: center;
            gap: 6px;
            background: rgba(255, 255, 255, 0.04);
            padding: 6px 12px;
            border-radius: 30px;
            font-size: 0.8rem;
            border: 1px solid rgba(255, 255, 255, 0.05);
            white-space: nowrap;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--danger-color);
            box-shadow: 0 0 10px var(--danger-color);
            transition: all 0.3s ease;
        }

        .status-dot.online {
            background-color: var(--success-color);
            box-shadow: 0 0 10px var(--success-color);
        }

        /* Language Toggle Button */
        .btn-lang {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--panel-border);
            color: var(--text-primary);
            padding: 6px 12px;
            font-size: 0.8rem;
            font-weight: 600;
            border-radius: 12px;
            cursor: pointer;
            transition: all 0.25s;
            height: 36px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .btn-lang:hover {
            background: rgba(255, 255, 255, 0.12);
            border-color: rgba(255, 255, 255, 0.2);
        }

        /* Quick Controls */
        .quick-actions {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 1rem;
        }

        .btn {
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            color: var(--text-primary);
            padding: 1rem;
            border-radius: 16px;
            font-weight: 600;
            font-size: 0.95rem;
            cursor: pointer;
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }

        .btn:hover {
            background: rgba(255, 255, 255, 0.05);
            transform: translateY(-2px);
        }

        .btn:active {
            transform: translateY(0);
        }

        .btn-all-on:hover {
            border-color: rgba(99, 102, 241, 0.4);
            box-shadow: var(--accent-glow);
        }

        .btn-all-off:hover {
            border-color: rgba(239, 68, 68, 0.4);
            box-shadow: var(--danger-glow);
        }

        /* Relay Card Grid */
        .relay-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
            gap: 1.25rem;
        }

        .relay-card {
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            border-radius: 24px;
            padding: 1.5rem;
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            height: 180px;
            position: relative;
            overflow: hidden;
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.15);
        }

        /* Ambient glows behind cards when active */
        .relay-card::before {
            content: '';
            position: absolute;
            top: -50%;
            left: -50%;
            width: 200%;
            height: 200%;
            background: radial-gradient(circle, var(--card-glow-color, transparent) 0%, transparent 70%);
            opacity: 0;
            transition: opacity 0.5s ease;
            pointer-events: none;
            z-index: 0;
        }

        .relay-card.active::before {
            opacity: 0.15;
        }

        .relay-card.r1 { --card-glow-color: var(--color-r1); --active-border: var(--color-r1); }
        .relay-card.r2 { --card-glow-color: var(--color-r2); --active-border: var(--color-r2); }
        .relay-card.r3 { --card-glow-color: var(--color-r3); --active-border: var(--color-r3); }
        .relay-card.r4 { --card-glow-color: var(--color-r4); --active-border: var(--color-r4); }

        .relay-card.active {
            border-color: var(--active-border);
            transform: translateY(-4px);
        }

        .relay-card.active.r1 { box-shadow: var(--glow-r1); }
        .relay-card.active.r2 { box-shadow: var(--glow-r2); }
        .relay-card.active.r3 { box-shadow: var(--glow-r3); }
        .relay-card.active.r4 { box-shadow: var(--glow-r4); }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            z-index: 1;
        }

        .icon-wrapper {
            width: 44px;
            height: 44px;
            border-radius: 12px;
            background: rgba(255, 255, 255, 0.04);
            display: flex;
            align-items: center;
            justify-content: center;
            border: 1px solid rgba(255, 255, 255, 0.05);
            transition: all 0.3s ease;
            color: var(--text-secondary);
        }

        .relay-card.active .icon-wrapper {
            background: rgba(255, 255, 255, 0.1);
            color: var(--text-primary);
        }

        /* Toggle Switch */
        .switch {
            position: relative;
            display: inline-block;
            width: 48px;
            height: 26px;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: rgba(255, 255, 255, 0.08);
            transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
            border-radius: 34px;
            border: 1px solid rgba(255, 255, 255, 0.05);
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 3px;
            bottom: 3px;
            background-color: #d1d5db;
            transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
            border-radius: 50%;
        }

        input:checked + .slider {
            background-color: var(--active-border);
        }

        input:checked + .slider:before {
            transform: translateX(22px);
            background-color: #ffffff;
        }

        .card-body {
            z-index: 1;
        }

        .relay-name {
            font-size: 1.05rem;
            font-weight: 600;
            margin-bottom: 4px;
            letter-spacing: -0.2px;
        }

        .relay-status-text {
            font-size: 0.8rem;
            color: var(--text-secondary);
            text-transform: uppercase;
            font-weight: 700;
            letter-spacing: 0.5px;
            transition: color 0.3s ease;
        }

        .relay-card.active .relay-status-text {
            color: #ffffff;
        }

        /* Metadata & Config Panel */
        .info-panel {
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            border-radius: 20px;
            padding: 1.25rem 1.5rem;
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            display: flex;
            flex-direction: column;
            gap: 0.75rem;
        }

        .info-row {
            display: flex;
            justify-content: space-between;
            font-size: 0.85rem;
            border-bottom: 1px solid rgba(255, 255, 255, 0.04);
            padding-bottom: 0.50rem;
        }

        .info-row:last-child {
            border-bottom: none;
            padding-bottom: 0;
        }

        .info-label {
            color: var(--text-secondary);
        }

        .info-value {
            font-weight: 600;
        }

        /* IP Configuration Settings */
        .config-trigger {
            font-size: 0.75rem;
            color: var(--text-secondary);
            cursor: pointer;
            text-decoration: underline;
            background: none;
            border: none;
            align-self: flex-start;
            margin-top: 4px;
        }
        
        .config-trigger:hover {
            color: var(--accent-color);
        }

        /* Update Banner Styling */
        .update-banner {
            background: rgba(99, 102, 241, 0.12);
            border: 1px solid rgba(99, 102, 241, 0.3);
            border-radius: 20px;
            padding: 1rem 1.25rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 15px;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            margin-bottom: 0.5rem;
        }

        .update-content {
            display: flex;
            align-items: center;
            gap: 10px;
            font-size: 0.85rem;
            font-weight: 500;
        }

        .update-icon {
            color: var(--accent-color);
            animation: pulse-blue 2s infinite;
            flex-shrink: 0;
        }

        .btn-update {
            background: var(--accent-color);
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 10px;
            font-weight: 600;
            cursor: pointer;
            font-size: 0.8rem;
            transition: all 0.3s;
            white-space: nowrap;
        }

        .btn-update:hover {
            background: var(--accent-active);
            transform: translateY(-1px);
        }

        .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.7);
            backdrop-filter: blur(8px);
            -webkit-backdrop-filter: blur(8px);
            justify-content: center;
            align-items: center;
            z-index: 10;
            padding: 1rem;
        }

        .modal.open {
            display: flex;
        }

        .modal-content {
            background: #111827;
            border: 1px solid var(--panel-border);
            border-radius: 24px;
            padding: 2rem;
            width: 100%;
            max-width: 400px;
            display: flex;
            flex-direction: column;
            gap: 1.25rem;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.5);
        }

        .modal-content h3 {
            font-size: 1.25rem;
            font-weight: 700;
        }

        .modal-content p {
            font-size: 0.85rem;
            color: var(--text-secondary);
        }

        .modal-content input {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--panel-border);
            border-radius: 12px;
            padding: 0.75rem 1rem;
            color: white;
            font-size: 0.95rem;
            outline: none;
            width: 100%;
        }

        .modal-content input:focus {
            border-color: var(--accent-color);
            box-shadow: var(--accent-glow);
        }

        .modal-buttons {
            display: flex;
            gap: 0.75rem;
        }

        .modal-buttons .btn {
            flex: 1;
            padding: 0.75rem;
        }

        .btn-primary {
            background: var(--accent-color);
            border-color: var(--accent-color);
        }

        .btn-primary:hover {
            background: var(--accent-active);
        }

        /* SVG Icons Styling */
        svg.icon {
            width: 24px;
            height: 24px;
            fill: none;
            stroke: currentColor;
            stroke-width: 2;
            stroke-linecap: round;
            stroke-linejoin: round;
        }
    </style>
</head>
<body>

<div class="container">
    <!-- Header -->
    <header>
        <div class="logo-section">
            <h1 id="uiTitle">El-Morsy Smart Home</h1>
        </div>
        <div class="header-actions">
            <!-- Language Toggle -->
            <button class="btn-lang" id="langToggleBtn" onclick="toggleLanguage()">عربي</button>
            
            <!-- Voice Panel -->
            <div class="voice-section">
                <button class="btn-mic" id="micBtn" onclick="toggleVoiceControl()" title="Click to speak (e.g. 'Turn on Light 1')">
                    <svg class="icon" viewBox="0 0 24 24">
                        <path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z"/>
                        <path d="M19 10v1a7 7 0 0 1-14 0v-1M12 19v3M8 22h8"/>
                    </svg>
                </button>
                <span id="voiceStatus">Listening...</span>
            </div>
            
            <!-- Status Badge -->
            <div class="status-badge">
                <span class="status-dot" id="connectionDot"></span>
                <span id="connectionText">Connecting...</span>
            </div>
        </div>
    </header>

    <!-- Update Alert Banner -->
    <div id="updateBanner" class="update-banner" style="display: none;">
        <div class="update-content">
            <svg class="icon update-icon" viewBox="0 0 24 24" style="width: 20px; height: 20px;">
                <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0zM12 9v4M12 17h.01"/>
            </svg>
            <span id="updateBannerText">يتوفر تحديث جديد للبرنامج (إصدار 1.1.0)</span>
        </div>
        <button class="btn-update" id="btnTriggerUpdate" onclick="startSystemUpdate()">تحديث الآن</button>
    </div>

    <!-- Quick Master Actions -->
    <div class="quick-actions">
        <button class="btn btn-all-on" onclick="setAllRelays(1)">
            <svg class="icon" viewBox="0 0 24 24"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
            <span id="uiAllOnText">All ON</span>
        </button>
        <button class="btn btn-all-off" onclick="setAllRelays(0)">
            <svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><path d="m4.93 4.93 14.14 14.14"/></svg>
            <span id="uiAllOffText">All OFF</span>
        </button>
    </div>

    <!-- Relay Grid -->
    <div class="relay-grid">
        <!-- Relay 1 -->
        <div class="relay-card r1" id="card1">
            <div class="card-header">
                <div class="icon-wrapper">
                    <!-- Light Bulb Icon -->
                    <svg class="icon" viewBox="0 0 24 24">
                        <path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A5.5 5.5 0 0 0 7.5 8c0 1.3.5 2.6 1.5 3.5.8.8 1.3 1.5 1.5 2.5"/>
                        <path d="M9 18h6M10 22h4"/>
                    </svg>
                </div>
                <label class="switch">
                    <input type="checkbox" id="switch1" onchange="toggleRelay(1, this.checked)">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="card-body">
                <div class="relay-name" id="uiRelayName1">Light 1</div>
                <div class="relay-status-text" id="status1">OFF</div>
            </div>
        </div>

        <!-- Relay 2 -->
        <div class="relay-card r2" id="card2">
            <div class="card-header">
                <div class="icon-wrapper">
                    <!-- Socket/Plug Icon -->
                    <svg class="icon" viewBox="0 0 24 24">
                        <path d="M12 2v5M6 7h12v10a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2zM9 11v3M15 11v3"/>
                    </svg>
                </div>
                <label class="switch">
                    <input type="checkbox" id="switch2" onchange="toggleRelay(2, this.checked)">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="card-body">
                <div class="relay-name" id="uiRelayName2">Socket 2</div>
                <div class="relay-status-text" id="status2">OFF</div>
            </div>
        </div>

        <!-- Relay 3 -->
        <div class="relay-card r3" id="card3">
            <div class="card-header">
                <div class="icon-wrapper">
                    <!-- Fan/Wind Icon -->
                    <svg class="icon" viewBox="0 0 24 24">
                        <path d="M12.82 2.82a1 1 0 0 1 1.03.11l2.42 1.94c.55.44.82 1.15.7 1.84L16.5 10c.87-.2 1.83.1 2.5.82a2.5 2.5 0 0 1 0 3.54c-.67.72-1.63 1.02-2.5.82l.47 3.29c.12.69-.15 1.4-.7 1.84l-2.42 1.94a1 1 0 0 1-1.25-.13 3 3 0 0 1-.92-2.3v-4.14c-1.32.32-2.8-.23-3.66-1.5a3.5 3.5 0 0 1 0-4.3c.86-1.27 2.34-1.82 3.66-1.5V5.12a3 3 0 0 1 .92-2.3z"/>
                        <circle cx="12" cy="12" r="1"/>
                    </svg>
                </div>
                <label class="switch">
                    <input type="checkbox" id="switch3" onchange="toggleRelay(3, this.checked)">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="card-body">
                <div class="relay-name" id="uiRelayName3">Fan 3</div>
                <div class="relay-status-text" id="status3">OFF</div>
            </div>
        </div>

        <!-- Relay 4 -->
        <div class="relay-card r4" id="card4">
            <div class="card-header">
                <div class="icon-wrapper">
                    <!-- TV/Monitor Icon -->
                    <svg class="icon" viewBox="0 0 24 24">
                        <rect x="2" y="3" width="20" height="14" rx="2" ry="2"/>
                        <line x1="8" y1="21" x2="16" y2="21"/>
                        <line x1="12" y1="17" x2="12" y2="21"/>
                    </svg>
                </div>
                <label class="switch">
                    <input type="checkbox" id="switch4" onchange="toggleRelay(4, this.checked)">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="card-body">
                <div class="relay-name" id="uiRelayName4">TV 4</div>
                <div class="relay-status-text" id="status4">OFF</div>
            </div>
        </div>
    </div>

    <!-- Diagnostic and Network Info -->
    <div class="info-panel">
        <div class="info-row">
            <span class="info-label" id="uiInfoRssiLabel">Signal Strength (RSSI)</span>
            <span class="info-value" id="infoRssi">N/A</span>
        </div>
        <div class="info-row">
            <span class="info-label" id="uiInfoUptimeLabel">ESP32 Uptime</span>
            <span class="info-value" id="infoUptime">N/A</span>
        </div>
        <div class="info-row">
            <span class="info-label" id="uiInfoTargetIpLabel">Target Controller IP</span>
            <span class="info-value" id="infoTargetIp">Auto (Local)</span>
        </div>
        <button class="config-trigger" id="uiConfigIpLink" onclick="openConfigModal()">Configure Controller Target IP</button>
    </div>
    
    <!-- Footer -->
    <footer style="text-align: center; margin-top: 1rem; font-size: 0.8rem; color: var(--text-secondary); display: flex; flex-direction: column; gap: 6px; z-index: 1;">
        <p style="font-weight: 600;">El-Morsy</p>
        <p>© 2026 Smart Home Hub. All rights reserved.</p>
    </footer>
</div>

<!-- Configuration Modal -->
<div class="modal" id="configModal">
    <div class="modal-content">
        <h3 id="uiModalTitle">Target Controller IP</h3>
        <p id="uiModalDesc">Enter the IP address of your ESP32 if you are running this webpage locally.</p>
        <input type="text" id="ipInput" placeholder="e.g. 192.168.100.200" style="direction: ltr;">
        <div class="modal-buttons">
            <button class="btn" id="uiModalCancel" onclick="closeConfigModal()">Cancel</button>
            <button class="btn btn-primary" id="uiModalSave" onclick="saveConfigIp()">Save Settings</button>
        </div>
        <hr style="border: 0; border-top: 1px solid var(--panel-border); margin: 0.25rem 0;">
        <button class="btn" id="btnWifiReset" onclick="resetWifiSettings()" style="background: rgba(239, 68, 68, 0.08); border-color: rgba(239, 68, 68, 0.15); color: var(--danger-color);">إعادة ضبط إعدادات الواي فاي</button>
    </div>
</div>

<!-- Update Progress Modal -->
<div class="modal" id="updateProgressModal">
    <div class="modal-content" style="align-items: center; text-align: center;">
        <h3 id="uiUpdateProgressTitle" style="background: linear-gradient(135deg, #a855f7, #6366f1); -webkit-background-clip: text; -webkit-text-fill-color: transparent;">جاري تحديث النظام...</h3>
        <p id="uiUpdateProgressDesc" style="font-size: 0.85rem; color: var(--text-secondary);">برجاء عدم إيقاف تشغيل الجهاز أو فصل الكهرباء أثناء التحديث.</p>
        <div style="margin-top: 1rem; width: 100%;">
            <div style="font-size: 0.85rem; color: var(--text-secondary); margin-bottom: 8px; display: flex; justify-content: space-between; direction: rtl;">
                <span id="updateProgressText">جاري التنزيل...</span>
                <span id="updateProgressPercent" style="direction: ltr;">0%</span>
            </div>
            <div style="background: rgba(255, 255, 255, 0.08); border-radius: 10px; height: 10px; overflow: hidden; width: 100%; border: 1px solid var(--panel-border);">
                <div id="updateProgressBar" style="background: var(--accent-color); width: 0%; height: 100%; transition: width 0.25s; box-shadow: 0 0 10px var(--accent-color);"></div>
            </div>
        </div>
    </div>
</div>

<script>
    // Translation dictionary
    const translations = {
        en: {
            title: "El-Morsy Smart Home",
            connecting: "Connecting...",
            connected: "Connected",
            disconnected: "Disconnected",
            listening: "Listening...",
            micTitle: "Click to speak (e.g. 'Turn on Light 1')",
            allOn: "All ON",
            allOff: "All OFF",
            relay1Name: "Light 1",
            relay2Name: "Socket 2",
            relay3Name: "Fan 3",
            relay4Name: "TV 4",
            stateOn: "ON",
            stateOff: "OFF",
            signal: "Signal Strength (RSSI)",
            uptime: "ESP32 Uptime",
            targetIp: "Target Controller IP",
            autoLocal: "Auto (Local)",
            configIpLink: "Configure Controller Target IP",
            modalTitle: "Target Controller IP",
            modalDesc: "Enter the IP address of your ESP32 if you are running this webpage locally.",
            cancel: "Cancel",
            save: "Save Settings",
            alertSpeechNotSupported: "Speech Recognition is not supported in this browser. Try Google Chrome or Microsoft Edge.",
            speakSuccessAllOn: "Turning all switches on",
            speakSuccessAllOff: "Turning all switches off",
            speakActionOn: "Turning",
            speakActionOff: "Turning",
            speakActionOnState: "on",
            speakActionOffState: "off",
            speakError: "Command not recognized. Try saying turn on light one, or turn off all"
        },
        ar: {
            title: "El-Morsy Smart Home",
            connecting: "جاري الاتصال...",
            connected: "متصل",
            disconnected: "غير متصل",
            listening: "تحدث الآن...",
            micTitle: "اضغط للتحدث (مثال: 'شغل اللمبة الأولى')",
            allOn: "تشغيل الكل",
            allOff: "إيقاف الكل",
            relay1Name: "اللمبة الأولى",
            relay2Name: "المقبس الثاني",
            relay3Name: "المروحة الثالثة",
            relay4Name: "الشاشة الرابعة",
            stateOn: "مفتوح (تشغيل)",
            stateOff: "مغلق (إيقاف)",
            signal: "قوة الإشارة (RSSI)",
            uptime: "وقت التشغيل",
            targetIp: "عنوان الـ IP للـ ESP32",
            autoLocal: "تلقائي (محلي)",
            configIpLink: "تعديل عنوان الـ IP للـ ESP32",
            modalTitle: "تعديل عنوان الـ IP",
            modalDesc: "أدخل عنوان الـ IP الخاص بـ ESP32 إذا كنت تتصفح هذه الصفحة محلياً من الكمبيوتر.",
            cancel: "إلغاء",
            save: "حفظ الإعدادات",
            alertSpeechNotSupported: "خاصية التعرف على الصوت غير مدعومة في متصفحك الحالي. يرجى استخدام Google Chrome أو Microsoft Edge.",
            speakSuccessAllOn: "تم تشغيل جميع المفاتيح",
            speakSuccessAllOff: "تم إيقاف جميع المفاتيح",
            speakActionOn: "تم تشغيل",
            speakActionOff: "تم إيقاف",
            speakActionOnState: "",
            speakActionOffState: "",
            speakError: "عذراً، لم أفهم الأمر الصوتي بشكل صحيح. يرجى تكراره."
        }
    };

    // State management
    let currentLang = localStorage.getItem('esp32_lang') || 'en';
    let targetIp = localStorage.getItem('esp32_target_ip') || '';
    let pollInterval = null;
    let recognition = null;
    let isListening = false;

    // Language switching function
    function setLanguage(lang) {
        currentLang = lang;
        localStorage.setItem('esp32_lang', lang);
        
        // Update DOM attributes (FORCE LTR)
        document.documentElement.lang = lang;
        document.documentElement.dir = 'ltr';
        
        // Update texts
        const t = translations[lang];
        document.getElementById('uiTitle').textContent = t.title;
        document.getElementById('uiAllOnText').textContent = t.allOn;
        document.getElementById('uiAllOffText').textContent = t.allOff;
        
        document.getElementById('uiRelayName1').textContent = t.relay1Name;
        document.getElementById('uiRelayName2').textContent = t.relay2Name;
        document.getElementById('uiRelayName3').textContent = t.relay3Name;
        document.getElementById('uiRelayName4').textContent = t.relay4Name;
        
        document.getElementById('uiInfoRssiLabel').textContent = t.signal;
        document.getElementById('uiInfoUptimeLabel').textContent = t.uptime;
        document.getElementById('uiInfoTargetIpLabel').textContent = t.targetIp;
        document.getElementById('uiConfigIpLink').textContent = t.configIpLink;
        
        document.getElementById('uiModalTitle').textContent = t.modalTitle;
        document.getElementById('uiModalDesc').textContent = t.modalDesc;
        document.getElementById('uiModalCancel').textContent = t.cancel;
        document.getElementById('uiModalSave').textContent = t.save;
        document.getElementById('btnWifiReset').textContent = lang === 'en' ? 'Reset Wi-Fi Settings' : 'إعادة ضبط إعدادات الواي فاي';
        
        document.getElementById('micBtn').title = t.micTitle;
        
        // Update language toggle button text
        document.getElementById('langToggleBtn').textContent = lang === 'en' ? 'عربي' : 'English';
        
        // Update voice status text if visible
        const vStatus = document.getElementById('voiceStatus');
        if (vStatus.style.display !== 'none') {
            vStatus.textContent = t.listening;
        }
        
        // Update Speech Recognition lang
        if (recognition) {
            recognition.lang = lang === 'en' ? 'en-US' : 'ar-SA';
        }
        
        // Re-sync states to refresh ON/OFF labels in the new language
        fetchStatus();
    }

    function toggleLanguage() {
        setLanguage(currentLang === 'en' ? 'ar' : 'en');
    }

    // Speech Recognition Setup
    if ('webkitSpeechRecognition' in window || 'SpeechRecognition' in window) {
        const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
        recognition = new SpeechRecognition();
        recognition.continuous = false;
        recognition.interimResults = false;
        recognition.lang = currentLang === 'en' ? 'en-US' : 'ar-SA';

        recognition.onstart = () => {
            isListening = true;
            document.getElementById('micBtn').className = 'btn-mic listening';
            const vStatus = document.getElementById('voiceStatus');
            vStatus.textContent = translations[currentLang].listening;
            vStatus.style.display = 'inline-block';
        };

        recognition.onend = () => {
            isListening = false;
            document.getElementById('micBtn').className = 'btn-mic';
            document.getElementById('voiceStatus').style.display = 'none';
        };

        recognition.onresult = (event) => {
            const transcript = event.results[0][0].transcript.toLowerCase().trim();
            console.log('Recognized speech:', transcript);
            
            const vStatus = document.getElementById('voiceStatus');
            vStatus.textContent = currentLang === 'en' ? `Heard: "${transcript}"` : `سمعت: "${transcript}"`;
            
            processVoiceCommand(transcript);
        };

        recognition.onerror = (event) => {
            console.error('Speech error:', event.error);
            if (event.error === 'not-allowed') {
                alert("Microphone permission blocked. Please check your browser address bar settings.");
            }
        };
    } else {
        document.getElementById('micBtn').style.opacity = '0.3';
        document.getElementById('micBtn').title = 'Speech not supported in this browser';
    }

    function toggleVoiceControl() {
        if (!recognition) {
            alert(translations[currentLang].alertSpeechNotSupported);
            return;
        }
        if (isListening) {
            recognition.stop();
        } else {
            recognition.start();
        }
    }

    function speak(text) {
        if ('speechSynthesis' in window) {
            window.speechSynthesis.cancel();
            const utterance = new SpeechSynthesisUtterance(text);
            utterance.lang = currentLang === 'en' ? 'en-US' : 'ar-SA';
            utterance.rate = 1.0;
            window.speechSynthesis.speak(utterance);
        }
    }

    function processVoiceCommand(command) {
        const t = translations[currentLang];
        let state = -1;
        
        if (currentLang === 'en') {
            if (command.includes('on') || command.includes('start') || command.includes('enable')) {
                state = 1;
            } else if (command.includes('off') || command.includes('stop') || command.includes('disable') || command.includes('close')) {
                state = 0;
            }

            // Master Commands
            if (command.includes('all') || command.includes('everything')) {
                if (state === 1) {
                    setAllRelays(1);
                    speak(t.speakSuccessAllOn);
                } else if (state === 0) {
                    setAllRelays(0);
                    speak(t.speakSuccessAllOff);
                }
                return;
            }

            // Channel commands
            let relayNum = 0;
            if (command.includes('one') || command.includes(' 1') || command.includes('light') || command.includes('lamp') || command.includes('first')) {
                relayNum = 1;
            } else if (command.includes('two') || command.includes(' 2') || command.includes('socket') || command.includes('plug') || command.includes('outlet') || command.includes('second')) {
                relayNum = 2;
            } else if (command.includes('three') || command.includes(' 3') || command.includes('fan') || command.includes('third')) {
                relayNum = 3;
            } else if (command.includes('four') || command.includes(' 4') || command.includes('tv') || command.includes('television') || command.includes('screen') || command.includes('fourth')) {
                relayNum = 4;
            }

            if (relayNum > 0 && state !== -1) {
                const label = relayNum === 1 ? t.relay1Name : relayNum === 2 ? t.relay2Name : relayNum === 3 ? t.relay3Name : t.relay4Name;
                const actionText = state === 1 ? t.speakActionOnState : t.speakActionOffState;
                toggleRelay(relayNum, state === 1);
                speak(`${t.speakActionOn} ${label} ${actionText}`);
            } else {
                speak(t.speakError);
            }
        } else {
            // Arabic processing with robust Egyptian colloquial support and normalization
            let normalized = command
                .replace(/[أإآ]/g, 'ا')
                .replace(/ة/g, 'ه')
                .replace(/ى/g, 'ي')
                .trim();

            // Egyptian Arabic synonyms for ON: شغل, افتح, ولع, قيد, ايد, نور, تفعيل, تشغيل
            const turnOnWords = ['شغل', 'افتح', 'ولع', 'قيد', 'ايد', 'نور', 'تفعيل', 'تشغيل'];
            // Egyptian Arabic synonyms for OFF: اطفي, اقفل, طفي, اغلق, بند, قفل, ايقاف, وقف, اوقف
            const turnOffWords = ['اطفي', 'اقفل', 'طفي', 'اغلق', 'بند', 'قفل', 'ايقاف', 'وقف', 'اوقف'];

            if (turnOnWords.some(w => normalized.includes(w))) {
                state = 1;
            } else if (turnOffWords.some(w => normalized.includes(w))) {
                state = 0;
            }

            // Channel commands with specific Egyptian word lists (supporting direct relative switch names)
            let relayNum = 0;
            
            // Channel 1: Light (لمبه, نور, اضاءه, المفتاح الاول, الاول, الاولي, الاوله, واحد, اول)
            const r1Words = ['لمبه', 'نور', 'اضاءه', 'المفتاح الاول', 'الاول', 'الاولي', 'الاوله', 'واحد', 'اول'];
            // Channel 2: Socket (مقبس, فيشه, بريزه, كبس, كوبس, كهربا, المفتاح التاني, المفتاح الثاني, التاني, الثاني, تاني, ثاني, اثنين, تنين)
            const r2Words = ['مقبس', 'فيشه', 'بريزه', 'كبس', 'كوبس', 'كهربا', 'المفتاح التاني', 'المفتاح الثاني', 'التاني', 'الثاني', 'تاني', 'ثاني', 'اثنين', 'تنين'];
            // Channel 3: Fan (مروحه, المفتاح التالت, المفتاح الثالث, التالت, الثالث, تالت, ثالث, تلاته, ثلاثه)
            const r3Words = ['مروحه', 'المفتاح التالت', 'المفتاح الثالث', 'التالت', 'الثالث', 'تالت', 'ثالث', 'تلاته', 'ثلاثه'];
            // Channel 4: Screen/TV (شاشه, تلفزيون, تليفزيون, المفتاح الرابع, الرابع, رابع, اربعه)
            const r4Words = ['شاشه', 'تلفزيون', 'تليفزيون', 'المفتاح الرابع', 'الرابع', 'رابع', 'اربعه'];

            // Match devices - check all lists independently to support multiple switches in one command
            let targetRelays = [];
            if (r1Words.some(w => normalized.includes(w))) {
                targetRelays.push(1);
            }
            if (r2Words.some(w => normalized.includes(w))) {
                targetRelays.push(2);
            }
            if (r3Words.some(w => normalized.includes(w))) {
                targetRelays.push(3);
            }
            if (r4Words.some(w => normalized.includes(w))) {
                targetRelays.push(4);
            }

            // Check if they are asking about all switches state: "حالة المفاتيح" or "المفاتيح شغالين ولا واقفين"
            if (normalized.includes('حاله المفاتيح') || (normalized.includes('المفاتيح') && (normalized.includes('شغال') || normalized.includes('واقف') || normalized.includes('مفتوح') || normalized.includes('مقفول')))) {
                const s1 = document.getElementById('switch1').checked;
                const s2 = document.getElementById('switch2').checked;
                const s3 = document.getElementById('switch3').checked;
                const s4 = document.getElementById('switch4').checked;
                
                let msg = "";
                msg += "اللمبة الأولى " + (s1 ? "قيد التشغيل" : "مغلقة") + "، ";
                msg += "المفتاح الثاني " + (s2 ? "قيد التشغيل" : "مغلق") + "، ";
                msg += "المروحة الثالثة " + (s3 ? "قيد التشغيل" : "مغلقة") + "، ";
                msg += "الشاشة الرابعة " + (s4 ? "قيد التشغيل" : "مغلقة") + ".";
                
                speak(msg);
                return;
            }

            // A. Check if user is asking about device state (e.g. "شغال ولا لأ؟", "حالة اللمبة الأولى")
            const queryWords = ['حاله', 'شغاله', 'شغال', 'مفتوح ولا', 'مقفول ولا', 'منور ولا', 'مفتوحه ولا', 'مقفوله ولا', 'مطفيه ولا', 'ايه الاخبار', 'منوره', 'مطفيه'];
            let isQuery = queryWords.some(w => normalized.includes(w)) || (normalized.startsWith('هل') || normalized.includes(' هل '));

            if (isQuery) {
                if (targetRelays.length > 0) {
                    let msg = "";
                    for (let relayNum of targetRelays) {
                        const label = relayNum === 1 ? 'اللمبة الأولى' : relayNum === 2 ? 'المفتاح الثاني' : relayNum === 3 ? 'المروحة الثالثة' : 'الشاشة الرابعة';
                        const isOn = document.getElementById(`switch${relayNum}`).checked;
                        msg += `${label} ${isOn ? 'قيد التشغيل' : 'متوقفة'}، `;
                    }
                    msg = msg.replace(/،\s*$/, '.'); // Clean up trailing comma
                    speak(msg);
                } else {
                    speak("عذراً، لم أستطع تحديد المفاتيح المطلوبة.");
                }
                return;
            }

            // B. Master Commands (e.g. الكل, كله, كل حاجه, الشقة كلها, ضلم الشقة, نور الشقة, المفاتيح)
            const allWords = ['الكل', 'كله', 'كل حاجه', 'كل حاجة', 'كل شي', 'كل شيء', 'الشقه كلها', 'البيت كله', 'الأنوار كلها', 'الانوار كلها', 'المفاتيح'];
            let isMasterOn = turnOnWords.some(w => normalized.includes(w)) && allWords.some(w => normalized.includes(w));
            let isMasterOff = turnOffWords.some(w => normalized.includes(w)) && allWords.some(w => normalized.includes(w));

            if (normalized.includes('ضلم') || normalized.includes('اطفي كله') || normalized.includes('اقفل كله') || normalized.includes('وقف كله')) {
                isMasterOff = true;
            }
            if (normalized.includes('نور كله') || normalized.includes('شغل كله') || normalized.includes('افتح كله')) {
                isMasterOn = true;
            }

            if (isMasterOn) {
                setAllRelays(1);
                speak("تم تشغيل جميع المفاتيح.");
                return;
            } else if (isMasterOff) {
                setAllRelays(0);
                speak("تم إيقاف جميع المفاتيح.");
                return;
            }

            // C. Control Command (ON/OFF) with redundancy check for multiple devices
            if (targetRelays.length > 0 && state !== -1) {
                let speechParts = [];
                let alreadyParts = [];

                for (let relayNum of targetRelays) {
                    const label = relayNum === 1 ? 'اللمبة الأولى' : relayNum === 2 ? 'المفتاح الثاني' : relayNum === 3 ? 'المروحة الثالثة' : 'الشاشة الرابعة';
                    const isOn = document.getElementById(`switch${relayNum}`).checked;

                    if (state === 1 && isOn) {
                        alreadyParts.push(`${label} قيد التشغيل بالفعل`);
                    } else if (state === 0 && !isOn) {
                        alreadyParts.push(`${label} مغلقة بالفعل`);
                    } else {
                        toggleRelay(relayNum, state === 1);
                        speechParts.push(label);
                    }
                }

                let finalSpeech = "";
                if (speechParts.length > 0) {
                    const actionText = state === 1 ? t.speakActionOn : t.speakActionOff;
                    finalSpeech += `${actionText} ${speechParts.join(' و ')}. `;
                }
                if (alreadyParts.length > 0) {
                    finalSpeech += alreadyParts.join(' و ') + ".";
                }
                speak(finalSpeech.trim());
            } else {
                speak(t.speakError);
            }
        }
    }

    // UI Updates
    function updateConnectionStatus(isOnline) {
        const dot = document.getElementById('connectionDot');
        const text = document.getElementById('connectionText');
        const t = translations[currentLang];
        
        if (isOnline) {
            dot.className = 'status-dot online';
            text.textContent = t.connected;
        } else {
            dot.className = 'status-dot';
            text.textContent = t.disconnected;
        }
    }

    function getBaseUrl() {
        if (targetIp) {
            return `http://${targetIp}`;
        }
        return '';
    }

    // Fetch states from ESP32
    async function fetchStatus() {
        try {
            const url = `${getBaseUrl()}/api/status`;
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 2000);

            const response = await fetch(url, { signal: controller.signal });
            clearTimeout(timeoutId);

            if (!response.ok) throw new Error('Network error');
            const data = await response.json();
            
            updateRelayUI(1, data.relay1);
            updateRelayUI(2, data.relay2);
            updateRelayUI(3, data.relay3);
            updateRelayUI(4, data.relay4);

            document.getElementById('infoRssi').textContent = `${data.rssi} dBm`;
            document.getElementById('infoUptime').textContent = formatUptime(data.uptime);
            
            if (data.version) {
                currentFirmwareVersion = data.version;
                compareVersions();
            }
            
            updateConnectionStatus(true);
        } catch (error) {
            console.error('Error fetching telemetry:', error);
            updateConnectionStatus(false);
        }
    }

    function updateRelayUI(relayId, state) {
        const card = document.getElementById(`card${relayId}`);
        const switchInput = document.getElementById(`switch${relayId}`);
        const statusText = document.getElementById(`status${relayId}`);
        const t = translations[currentLang];
        const isOn = state === 1;

        switchInput.checked = isOn;
        if (isOn) {
            card.classList.add('active');
            statusText.textContent = t.stateOn;
        } else {
            card.classList.remove('active');
            statusText.textContent = t.stateOff;
        }
    }

    // Toggle single relay
    async function toggleRelay(relayId, isChecked) {
        const state = isChecked ? 1 : 0;
        updateRelayUI(relayId, state);

        try {
            const url = `${getBaseUrl()}/api/control?relay=${relayId}&state=${state}`;
            const response = await fetch(url, { method: 'GET' });
            if (!response.ok) throw new Error('Control failed');
            fetchStatus();
        } catch (error) {
            console.error(`Failed to toggle Relay ${relayId}:`, error);
            updateRelayUI(relayId, state === 1 ? 0 : 1);
            updateConnectionStatus(false);
        }
    }

    // Master Switch controls
    async function setAllRelays(state) {
        for (let i = 1; i <= 4; i++) {
            updateRelayUI(i, state);
        }

        try {
            const promises = [];
            for (let i = 1; i <= 4; i++) {
                promises.push(fetch(`${getBaseUrl()}/api/control?relay=${i}&state=${state}`));
            }
            await Promise.all(promises);
            fetchStatus();
        } catch (error) {
            console.error('Failed to set master state:', error);
            fetchStatus();
        }
    }

    function formatUptime(seconds) {
        if (!seconds) return 'N/A';
        const d = Math.floor(seconds / (3600*24));
        const h = Math.floor(seconds % (3600*24) / 3600);
        const m = Math.floor(seconds % 3600 / 60);
        const s = Math.floor(seconds % 60);
        
        if (currentLang === 'ar') {
            let out = '';
            if (d > 0) out += `${d} يوم `;
            if (h > 0 || d > 0) out += `${h} ساعة `;
            if (m > 0 || h > 0 || d > 0) out += `${m} دقيقة `;
            out += `${s} ثانية`;
            return out;
        } else {
            let out = '';
            if (d > 0) out += `${d}d `;
            if (h > 0 || d > 0) out += `${h}h `;
            if (m > 0 || h > 0 || d > 0) out += `${m}m `;
            out += `${s}s`;
            return out;
        }
    }

    function openConfigModal() {
        document.getElementById('ipInput').value = targetIp;
        document.getElementById('configModal').classList.add('open');
    }

    function closeConfigModal() {
        document.getElementById('configModal').classList.remove('open');
    }

    function saveConfigIp() {
        const val = document.getElementById('ipInput').value.trim();
        targetIp = val;
        if (val) {
            localStorage.setItem('esp32_target_ip', val);
            document.getElementById('infoTargetIp').textContent = val;
        } else {
            localStorage.removeItem('esp32_target_ip');
            document.getElementById('infoTargetIp').textContent = 'Auto (Local)';
        }
        closeConfigModal();
        fetchStatus();
    }

    async function resetWifiSettings() {
        if (!confirm(currentLang === 'en' ? 'Are you sure you want to reset Wi-Fi settings? The device will restart and broadcast Smart-Home-Setup.' : 'هل أنت متأكد من إعادة ضبط إعدادات الواي فاي؟ سيعاد تشغيل الجهاز ويبث شبكة إعدادات جديدة.')) return;
        try {
            const url = `${getBaseUrl()}/api/wifi_reset`;
            const response = await fetch(url);
            if (response.ok) {
                alert(currentLang === 'en' ? 'Wi-Fi reset successfully! Reconnect to Smart-Home-Setup.' : 'تمت إعادة ضبط الواي فاي بنجاح! اتصل بشبكة Smart-Home-Setup لإعادة إعداد الجهاز.');
                window.location.reload();
            } else {
                throw new Error("Reset failed");
            }
        } catch (e) {
            alert(currentLang === 'en' ? 'Failed to reset Wi-Fi.' : 'فشل إعادة ضبط الواي فاي. تأكد من الاتصال بالجهاز.');
        }
    }

    // Update System Configuration & Logic
    const UPDATE_CHECK_URL = 'https://raw.githubusercontent.com/El-Morsy74/SH/main/version.json';
    let latestUpdateInfo = null;
    let currentFirmwareVersion = "";

    async function checkSystemUpdates() {
        try {
            const response = await fetch(UPDATE_CHECK_URL);
            if (!response.ok) return;
            const data = await response.json();
            latestUpdateInfo = data;
            compareVersions();
        } catch (e) {
            console.log("Update check skipped or offline:", e);
        }
    }

    function compareVersions() {
        if (!latestUpdateInfo || !currentFirmwareVersion) return;
        
        if (latestUpdateInfo.version !== currentFirmwareVersion) {
            const banner = document.getElementById('updateBanner');
            const text = document.getElementById('updateBannerText');
            const btn = document.getElementById('btnTriggerUpdate');
            
            if (currentLang === 'ar') {
                text.textContent = `يتوفر تحديث جديد للبرنامج (إصدار ${latestUpdateInfo.version})`;
                btn.textContent = "تحديث الآن";
            } else {
                text.textContent = `New firmware update available (v${latestUpdateInfo.version})`;
                btn.textContent = "Update Now";
            }
            banner.style.display = 'flex';
        } else {
            document.getElementById('updateBanner').style.display = 'none';
        }
    }

    async function startSystemUpdate() {
        if (!latestUpdateInfo || !latestUpdateInfo.url) return;
        
        const modal = document.getElementById('updateProgressModal');
        const progressBar = document.getElementById('updateProgressBar');
        const progressText = document.getElementById('updateProgressText');
        const progressPercent = document.getElementById('updateProgressPercent');
        
        modal.classList.add('open');
        progressBar.style.width = '0%';
        progressPercent.textContent = '0%';
        progressBar.style.background = 'var(--accent-color)';
        
        if (currentLang === 'ar') {
            document.getElementById('uiUpdateProgressTitle').textContent = "جاري تحديث النظام...";
            document.getElementById('uiUpdateProgressDesc').textContent = "برجاء عدم إيقاف تشغيل الجهاز أو فصل الكهرباء أثناء التحديث.";
            progressText.textContent = "جاري إرسال طلب التحديث...";
        } else {
            document.getElementById('uiUpdateProgressTitle').textContent = "System Updating...";
            document.getElementById('uiUpdateProgressDesc').textContent = "Please do not turn off the device or disconnect power.";
            progressText.textContent = "Sending update request...";
        }
        
        try {
            const targetUrl = `${getBaseUrl()}/api/trigger_update?url=${encodeURIComponent(latestUpdateInfo.url)}`;
            const response = await fetch(targetUrl);
            if (!response.ok) throw new Error("Update trigger failed");
            
            let percent = 5;
            const interval = setInterval(async () => {
                if (percent < 90) {
                    percent += Math.floor(Math.random() * 8) + 2;
                    if (percent > 90) percent = 90;
                    progressBar.style.width = percent + '%';
                    progressPercent.textContent = percent + '%';
                    progressText.textContent = currentLang === 'ar' ? "جاري تنزيل التحديث وتثبيته..." : "Downloading and writing firmware...";
                } else {
                    // Poll /api/status to check if ESP32 is back online with new version
                    try {
                        const testUrl = `${getBaseUrl()}/api/status`;
                        const testRes = await fetch(testUrl);
                        if (testRes.ok) {
                            const testData = await testRes.json();
                            if (testData.uptime < 30) { // It restarted!
                                clearInterval(interval);
                                progressBar.style.width = '100%';
                                progressPercent.textContent = '100%';
                                progressBar.style.background = 'var(--success-color)';
                                progressText.textContent = currentLang === 'ar' ? "اكتمل التحديث بنجاح!" : "Update completed successfully!";
                                setTimeout(() => {
                                    modal.classList.remove('open');
                                    window.location.reload();
                                }, 2000);
                            }
                        }
                    } catch (e) {
                        progressText.textContent = currentLang === 'ar' ? "جاري تثبيت الملف وإعادة التشغيل..." : "Writing flash and rebooting...";
                    }
                }
            }, 1000);
            
        } catch (error) {
            console.error("Update failed:", error);
            progressBar.style.background = 'var(--danger-color)';
            progressText.textContent = currentLang === 'ar' ? "فشل التحديث. يرجى المحاولة لاحقاً." : "Update failed. Please try again.";
            setTimeout(() => {
                modal.classList.remove('open');
            }, 4000);
        }
    }

    window.addEventListener('DOMContentLoaded', () => {
        // Load language preference
        setLanguage(currentLang);
        
        if (targetIp) {
            document.getElementById('infoTargetIp').textContent = targetIp;
        }
        fetchStatus();
        checkSystemUpdates(); // Check GitHub for updates
        pollInterval = setInterval(fetchStatus, 3000);
    });
</script>
</body>
</html>

)rawliteral";

const char WIFI_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>إعدادات الاتصال - Smart Home Setup</title>
    <style>
        :root {
            --bg-color: #0b0f19;
            --panel-bg: rgba(17, 24, 39, 0.75);
            --panel-border: rgba(255, 255, 255, 0.08);
            --text-primary: #f3f4f6;
            --text-secondary: #9ca3af;
            --accent-color: #6366f1;
            --accent-active: #818cf8;
            --success-color: #10b981;
            --danger-color: #ef4444;
        }
        body {
            background-color: var(--bg-color);
            background-image: radial-gradient(at 0% 0%, rgba(99, 102, 241, 0.1) 0px, transparent 50%), radial-gradient(at 100% 0%, rgba(236, 72, 153, 0.05) 0px, transparent 50%);
            color: var(--text-primary);
            font-family: system-ui, -apple-system, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
            padding: 1rem;
            box-sizing: border-box;
        }
        .card {
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            border-radius: 24px;
            padding: 2.5rem 2rem;
            width: 100%;
            max-width: 420px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.4);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
        }
        h2 {
            margin-top: 0;
            margin-bottom: 0.5rem;
            font-size: 1.5rem;
            background: linear-gradient(135deg, #a855f7, #6366f1);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-align: center;
        }
        p {
            color: var(--text-secondary);
            font-size: 0.85rem;
            text-align: center;
            margin-bottom: 2rem;
        }
        .form-group {
            margin-bottom: 1.5rem;
            display: flex;
            flex-direction: column;
            gap: 8px;
            text-align: right;
        }
        label {
            font-size: 0.85rem;
            font-weight: 600;
            color: var(--text-primary);
        }
        select, input {
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid var(--panel-border);
            border-radius: 12px;
            padding: 0.8rem 1rem;
            color: white;
            font-size: 0.95rem;
            outline: none;
            width: 100%;
            box-sizing: border-box;
            transition: all 0.3s;
        }
        select:focus, input:focus {
            border-color: var(--accent-color);
            box-shadow: 0 0 10px rgba(99, 102, 241, 0.2);
        }
        option {
            background: #111827;
            color: white;
        }
        button {
            width: 100%;
            padding: 0.9rem;
            background: var(--accent-color);
            border: none;
            border-radius: 12px;
            color: white;
            font-weight: 700;
            cursor: pointer;
            transition: background 0.3s;
            margin-top: 1rem;
            font-size: 0.95rem;
        }
        button:hover {
            background: var(--accent-active);
        }
        .status {
            text-align: center;
            font-size: 0.85rem;
            margin-top: 1rem;
            display: none;
        }
        .status.success { color: var(--success-color); }
        .status.error { color: var(--danger-color); }
        .loading-spinner {
            display: inline-block;
            width: 16px;
            height: 16px;
            border: 2px solid rgba(255,255,255,0.2);
            border-radius: 50%;
            border-top-color: white;
            animation: spin 0.8s linear infinite;
            margin-left: 8px;
            vertical-align: middle;
        }
        @keyframes spin { to { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="card">
        <h2>إعدادات الواي فاي 🌐</h2>
        <p>يرجى اختيار شبكة الواي فاي المنزلية وتوصيل اللوحة.</p>
        
        <form id="wifiForm">
            <div class="form-group">
                <label for="ssid">اسم الشبكة (SSID)</label>
                <div style="position: relative;">
                    <select id="ssid" required>
                        <option value="">جاري البحث عن الشبكات المتاحة...</option>
                    </select>
                </div>
            </div>
            
            <div class="form-group">
                <label for="password">كلمة المرور (Password)</label>
                <input type="password" id="password" placeholder="أدخل كلمة المرور" required style="direction: ltr; text-align: left;">
            </div>
            
            <button type="submit" id="btnSubmit">توصيل وحفظ الإعدادات</button>
        </form>
        
        <div id="status" class="status"></div>
    </div>

    <script>
        const ssidSelect = document.getElementById('ssid');
        const form = document.getElementById('wifiForm');
        const statusDiv = document.getElementById('status');
        const btnSubmit = document.getElementById('btnSubmit');

        // Fetch scanned networks on load
        async function scanWifi() {
            try {
                const response = await fetch('/api/scan_wifi');
                if (!response.ok) throw new Error("Failed to scan");
                const networks = await response.json();
                
                ssidSelect.innerHTML = '<option value="">-- اختر الشبكة --</option>';
                networks.forEach(net => {
                    const opt = document.createElement('option');
                    opt.value = net.ssid;
                    opt.textContent = `${net.ssid} (${net.rssi} dBm)`;
                    ssidSelect.appendChild(opt);
                });
                
                // Add a manual option
                const manualOpt = document.createElement('option');
                manualOpt.value = "__MANUAL__";
                manualOpt.textContent = "[كتابة الشبكة يدوياً]";
                ssidSelect.appendChild(manualOpt);
            } catch (e) {
                ssidSelect.innerHTML = '<option value="">فشل البحث. اكتب الشبكة يدوياً...</option>';
                const manualOpt = document.createElement('option');
                manualOpt.value = "__MANUAL__";
                manualOpt.textContent = "[كتابة الشبكة يدوياً]";
                ssidSelect.appendChild(manualOpt);
            }
        }

        // Handle SSID select change
        ssidSelect.addEventListener('change', () => {
            if (ssidSelect.value === "__MANUAL__") {
                const manualSsid = prompt("أدخل اسم الشبكة يدوياً:");
                if (manualSsid) {
                    const opt = document.createElement('option');
                    opt.value = manualSsid;
                    opt.textContent = manualSsid;
                    opt.selected = true;
                    ssidSelect.appendChild(opt);
                } else {
                    ssidSelect.selectedIndex = 0;
                }
            }
        });

        // Submit form
        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            const ssid = ssidSelect.value;
            const password = document.getElementById('password').value;
            
            if (!ssid) {
                alert("برجاء اختيار شبكة");
                return;
            }
            
            btnSubmit.disabled = true;
            btnSubmit.innerHTML = 'جاري التوصيل والحفظ... <span class="loading-spinner"></span>';
            statusDiv.style.display = 'none';

            try {
                const url = `/api/wifi_save?ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`;
                const response = await fetch(url);
                if (response.ok) {
                    statusDiv.className = 'status success';
                    statusDiv.textContent = 'تم حفظ الإعدادات بنجاح! سيعاد تشغيل الجهاز الآن للاتصال بشبكتك المنزلية.';
                    statusDiv.style.display = 'block';
                    form.style.display = 'none';
                } else {
                    throw new Error("Save failed");
                }
            } catch (err) {
                statusDiv.className = 'status error';
                statusDiv.textContent = 'فشلت عملية الحفظ. يرجى المحاولة مرة أخرى.';
                statusDiv.style.display = 'block';
                btnSubmit.disabled = false;
                btnSubmit.textContent = 'توصيل وحفظ الإعدادات';
            }
        });

        // Run scan on boot
        scanWifi();
    </script>
</body>
</html>
)rawliteral";

// ==========================================
// 4. HTTP REQUEST HANDLERS
// ==========================================

// Serve the Main Dashboard
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

// Serve Status JSON
void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  unsigned long uptimeSeconds = millis() / 1000;
  int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  
  String json = "{";
  json += "\"relay1\":" + String(relayStates[0]) + ",";
  json += "\"relay2\":" + String(relayStates[1]) + ",";
  json += "\"relay3\":" + String(relayStates[2]) + ",";
  json += "\"relay4\":" + String(relayStates[3]) + ",";
  json += "\"rssi\":" + String(rssi) + ",";
  json += "\"uptime\":" + String(uptimeSeconds) + ",";
  json += "\"version\":\"" + FIRMWARE_VERSION + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// Handle Relay Control API
void handleControl() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  if (!server.hasArg("relay") || !server.hasArg("state")) {
    server.send(400, "text/plain", "Error: Missing parameters");
    return;
  }
  
  int relayNum = server.arg("relay").toInt();
  int state = server.arg("state").toInt();
  
  if (relayNum < 1 || relayNum > 4 || (state != 0 && state != 1)) {
    server.send(400, "text/plain", "Error: Invalid parameters");
    return;
  }
  
  // Save status
  relayStates[relayNum - 1] = state;
  
  // Write to GPIO pin (Inverted for NC wiring: state 1 -> write HIGH, state 0 -> write LOW)
  digitalWrite(RELAY_PINS[relayNum - 1], state == 1 ? HIGH : LOW);
  
  Serial.print("Relay ");
  Serial.print(relayNum);
  Serial.print(" set to ");
  Serial.println(state == 1 ? "ON (HIGH)" : "OFF (LOW)");
  
  server.send(200, "text/plain", "OK");
}

// Handle 404 Route
void handleNotFound() {
  server.send(404, "text/plain", "Error: 404 Not Found");
}

// Handle triggering the OTA update from remote HTTP/HTTPS URL
void handleTriggerUpdate() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  if (!server.hasArg("url")) {
    server.send(400, "text/plain", "Error: Missing url parameter");
    return;
  }
  
  String updateUrl = server.arg("url");
  
  // Send OK back to client before starting update to prevent browser timeout
  server.send(200, "text/plain", "OK");
  delay(500);
  
  Serial.print("Starting HTTP OTA Update from URL: ");
  Serial.println(updateUrl);
  
  WiFiClientSecure client;
  client.setInsecure(); // Skip SSL certificate validation
  client.setTimeout(12000); // 12 seconds timeout for slow connection
  
  // Optional progress reporting to Serial Monitor
  httpUpdate.onProgress([](int cur, int total) {
    static int lastPercent = -1;
    int percent = (cur * 100) / total;
    if (percent % 10 == 0 && percent != lastPercent) {
      Serial.printf("Update Progress: %d%%\n", percent);
      lastPercent = percent;
    }
  });

  t_httpUpdate_return ret = httpUpdate.update(client, updateUrl);
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP Update Failed (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP Update: No Updates Available");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP Update Success! Rebooting...");
      break;
  }
}

// Serve the Manual update upload page
void handleUpdatePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>تحديث النظام يدوياً</title>
    <style>
        body { background-color: #0b0f19; color: #f3f4f6; font-family: system-ui, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
        .card { background: rgba(17, 24, 39, 0.7); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 20px; padding: 2rem; width: 100%; max-width: 400px; text-align: center; box-shadow: 0 10px 30px rgba(0,0,0,0.3); }
        h2 { background: linear-gradient(135deg, #a855f7, #6366f1); -webkit-background-clip: text; -webkit-text-fill-color: transparent; margin-bottom: 1.5rem; }
        .file-label { display: block; background: rgba(255,255,255,0.03); border: 1px dashed rgba(255,255,255,0.2); padding: 1.5rem; border-radius: 12px; cursor: pointer; margin-bottom: 1.5rem; transition: 0.3s; }
        .file-label:hover { border-color: #6366f1; background: rgba(99,102,241,0.05); }
        input[type="file"] { display: none; }
        button { width: 100%; padding: 0.8rem; background: #6366f1; border: none; border-radius: 10px; color: white; font-weight: 600; cursor: pointer; transition: 0.3s; }
        button:hover { background: #4f46e5; }
        #progress { margin-top: 1rem; display: none; }
        .bar-container { background: rgba(255,255,255,0.1); border-radius: 5px; height: 8px; overflow: hidden; margin-top: 0.5rem; }
        .bar { background: #6366f1; width: 0%; height: 100%; transition: width 0.1s; }
    </style>
</head>
<body>
    <div class="card">
        <h2>تحديث النظام يدوياً</h2>
        <form method="POST" action="/update" enctype="multipart/form-data" id="uploadForm">
            <label for="fileInput" class="file-label">
                <span id="fileText">اختر ملف التحديث (.bin)</span>
            </label>
            <input type="file" name="update" id="fileInput" accept=".bin" required>
            <button type="submit">بدء التحديث اليدوي</button>
        </form>
        <div id="progress">
            <span id="progressText">جاري الرفع: 0%</span>
            <div class="bar-container"><div class="bar" id="progressBar"></div></div>
        </div>
    </div>
    <script>
        const fileInput = document.getElementById('fileInput');
        const fileText = document.getElementById('fileText');
        const form = document.getElementById('uploadForm');
        const progressDiv = document.getElementById('progress');
        const progressBar = document.getElementById('progressBar');
        const progressText = document.getElementById('progressText');

        fileInput.addEventListener('change', () => {
            if (fileInput.files.length > 0) fileText.textContent = fileInput.files[0].name;
        });

        form.addEventListener('submit', (e) => {
            e.preventDefault();
            const file = fileInput.files[0];
            if (!file) return;
            progressDiv.style.display = 'block';
            form.style.display = 'none';

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/update', true);
            xhr.upload.addEventListener('progress', (evt) => {
                if (evt.lengthComputable) {
                    const percent = Math.round((evt.loaded / evt.total) * 100);
                    progressBar.style.width = percent + '%';
                    progressText.textContent = `جاري الرفع: ${percent}%`;
                }
            });
            xhr.onload = () => {
                if (xhr.status === 200) {
                    progressText.textContent = 'تم التحديث بنجاح! جاري إعادة تشغيل اللوحة...';
                    progressBar.style.background = '#10b981';
                    setTimeout(() => { window.location.href = '/'; }, 5000);
                } else {
                    progressText.textContent = 'فشلت العملية: ' + xhr.responseText;
                    progressBar.style.background = '#ef4444';
                }
            };
            const formData = new FormData();
            formData.append('update', file);
            xhr.send(formData);
        });
    </script>
</body>
</html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  delay(1000);
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

// Serve the WiFi Configuration Portal page
void handleWifiPortal() {
  server.send(200, "text/html", WIFI_PORTAL_HTML);
}

// Scan WiFi networks and return JSON
void handleScanWifi() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Save Wi-Fi credentials to Preferences and restart
void handleWifiSave() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    server.send(400, "text/plain", "Error: Missing SSID or Password");
    return;
  }
  
  String newSsid = server.arg("ssid");
  String newPass = server.arg("password");
  
  Preferences preferences;
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", newSsid);
  preferences.putString("password", newPass);
  preferences.end();
  
  server.send(200, "text/plain", "OK");
  delay(1000);
  ESP.restart();
}

// Reset Wi-Fi configuration and reboot to portal AP mode
void handleWifiReset() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  Preferences preferences;
  preferences.begin("wifi-config", false);
  preferences.clear();
  preferences.end();
  
  server.send(200, "text/plain", "OK");
  delay(1000);
  ESP.restart();
}

// ==========================================
// 5. MAIN SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n--- Smart Home Relay Controller Booting ---");

  // Initialize GPIO Relay pins as outputs and set them LOW (ON for trigger -> NC disconnected -> Lamp OFF by default)
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    relayStates[i] = 0;
  }
  Serial.println("GPIOs Initialized (Relays ON, NC open, Lamp OFF by default).");

  // Read saved Wi-Fi credentials from Preferences
  Preferences preferences;
  preferences.begin("wifi-config", true); // Read-only mode
  wifi_ssid = preferences.getString("ssid", "");
  wifi_password = preferences.getString("password", "");
  preferences.end();

  bool portalMode = false;

  if (wifi_ssid == "") {
    Serial.println("No saved Wi-Fi configuration found. Starting portal AP mode...");
    portalMode = true;
  } else {
    Serial.print("Saved Wi-Fi configuration found: ");
    Serial.println(wifi_ssid);
    
    // Attempt connection
    WiFi.mode(WIFI_AP_STA);
    
    // Apply static IP configuration (only for fallback home-wifi mode)
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure Static IP");
    } else {
      Serial.println("Static IP configured successfully.");
    }
    
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    // Wait for Wi-Fi connection with a timeout of 15 seconds
    unsigned long startAttemptTime = millis();
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected successfully!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Failed to connect to home Wi-Fi. Entering AP portal configuration mode...");
      portalMode = true;
    }
  }

  if (portalMode) {
    // Start AP Setup Mode (Open SSID "Smart-Home-Setup")
    WiFi.mode(WIFI_AP);
    WiFi.softAP(setup_ap_ssid);
    Serial.print("Access Point started: ");
    Serial.println(setup_ap_ssid);
    Serial.print("Setup Portal URL: http://");
    Serial.println(WiFi.softAPIP());
    
    // Setup Portal Routes
    server.on("/", HTTP_GET, handleWifiPortal);
    server.on("/api/scan_wifi", HTTP_GET, handleScanWifi);
    server.on("/api/wifi_save", HTTP_GET, handleWifiSave);
    server.onNotFound(handleNotFound);
  } else {
    // Normal Mode Routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/control", HTTP_GET, handleControl);
    server.on("/api/trigger_update", HTTP_GET, handleTriggerUpdate);
    server.on("/api/wifi_reset", HTTP_GET, handleWifiReset); // Add resetting WiFi settings route
    server.on("/update", HTTP_GET, handleUpdatePage);
    server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.onNotFound(handleNotFound);
  }

  // Start server
  server.begin();
  Serial.println("HTTP Web Server Started.");
}

void loop() {
  // Handle client requests
  server.handleClient();
  
  // Yield execution to allow underlying Wi-Fi stack tasks to run
  delay(2);
}
