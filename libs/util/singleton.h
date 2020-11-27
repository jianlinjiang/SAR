// MIT License

// Copyright (c) 2020 jianlinjiang
#include <memory>
#include <mutex>
#include <thread>
namespace sar
{
    template <typename T>
    class Singleton
    {
    public:
        template <typename... Args>
        static std::unique_ptr<T> &getInstance(Args &&... args)
        {
            std::call_once(flag_, [&]() {
                instance_.reset(new T(std::forward<Args>(args)...));
            });
            return instance_;
        }
        ~Singleton() = default;

    private:
        Singleton() = default;
        Singleton(const Singleton &) = delete;
        Singleton &operator=(const Singleton &) = delete;

    private:
        static std::unique_ptr<T> instance_;
        static std::once_flag flag_;
        // an internal state, default is 0. automically update.
    };
    template <typename Test>
    std::unique_ptr<Test> Singleton<Test>::instance_;
    template <typename T>
    std::once_flag Singleton<T>::flag_;
} // namespace sar
