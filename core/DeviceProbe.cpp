#include "DeviceProbe.hpp"
#include <sys/sysctl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/utsname.h>
#include <mach/mach.h>

// ===== ОСНОВНАЯ ФУНКЦИЯ ДЕТЕКЦИИ =====
DeviceDescriptor DeviceProbe::detect()
{
    DeviceDescriptor d;

    // ===== RESOURCES =====
    d.resources.ram_mb = detectRAM();
    d.resources.cpu_cores = detectCPU();
    d.resources.cpu_ghz = detectCPUFrequency();
    d.resources.gpu = detectGPU();

    // ===== SENSORS =====
    d.sensors.camera = detectCamera();
    d.sensors.microphone = detectMicrophone();
    d.sensors.imu = detectIMU();
    d.sensors.gps = detectGPS();
    d.sensors.touch = detectTouch();

    // ===== ACTUATORS =====
    d.actuators.display = detectDisplay();
    d.actuators.speaker = detectSpeaker();
    d.actuators.motor = detectMotor();
    d.actuators.network = detectNetwork();

    // ===== INTERFACES =====
    d.interfaces.stdin = isatty(STDIN_FILENO);
    d.interfaces.stdout = isatty(STDOUT_FILENO);
    d.interfaces.filesystem = true; // почти всегда есть
    d.interfaces.bluetooth = detectBluetooth();
    d.interfaces.wifi = detectWifi();

    // ===== DEVICE TYPE =====
    d.device_type = detectDeviceType(d);

    return d;
}

// ===== HARDWARE ID =====
std::string DeviceProbe::getHardwareId()
{
    char buffer[128];
    size_t len = sizeof(buffer);

    if (sysctlbyname("kern.uuid", buffer, &len, NULL, 0) == 0)
        return std::string(buffer);

    // Fallback: комбинация hostname + время
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    time_t now = time(nullptr);
    std::stringstream ss;
    ss << hostname << "_" << now;
    return ss.str();
}

// ===== РЕСУРСЫ =====
size_t DeviceProbe::detectRAM()
{
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t size = 0;
    size_t len = sizeof(size);

    if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) {
        return size / (1024 * 1024); // в MB
    }
    return 1024; // fallback: 1GB
}

int DeviceProbe::detectCPU()
{
    int cores;
    size_t len = sizeof(cores);
    
    if (sysctlbyname("hw.ncpu", &cores, &len, NULL, 0) == 0) {
        return cores;
    }
    return 2; // fallback
}

float DeviceProbe::detectCPUFrequency()
{
    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
        return freq / 1e9; // в GHz
    }
    return 2.0f; // fallback
}

bool DeviceProbe::detectGPU()
{
#ifdef __APPLE__
    // На Mac есть GPU (Apple Silicon или дискретная)
    return true;
#else
    // Проверка наличия /dev/dri
    std::ifstream dri("/dev/dri");
    return dri.good();
#endif
}

// ===== СЕНСОРЫ =====
bool DeviceProbe::detectCamera()
{
    std::ifstream dev("/dev/video0");
    if (dev.good()) return true;
    
    dev.open("/dev/video1");
    return dev.good();
}

bool DeviceProbe::detectMicrophone()
{
#ifdef __APPLE__
    // На Mac почти всегда есть микрофон
    return true;
#else
    std::ifstream dev("/dev/snd/");
    return dev.good();
#endif
}

bool DeviceProbe::detectIMU()
{
    // IMU (акселерометр, гироскоп) - для роботов/телефонов
    std::ifstream imu("/sys/bus/iio/devices/");
    if (imu.good()) return true;
    
    // Проверка наличия акселерометра в macOS
#ifdef __APPLE__
    // Через IOKit можно проверить наличие Motion Sensors
    // Упрощённо: считаем что на MacBook есть
    char model[256];
    size_t len = sizeof(model);
    if (sysctlbyname("hw.model", &model, &len, NULL, 0) == 0) {
        std::string model_str(model);
        if (model_str.find("MacBook") != std::string::npos) {
            return true; // у MacBook есть IMU для защиты падения
        }
    }
#endif
    return false;
}

