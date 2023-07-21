#ifndef MachineExceptions_hpp
#define MachineExceptions_hpp

#include <stdexcept>

class CupNotFoundException : public std::exception
{
public:
    CupNotFoundException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class BeverageDisabledException : public std::exception
{
public:
    BeverageDisabledException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class BottleDisabledException : public std::exception
{
public:
    BottleDisabledException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class MotorTimeoutException : public std::exception
{
public:
    MotorTimeoutException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class CupRemovedException : public std::exception
{
public:
    CupRemovedException(const std::string &message, double oz)
        : message_(message), dispensed_oz_(oz) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

    double get_dispensed_oz() const noexcept
    {
        return dispensed_oz_;
    }
    void set_priceDispensed(double price)
    {
        priceDispensed = price;
    }
    double get_priceDispensed() const noexcept
    {
        return priceDispensed;
    }

private:
    std::string message_;
    double dispensed_oz_;
    double priceDispensed = 0.0;
};

class BeverageCancelledException : public std::exception
{
public:
    BeverageCancelledException(const std::string &message, double oz)
        : message_(message), dispensed_oz_(oz) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

    double get_dispensed_oz() const noexcept
    {
        return dispensed_oz_;
    }
    void set_priceDispensed(double price)
    {
        priceDispensed = price;
    }
    double get_priceDispensed() const noexcept
    {
        return priceDispensed;
    }

private:
    std::string message_;
    double dispensed_oz_;
    double priceDispensed = 0.0;
};

class WifiFailedInit : public std::exception
{
public:
    WifiFailedInit(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class MqttFailedInit : public std::exception
{
public:
    MqttFailedInit(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class MqttFailedPublish : public std::exception
{
public:
    MqttFailedPublish(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

#endif /* MachineExceptions_hpp */