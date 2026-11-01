#include "embedded_hal.hpp"
#include <ctime>
#include <sstream>
#include <iomanip>

namespace workbench {

namespace {
std::string isoTimestamp(int seq) {
    // Produce a deterministic ISO-8601 timestamp offset by seq seconds from a fixed base
    std::tm base_tm{};
    base_tm.tm_year = 126;  // 2026 - 1900
    base_tm.tm_mon  = 10;   // November (0-indexed)
    base_tm.tm_mday = 1;
    base_tm.tm_hour = 8;
    base_tm.tm_min  = 0;
    base_tm.tm_sec  = seq;
    std::mktime(&base_tm);
    std::ostringstream oss;
    oss << std::put_time(&base_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
} // namespace

DeviceDescriptor Stm32McuMockAdapter::discover() const {
    DeviceDescriptor d;
    d.device_id = "MCU-STM32F407VG-01";
    d.model_name = "STM32F407VG Cortex-M4 Microcontroller Board (SIMULATED)";
    d.category = DeviceCategory::MCU_BOARD;
    d.host_interface = "USB 2.0 FS CDC-ACM Virtual COM (115200 8N1)";
    d.connector_type = "USB Micro-B + 2x25 Pin 2.54mm GPIO Header + 10-pin Cortex Debug";
    d.io_voltage_V = 3.3;
    d.core_voltage_V = 1.2; // Internal LDO from 3.3V VDD
    d.level_shifting_scheme = "Direct 3.3V CMOS to Host USB-UART Bridge; SN74AVC4T245 for 1.8V FPGA SPI link";
    d.capabilities = {
        "ADC_12BIT_TEMP_SENSE",
        "PWM_HEATER_CONTROL",
        "UART_TELEMETRY_STREAM",
        "SWD_JTAG_SLAVE_TAP (IDCODE 0x4BA00477)"
    };
    d.is_simulated = true;
    return d;
}

bool Stm32McuMockAdapter::connect() {
    connected_ = true;
    return true;
}

bool Stm32McuMockAdapter::disconnect() {
    connected_ = false;
    return true;
}

TelemetryPacket Stm32McuMockAdapter::readTelemetry() {
    TelemetryPacket pkt;
    pkt.device_id = "MCU-STM32F407VG-01";
    pkt.timestamp_iso = isoTimestamp(++seq_);

    if (!connected_) {
        pkt.success = false;
        pkt.status_label = "DISCONNECTED";
        pkt.diagnostic_message = "[SIMULATED] Cannot read MCU telemetry: adapter is disconnected.";
        return pkt;
    }

    if (fault_injected_) {
        // Simulate recoverable UART framing/bus timeout and automatic retry recovery
        pkt.retry_attempts = 2;
        pkt.recovered_from_fault = true;
        fault_injected_ = false; // Cleared upon recovery
        pkt.success = true;
        pkt.status_label = "RECOVERED_AFTER_TIMEOUT";
        pkt.payload_json = R"({"mode":"SIMULATED","vdd_V":3.302,"core_temp_C":41.8,"adc_ch0_mV":1650,"pwm_duty_pct":35.0})";
        pkt.diagnostic_message = "[SIMULATED] Transient UART RX timeout (250 ms) detected on attempt #1; bus flushed and recovered cleanly on retry #2.";
        return pkt;
    }

    pkt.success = true;
    pkt.status_label = "NOMINAL_SIMULATED";
    pkt.payload_json = R"({"mode":"SIMULATED","vdd_V":3.304,"core_temp_C":41.2,"adc_ch0_mV":1648,"pwm_duty_pct":35.0})";
    pkt.diagnostic_message = "[SIMULATED] Nominal MCU telemetry packet received over virtual USB-CDC.";
    return pkt;
}

DeviceDescriptor Artix7FpgaMockAdapter::discover() const {
    DeviceDescriptor d;
    d.device_id = "FPGA-XC7A35T-01";
    d.model_name = "Xilinx Artix-7 XC7A35T FPGA Carrier Board (SIMULATED)";
    d.category = DeviceCategory::FPGA_BOARD;
    d.host_interface = "USB-FIFO / SPI Slave Bridge (10 MHz Mode 0)";
    d.connector_type = "USB-C Power/Data + PMOD 12-pin + 14-pin Xilinx JTAG Header";
    d.io_voltage_V = 1.8;   // Bank 14/15 high-speed I/O at 1.8V; Bank 34 at 3.3V
    d.core_voltage_V = 1.0; // VCCINT = 1.0V, VCCAUX = 1.8V
    d.level_shifting_scheme = "Dual-supply bidirectional SN74AVC4T245 translating 1.8V Bank 14 I/O <-> 3.3V MCU SPI";
    d.capabilities = {
        "XADC_DIE_TEMP_AND_VCCINT_MONITOR",
        "PARALLEL_PWM_LUT_FABRIC",
        "SPI_SLAVE_REGISTER_BANK",
        "IEEE1149_1_JTAG_TAP (IDCODE 0x0362D093)"
    };
    d.is_simulated = true;
    return d;
}

bool Artix7FpgaMockAdapter::connect() {
    connected_ = true;
    return true;
}

bool Artix7FpgaMockAdapter::disconnect() {
    connected_ = false;
    return true;
}

TelemetryPacket Artix7FpgaMockAdapter::readTelemetry() {
    TelemetryPacket pkt;
    pkt.device_id = "FPGA-XC7A35T-01";
    pkt.timestamp_iso = isoTimestamp(++seq_);

    if (!connected_) {
        pkt.success = false;
        pkt.status_label = "DISCONNECTED";
        pkt.diagnostic_message = "[SIMULATED] Cannot read FPGA telemetry: adapter is disconnected.";
        return pkt;
    }

    if (fault_injected_) {
        pkt.retry_attempts = 2;
        pkt.recovered_from_fault = true;
        fault_injected_ = false;
        pkt.success = true;
        pkt.status_label = "RECOVERED_AFTER_TIMEOUT";
        pkt.payload_json = R"({"mode":"SIMULATED","vccint_V":1.001,"vcco_b14_V":1.802,"die_temp_C":48.6,"lut_utilization_pct":24.5})";
        pkt.diagnostic_message = "[SIMULATED] Transient SPI CS_N framing timeout detected; re-synchronized state machine on retry #2.";
        return pkt;
    }

    pkt.success = true;
    pkt.status_label = "NOMINAL_SIMULATED";
    pkt.payload_json = R"({"mode":"SIMULATED","vccint_V":1.000,"vcco_b14_V":1.801,"die_temp_C":48.3,"lut_utilization_pct":24.5})";
    pkt.diagnostic_message = "[SIMULATED] Nominal FPGA XADC telemetry read complete.";
    return pkt;
}

DeviceDescriptor Ft2232JtagMockAdapter::discover() const {
    DeviceDescriptor d;
    d.device_id = "JTAG-FT2232H-PROBE-01";
    d.model_name = "FT2232H IEEE 1149.1 Multi-Voltage JTAG Debug Probe (SIMULATED)";
    d.category = DeviceCategory::JTAG_DEBUG_ADAPTER;
    d.host_interface = "USB 2.0 Hi-Speed MPSSE JTAG Engine (480 Mbps USB -> 15 MHz TCK)";
    d.connector_type = "20-pin ARM / 14-pin Xilinx JTAG Breakouts with VTref Sense Pin";
    d.io_voltage_V = 3.3;   // Tracks target VTref (1.65V - 5.0V via SN74LVC2T45 buffers)
    d.core_voltage_V = 3.3;
    d.level_shifting_scheme = "Target-powered VTref reference input on Pin 1 biasing SN74LVC2T45 output stages (1.8V or 3.3V)";
    d.capabilities = {
        "IEEE1149_1_TAP_CONTROLLER_MASTER",
        "BOUNDARY_SCAN_IDCODE_ENUMERATION",
        "TARGET_VTREF_SENSE_AND_FAULT_LOCKOUT",
        "OPENOCD_GDB_DEBUG_TRANSPORT"
    };
    d.is_simulated = true;
    return d;
}

bool Ft2232JtagMockAdapter::connect() {
    connected_ = true;
    return true;
}

bool Ft2232JtagMockAdapter::disconnect() {
    connected_ = false;
    return true;
}

TelemetryPacket Ft2232JtagMockAdapter::readTelemetry() {
    TelemetryPacket pkt;
    pkt.device_id = "JTAG-FT2232H-PROBE-01";
    pkt.timestamp_iso = isoTimestamp(++seq_);

    if (!connected_) {
        pkt.success = false;
        pkt.status_label = "DISCONNECTED";
        pkt.diagnostic_message = "[SIMULATED] Cannot poll JTAG TAP chain: probe is disconnected.";
        return pkt;
    }

    if (fault_injected_) {
        pkt.retry_attempts = 2;
        pkt.recovered_from_fault = true;
        fault_injected_ = false;
        pkt.success = true;
        pkt.status_label = "RECOVERED_TAP_RESET";
        pkt.payload_json = R"({"mode":"SIMULATED","vtref_sense_V":3.298,"tck_freq_MHz":10.0,"tap_state":"RUN_TEST_IDLE","idcodes":["0x4BA00477","0x0362D093"]})";
        pkt.diagnostic_message = "[SIMULATED] JTAG TDO CRC mismatch/timeout on scan; issued 5x TMS=1 Test-Logic-Reset recovery sequence and restored chain.";
        return pkt;
    }

    pkt.success = true;
    pkt.status_label = "NOMINAL_SIMULATED";
    pkt.payload_json = R"({"mode":"SIMULATED","vtref_sense_V":3.301,"tck_freq_MHz":10.0,"tap_state":"RUN_TEST_IDLE","idcodes":["0x4BA00477","0x0362D093"]})";
    pkt.diagnostic_message = "[SIMULATED] JTAG boundary-scan chain healthy; 2 TAP devices enumerated.";
    return pkt;
}

EmbeddedWorkbenchManager::EmbeddedWorkbenchManager() {
    adapters_.push_back(std::make_unique<Stm32McuMockAdapter>());
    adapters_.push_back(std::make_unique<Artix7FpgaMockAdapter>());
    adapters_.push_back(std::make_unique<Ft2232JtagMockAdapter>());
}

std::vector<DeviceDescriptor> EmbeddedWorkbenchManager::discoverAll() const {
    std::vector<DeviceDescriptor> out;
    for (const auto& a : adapters_) {
        out.push_back(a->discover());
    }
    return out;
}

bool EmbeddedWorkbenchManager::connectAll() {
    bool ok = true;
    for (auto& a : adapters_) {
        ok = a->connect() && ok;
    }
    return ok;
}

bool EmbeddedWorkbenchManager::disconnectAll() {
    bool ok = true;
    for (auto& a : adapters_) {
        ok = a->disconnect() && ok;
    }
    return ok;
}

std::vector<TelemetryPacket> EmbeddedWorkbenchManager::runTelemetryAndRecoveryDemo() {
    std::vector<TelemetryPacket> log;
    connectAll();
    // 1. Nominal telemetry read from all 3 devices
    for (auto& a : adapters_) {
        log.push_back(a->readTelemetry());
    }
    // 2. Inject recoverable timeout failure on FPGA adapter and demonstrate recovery
    adapters_[1]->injectTransientTimeout(true);
    log.push_back(adapters_[1]->readTelemetry());
    // 3. Inject recoverable JTAG TAP scan timeout and demonstrate Test-Logic-Reset recovery
    adapters_[2]->injectTransientTimeout(true);
    log.push_back(adapters_[2]->readTelemetry());
    // 4. Subsequent nominal read confirming clean post-recovery state
    log.push_back(adapters_[1]->readTelemetry());
    disconnectAll();
    return log;
}

} // namespace workbench