bool DeviceProbe::detectGPS()
{
    std::ifstream gps("/dev/ttyGPS0");
    if (gps.good()) return true;
    
    gps.open("/dev/ttyUSB0");
    return gps.good();
}

bool DeviceProbe::detectTouch()
{
#ifdef __APPLE__
    return false; // у Mac нет touch screen
#else
    std::ifstream touch("/dev/input/event0");
    return touch.good();
#endif
}

// ===== АКТУАТОРЫ =====
bool DeviceProbe::detectDisplay()
{
    return getenv("DISPLAY") != nullptr || 
           getenv("WAYLAND_DISPLAY") != nullptr;
}

bool DeviceProbe::detectSpeaker()
{
    // Почти всегда есть (хотя бы пищалка)
    return true;
}

bool DeviceProbe::detectMotor()
{
    std::ifstream pwm("/sys/class/pwm/");
    if (pwm.good()) return true;
    
    // Проверка GPIO для роботов
    std::ifstream gpio("/sys/class/gpio/");
    return gpio.good();
}

bool DeviceProbe::detectNetwork()
{
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1)
        return false;
    
    bool has_network = false;
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            std::string name(ifa->ifa_name);
            if (name != "lo0" && name != "lo") {
                has_network = true;
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return has_network;
}

// ===== ИНТЕРФЕЙСЫ =====
bool DeviceProbe::detectBluetooth()
{
    std::ifstream bt("/sys/class/bluetooth/");
    return bt.good();
}

bool DeviceProbe::detectWifi()
{
    std::ifstream wifi("/sys/class/net/wlan0");
    if (wifi.good()) return true;
    
    wifi.open("/sys/class/net/en0"); // на Mac
    return wifi.good();
}

// ===== ОПРЕДЕЛЕНИЕ ТИПА УСТРОЙСТВА =====
std::string DeviceProbe::detectDeviceType(const DeviceDescriptor& d)
{
    // Проверяем по наличию характерных компонентов
    if (d.actuators.motor && d.sensors.imu)
        return "robot";
    
    if (d.sensors.gps && d.sensors.imu && d.actuators.motor)
        return "drone";
    
    if (d.sensors.touch && d.sensors.camera && d.sensors.imu)
        return "phone";
    
    if (d.sensors.camera && d.sensors.microphone && d.actuators.display)
        return "computer";
    
    if (d.resources.ram_mb > 2000 && d.resources.cpu_cores > 4)
        return "server";
    
    if (d.resources.ram_mb < 512 || d.resources.cpu_cores < 2)
        return "embedded";
    
    if (d.sensors.imu && !d.actuators.motor)
        return "wearable";
    
    return "generic";
}

// ===== ПОСТРОЕНИЕ ГРАФА ВОЗМОЖНОСТЕЙ =====
std::vector<Capability> DeviceProbe::buildCapabilities(const DeviceDescriptor& d)
{
    std::vector<Capability> caps;

    if (d.sensors.camera)
        caps.push_back({"camera", {"capture_frame", "detect_motion", "recognize_object"}});

    if (d.sensors.microphone)
        caps.push_back({"microphone", {"listen", "detect_voice", "record_audio"}});

    if (d.sensors.imu)
        caps.push_back({"imu", {"detect_motion", "detect_orientation", "detect_shake"}});

    if (d.sensors.gps)
        caps.push_back({"gps", {"get_location", "track_movement"}});

    if (d.sensors.touch)
        caps.push_back({"touch", {"detect_tap", "detect_swipe", "detect_pinch"}});

    if (d.actuators.display)
        caps.push_back({"display", {"render_text", "render_ui", "show_image"}});

    if (d.actuators.speaker)
        caps.push_back({"speaker", {"play_sound", "speak_text"}});

    if (d.actuators.motor)
        caps.push_back({"motor", {"move", "rotate", "stop"}});

    if (d.actuators.network)
        caps.push_back({"network", {"send_data", "receive_data"}});

    if (d.interfaces.bluetooth)
        caps.push_back({"bluetooth", {"scan", "connect", "send"}});

    if (d.interfaces.wifi)
        caps.push_back({"wifi", {"scan", "connect", "send"}});

    return caps;
}

