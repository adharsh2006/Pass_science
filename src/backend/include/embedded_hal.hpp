#pragma once
#include <string>
#include <vector>
#include <memory>

namespace workbench {

enum class DeviceCategory {
    MCU_BOARD,
    FPGA_BOARD,
    JTAG_DEBUG_ADAPTER
};

struct DeviceDescriptor {
    std::string device_id;
    std::string model_name;
    DeviceCategory category;
    std::string host_interface;
    std::string connector_type;
    double io_voltage_V{3.3};
    double core_voltage_V{3.3};
    std::string level_shifting_scheme;
    std::vector<std::string> capabilities;
    bool is_simulated{true};
};

struct TelemetryPacket {
    bool success{false};
    bool recovered_from_fault{false};
    int retry_attempts{0};
    std::string device_id;
    std::string timestamp_iso;
    std::string status_label;
    std::string payload_json;
    std::string diagnostic_message;
};

class IHardwareAdapter {
public:
    virtual ~IHardwareAdapter() = default;
    virtual DeviceDescriptor discover() const = 0;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual void injectTransientTimeout(bool enable) = 0;
    virtual TelemetryPacket readTelemetry() = 0;
};

class Stm32McuMockAdapter : public IHardwareAdapter {
public:
    DeviceDescriptor discover() const override;
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override { return connected_; }
    void injectTransientTimeout(bool enable) override { fault_injected_ = enable; }
    TelemetryPacket readTelemetry() override;
private:
    bool connected_{false};
    bool fault_injected_{false};
    int seq_{0};
};

class Artix7FpgaMockAdapter : public IHardwareAdapter {
public:
    DeviceDescriptor discover() const override;
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override { return connected_; }
    void injectTransientTimeout(bool enable) override { fault_injected_ = enable; }
    TelemetryPacket readTelemetry() override;
private:
    bool connected_{false};
    bool fault_injected_{false};
    int seq_{0};
};

class Ft2232JtagMockAdapter : public IHardwareAdapter {
public:
    DeviceDescriptor discover() const override;
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override { return connected_; }
    void injectTransientTimeout(bool enable) override { fault_injected_ = enable; }
    TelemetryPacket readTelemetry() override;
private:
    bool connected_{false};
    bool fault_injected_{false};
    int seq_{0};
};

class EmbeddedWorkbenchManager {
public:
    EmbeddedWorkbenchManager();
    std::vector<DeviceDescriptor> discoverAll() const;
    bool connectAll();
    bool disconnectAll();
    std::vector<TelemetryPacket> runTelemetryAndRecoveryDemo();
private:
    std::vector<std::unique_ptr<IHardwareAdapter>> adapters_;
};

} // namespace workbench
