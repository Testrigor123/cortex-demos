#include "catch.hpp"

#include <cstring>

#include "mock_memio.hpp"

#include "driver/adc.hpp"
#include "nrf52/pinctrl.hpp"

constexpr uint32_t saadc_base = 0x40007000;

// Offsets for various registers
constexpr auto saadc_enable = 0x500;
constexpr auto chx_pselp(unsigned int x) {
    return 0x510 + 16 * x;
}

constexpr auto result_ptr = 0x62c;
constexpr auto result_maxcnt = 0x630;

constexpr auto task_start = 0;
constexpr auto task_sample = 4;

constexpr auto event_started = 0x100;
constexpr auto event_done = 0x108;


TEST_CASE("Test ADC API") {
    auto& mem = mock::get_global_memory();
    mem.reset();

    auto get_reg_value = [&mem](uint32_t offset) -> uint32_t {
        return mem.get_value_at(saadc_base + offset);
    };

    SECTION("Test Enable") {
        auto* saadc = driver::ADC::request_by_id(driver::ADC::ID::ADC0);
        REQUIRE(saadc != nullptr);

        CHECK(get_reg_value(saadc_enable) == 1);

        // Based on fake_board configuration
        using ps = pinctrl::saadc;
        CHECK(get_reg_value(chx_pselp(0)) == ps::AIN3);
        CHECK(get_reg_value(chx_pselp(2)) == ps::AIN5);

        for (auto ch : {1, 3, 4, 5, 6, 7}) {
            CAPTURE(ch);
            CHECK(get_reg_value(chx_pselp(ch)) == 0);
        }

        CHECK(mem.get_ptr_at(saadc_base + result_ptr) != nullptr);
        CHECK(get_reg_value(result_maxcnt) > 0);
    }

    SECTION("Test Measurement") {
        auto* saadc = driver::ADC::request_by_id(driver::ADC::ID::ADC0);
        REQUIRE(saadc != nullptr);

        mock::IgnoreWrites event_sink;

        mem.set_value_at(saadc_base + event_started, 1);
        mem.set_value_at(saadc_base + event_done, 1);
        mem.set_addr_io_handler(saadc_base + event_started, &event_sink);
        mem.set_addr_io_handler(saadc_base + event_done, &event_sink);
        auto num_samples = 4;
        CHECK(saadc->start(num_samples) == num_samples);
        CHECK(get_reg_value(task_start) > 0);

        CHECK(mem.get_op_count(mock::Memory::Op::WRITE32, saadc_base + task_start) == 1);
        CHECK(mem.get_op_count(mock::Memory::Op::WRITE32, saadc_base + event_started) == 1);
        CHECK(mem.get_op_count(mock::Memory::Op::WRITE32, saadc_base + event_done) == num_samples);
        CHECK(mem.get_op_count(mock::Memory::Op::WRITE32, saadc_base + task_sample) == num_samples);
    }
}