// ===== SELF-DESCRIPTION API =====
std::string DeviceProbe::describe(const DeviceDescriptor& d)
{
    std::stringstream ss;
    
    ss << "=== MARY DEVICE DESCRIPTION ===\n";
    ss << "Device type: " << d.device_type << "\n";
    ss << "Resources:\n";
    ss << "  RAM: " << d.resources.ram_mb << " MB\n";
    ss << "  CPU: " << d.resources.cpu_cores << " cores @ " 
       << d.resources.cpu_ghz << " GHz\n";
    ss << "  GPU: " << (d.resources.gpu ? "yes" : "no") << "\n";
    
    ss << "Sensors:\n";
    ss << "  Camera: " << (d.sensors.camera ? "✓" : "✗") << "\n";
    ss << "  Microphone: " << (d.sensors.microphone ? "✓" : "✗") << "\n";
    ss << "  IMU: " << (d.sensors.imu ? "✓" : "✗") << "\n";
    ss << "  GPS: " << (d.sensors.gps ? "✓" : "✗") << "\n";
    ss << "  Touch: " << (d.sensors.touch ? "✓" : "✗") << "\n";
    
    ss << "Actuators:\n";
    ss << "  Display: " << (d.actuators.display ? "✓" : "✗") << "\n";
    ss << "  Speaker: " << (d.actuators.speaker ? "✓" : "✗") << "\n";
    ss << "  Motor: " << (d.actuators.motor ? "✓" : "✗") << "\n";
    ss << "  Network: " << (d.actuators.network ? "✓" : "✗") << "\n";
    
    return ss.str();
}

// ===== ПРЕОБРАЗОВАНИЕ В ВЕКТОР ДЛЯ НЕЙРОСЕТИ =====
std::vector<float> DeviceProbe::encodeDevice(const DeviceDescriptor& d)
{
    // 16-мерный вектор состояния устройства
    return {
        // Ресурсы (нормализованные)
        std::min(1.0f, static_cast<float>(d.resources.ram_mb) / 8192.0f),  // RAM (0-1)
        std::min(1.0f, static_cast<float>(d.resources.cpu_cores) / 32.0f), // CPU cores
        std::min(1.0f, d.resources.cpu_ghz / 5.0f),                        // CPU freq
        d.resources.gpu ? 1.0f : 0.0f,                                      // GPU
        
        // Сенсоры (битовое поле)
        d.sensors.camera ? 1.0f : 0.0f,
        d.sensors.microphone ? 1.0f : 0.0f,
        d.sensors.imu ? 1.0f : 0.0f,
        d.sensors.gps ? 1.0f : 0.0f,
        d.sensors.touch ? 1.0f : 0.0f,
        
        // Актуаторы
        d.actuators.display ? 1.0f : 0.0f,
        d.actuators.speaker ? 1.0f : 0.0f,
        d.actuators.motor ? 1.0f : 0.0f,
        d.actuators.network ? 1.0f : 0.0f,
        
        // Интерфейсы
        d.interfaces.bluetooth ? 1.0f : 0.0f,
        d.interfaces.wifi ? 1.0f : 0.0f,
        
        // Тип устройства (кодирование)
        d.device_type == "robot" ? 0.8f :
        d.device_type == "drone" ? 0.7f :
        d.device_type == "phone" ? 0.6f :
        d.device_type == "computer" ? 0.5f :
        d.device_type == "server" ? 0.4f :
        d.device_type == "embedded" ? 0.2f :
        d.device_type == "wearable" ? 0.3f : 0.1f
    };
}