#include "Crypto.h"

Crypto::Crypto(State* state, LowLevel* lowLevel)
    : state_(state), lowLevel_(lowLevel)
{
#ifdef CONFIG_ILSS_CRYPTO_LOG_LEVEL
    logger_.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_CRYPTO_LOG_LEVEL));
#else
    logger_.setLogLevel(LogLevel::INFO);
#endif
    (void)state_;
}

bool Crypto::begin()
{
    if (!lowLevel_) return false;
    // Driver is started by LowLevel::begin(); verify readiness.
    initialized_ = lowLevel_->get_crypto().isReady();
    if (!initialized_) {
        logger_.LOGW(TAG, "ATECC608B not ready");
    }
    return initialized_;
}

bool Crypto::isReady() const
{
    return initialized_ && lowLevel_ && lowLevel_->get_crypto().isReady();
}

bool Crypto::wake()
{
    return isReady() && lowLevel_->get_crypto().wake();
}

bool Crypto::idle()
{
    return isReady() && lowLevel_->get_crypto().idle();
}

bool Crypto::sleep()
{
    return isReady() && lowLevel_->get_crypto().sleep();
}

bool Crypto::readRevision(uint8_t out[ATECC608BDriver::kRevisionLen])
{
    return isReady() && lowLevel_->get_crypto().readRevision(out);
}

bool Crypto::readSerial(uint8_t out[ATECC608BDriver::kSerialLen])
{
    return isReady() && lowLevel_->get_crypto().readSerial(out);
}

bool Crypto::randomBytes(uint8_t out[ATECC608BDriver::kRandomLen])
{
    return isReady() && lowLevel_->get_crypto().random(out);
}

const char* Crypto::revisionName(const uint8_t rev[ATECC608BDriver::kRevisionLen])
{
    return ATECC608BDriver::revisionName(rev);
}
