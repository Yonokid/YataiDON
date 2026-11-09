#include <chrono>
#include <nfc/nfc.h>
#include <spdlog/spdlog.h>

#include "../card_reader.h"

namespace card_reader {

    namespace {
        class LibNfcReader final : public Reader {
        public:
            ~LibNfcReader() override {
                stop_polling();
                if (device_) {
                    nfc_close(device_);
                }

                if (context_) {
                    nfc_exit(context_);
                }
            }

            bool initialize(const std::string& connection) {
                nfc_init(&context_);
                if (!context_) {
                    spdlog::error("Unable to initialize libnfc");
                    return false;
                }

                device_ = nfc_open(context_, connection.empty() ? nullptr : connection.c_str());
                if (!device_) {
                    spdlog::error("Unable to open libnfc reader; check device configuration and permissions");
                    return false;
                }

                if (nfc_initiator_init(device_) < 0 ||
                    nfc_device_set_property_bool(device_, NP_INFINITE_SELECT, false) < 0 ||
                    nfc_device_set_property_int(device_, NP_TIMEOUT_COMMAND, 500) < 0) {
                    spdlog::error("Unable to configure libnfc reader: {}", nfc_strerror(device_));
                    return false;
                }

                spdlog::info("libnfc reader initialized: {}", nfc_device_get_name(device_));
                return true;
            }

            bool start_polling() override {
                if (worker_.joinable()) {
                    return !stop_;
                }

                if (nfc_device_set_property_bool(device_, NP_ACTIVATE_FIELD, true) < 0) {
                    spdlog::error("Unable to activate NFC reader: {}", nfc_strerror(device_));
                    return false;
                }

                reset_detection();
                stop_ = false;
                worker_ = std::thread([this] {
                    const nfc_modulation modulation{NMT_ISO14443A, NBR_106};
                    bool reported_error = false;
                    while (!stop_) {
                        const auto generation = detection_generation();
                        nfc_target target{};
                        const int result =
                            nfc_initiator_select_passive_target(device_, modulation, nullptr, 0, &target);

                        CardInfo info;
                        if (result > 0) {
                            const auto& card = target.nti.nai;
                            if (card.szUidLen == 4 || card.szUidLen == 7 || card.szUidLen == 10) {
                                static constexpr char hex[] = "0123456789ABCDEF";
                                info.type = CardType::NFC_UID;
                                for (size_t i = 0; i < card.szUidLen; ++i) {
                                    info.card_id += hex[card.abtUid[i] >> 4];
                                    info.card_id += hex[card.abtUid[i] & 0x0F];
                                }

                                info.valid = true;
                            }

                            nfc_initiator_deselect_target(device_);
                        }

                        if (result < 0 && result != NFC_ETIMEOUT && !stop_) {
                            if (!reported_error) {
                                spdlog::warn("libnfc polling failed: {}", nfc_strerror(device_));
                            }

                            reported_error = true;
                        } else {
                            reported_error = false;
                        }

                        // A target-selection timeout can mean that the card was removed.
                        if (!stop_ && (result == 0 || result == NFC_ETIMEOUT || info.valid)) {
                            report_card(info, generation);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                });

                return true;
            }

            void stop_polling() override {
                stop_ = true;
                if (worker_.joinable()) {
                    worker_.join();
                    nfc_idle(device_);
                }

                reset_detection();
            }

            bool is_polling() const override { return !stop_; }

        private:
            nfc_context* context_ = nullptr;
            nfc_device* device_ = nullptr;
            std::atomic<bool> stop_{true};
            std::thread worker_;
        };
    } // namespace

    std::unique_ptr<Reader> open_libnfc_reader(const std::string& device) {
        auto reader = std::make_unique<LibNfcReader>();
        if (!reader->initialize(device)) {
            return nullptr;
        }

        return reader;
    }

} // namespace card_reader
