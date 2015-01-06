#include "any.hpp"

namespace ctl {
    template<typename T>
    void packet_write(packet_t::base& p, const void* vdata, std::uint32_t size) {
        const T* data = reinterpret_cast<const T*>(vdata);
        for (std::size_t i = 0; i < size; ++i) {
            p << data[i];
        }
    }

    packet_t::base& operator << (packet_t::base& p, const any& data) {
        p << data.type_ << data.size_;
        switch (data.type_) {
            case any_type::none : break;
            case any_type::boolean : packet_write<bool>(p, data.data_, data.size_); break;
            case any_type::int8 : packet_write<std::int8_t>(p, data.data_, data.size_); break;
            case any_type::int16 : packet_write<std::int16_t>(p, data.data_, data.size_); break;
            case any_type::int32 : packet_write<std::int32_t>(p, data.data_, data.size_); break;
            case any_type::uint8 : packet_write<std::uint8_t>(p, data.data_, data.size_); break;
            case any_type::uint16 : packet_write<std::uint16_t>(p, data.data_, data.size_); break;
            case any_type::uint32 : packet_write<std::uint32_t>(p, data.data_, data.size_); break;
            case any_type::float32 : packet_write<float>(p, data.data_, data.size_); break;
            case any_type::float64 : packet_write<double>(p, data.data_, data.size_); break;
            case any_type::string : packet_write<std::string>(p, data.data_, data.size_); break;
        }

        return p;
    }

    template<typename T>
    void packet_read(packet_t::base& p, void*& vdata, std::uint32_t size) {
        T* data = new T[size];
        vdata = data;
        for (std::size_t i = 0; i < size; ++i) {
            p >> data[i];
        }
    }

    packet_t::base& operator >> (packet_t::base& p, any& data) {
        p >> data.type_ >> data.size_;
        switch (data.type_) {
            case any_type::none : break;
            case any_type::boolean : packet_read<bool>(p, data.data_, data.size_); break;
            case any_type::int8 : packet_read<std::int8_t>(p, data.data_, data.size_); break;
            case any_type::int16 : packet_read<std::int16_t>(p, data.data_, data.size_); break;
            case any_type::int32 : packet_read<std::int32_t>(p, data.data_, data.size_); break;
            case any_type::uint8 : packet_read<std::uint8_t>(p, data.data_, data.size_); break;
            case any_type::uint16 : packet_read<std::uint16_t>(p, data.data_, data.size_); break;
            case any_type::uint32 : packet_read<std::uint32_t>(p, data.data_, data.size_); break;
            case any_type::float32 : packet_read<float>(p, data.data_, data.size_); break;
            case any_type::float64 : packet_read<double>(p, data.data_, data.size_); break;
            case any_type::string : packet_read<std::string>(p, data.data_, data.size_); break;
        }

        return p;
    }

    any::any(any&& a) : type_(a.type_), size_(a.size_), data_(a.data_) {
        a.type_ = any_type::none;
        a.size_ = 0u;
        a.data_ = nullptr;
    }

    template<typename T>
    void destroy(void*& data, std::uint32_t size) {
        T* cdata = reinterpret_cast<T*>(data);
        if (size > 1) {
            delete[] cdata;
        } else {
            delete cdata;
        }

        data = nullptr;
    }

    any::~any() {
        switch (type_) {
            case any_type::none : break;
            case any_type::boolean : destroy<bool>(data_, size_); break;
            case any_type::int8 : destroy<std::int8_t>(data_, size_); break;
            case any_type::int16 : destroy<std::int16_t>(data_, size_); break;
            case any_type::int32 : destroy<std::int32_t>(data_, size_); break;
            case any_type::uint8 : destroy<std::uint8_t>(data_, size_); break;
            case any_type::uint16 : destroy<std::uint16_t>(data_, size_); break;
            case any_type::uint32 : destroy<std::uint32_t>(data_, size_); break;
            case any_type::float32 : destroy<float>(data_, size_); break;
            case any_type::float64 : destroy<double>(data_, size_); break;
            case any_type::string : destroy<std::string>(data_, size_); break;
        }
    }

    any& any::operator = (any&& a) {
        type_ = a.type_;
        size_ = a.size_;
        data_ = a.data_;
        a.type_ = any_type::none;
        a.size_ = 0u;
        a.data_ = nullptr;
        return *this;
    }

    any_type any::type() const {
        return type_;
    }

    std::uint32_t any::size() const {
        return size_;
    }
}
