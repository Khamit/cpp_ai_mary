
#include <iostream>

/*
При старте:

DeviceDescriptor device = DeviceProbe::detect();
mary_core.initialize(device);

или так -
DeviceDescriptor DeviceProbe::detect()
{
    DeviceDescriptor d;

    d.resources.ram_mb = detectRAM();
    d.resources.cpu_cores = detectCPU();
    d.sensors.camera = detectCamera();
    d.sensors.microphone = detectMic();

    return d;
}

4. Универсальный IO слой

Нужно сделать единый интерфейс:

class IOChannel
{
public:
    virtual std::string read() = 0;
    virtual void write(const std::string& data) = 0;
};

Реализации:

ConsoleIO
NetworkIO
BluetoothIO
SerialIO
VoiceIO

Mary работает только с интерфейсом:

IOChannel

а не с устройствами напрямую.
*/

struct Capability
{
    std::string name;
    std::vector<std::string> actions;
};

struct DeviceDescriptor
{
    std::string device_type;

    struct Resources {
        size_t ram_mb;
        float cpu_ghz;
        int cpu_cores;
        bool gpu;
    } resources;

    struct Sensors {
        bool camera;
        bool microphone;
        bool imu;
        bool gps;
        bool touch;
    } sensors;

    struct Actuators {
        bool display;
        bool speaker;
        bool motor;
        bool network;
    } actuators;

    struct Interfaces {
        bool stdin;
        bool stdout;
        bool filesystem;
        bool bluetooth;
        bool wifi;
    } interfaces;
};