#pragma once

#include <Windows.h>
#include <unknwn.h>
#include <atomic>
#include <type_traits>

namespace UndownUnlock {
namespace DXHook {

/**
 * @brief Smart COM pointer template for automatic reference counting
 * 
 * This class provides RAII-style management of COM interfaces,
 * automatically calling AddRef when copied and Release when destroyed.
 */
template <typename T>
class ComPtr {
public:
    // Ensure T is derived from IUnknown
    static_assert(std::is_base_of<IUnknown, T>::value || std::is_same<IUnknown, T>::value, 
                  "ComPtr can only be used with COM interfaces derived from IUnknown");

    // Default constructor
    ComPtr() noexcept : m_ptr(nullptr) {}

    // Constructor from raw pointer
    ComPtr(T* ptr) noexcept : m_ptr(ptr) { 
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    // Copy constructor
    ComPtr(const ComPtr& other) noexcept : m_ptr(other.m_ptr) {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    // Move constructor
    ComPtr(ComPtr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    // Destructor
    ~ComPtr() {
        Release();
    }

    // Assignment operator
    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            // Release current reference
            Release();
            
            // Acquire new reference
            m_ptr = other.m_ptr;
            if (m_ptr) {
                m_ptr->AddRef();
            }
        }
        return *this;
    }

    // Move assignment operator
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            // Release current reference
            Release();
            
            // Take ownership of other's pointer
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    // Assignment from raw pointer
    ComPtr& operator=(T* ptr) noexcept {
        if (m_ptr != ptr) {
            // Release current reference
            Release();
            
            // Acquire new reference
            m_ptr = ptr;
            if (m_ptr) {
                m_ptr->AddRef();
            }
        }
        return *this;
    }

    // Release the interface
    void Release() {
        T* temp = m_ptr;
        if (temp) {
            m_ptr = nullptr;
            temp->Release();
        }
    }

    // AddRef on the interface
    void AddRef() {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    // Attach to raw pointer without AddRef
    void Attach(T* ptr) noexcept {
        Release();
        m_ptr = ptr;
    }

    // Detach pointer without Release
    T* Detach() noexcept {
        T* temp = m_ptr;
        m_ptr = nullptr;
        return temp;
    }

    // Get raw pointer (no AddRef)
    T* Get() const noexcept {
        return m_ptr;
    }

    // Get address of pointer for out params
    T** GetAddressOf() noexcept {
        Release();
        return &m_ptr;
    }

    // Get address of pointer for out params without releasing
    T** ReleaseAndGetAddressOf() noexcept {
        return &m_ptr;
    }

    // Cast to another interface type using QueryInterface
    template <typename U>
    HRESULT As(ComPtr<U>& other) const noexcept {
        return m_ptr->QueryInterface(__uuidof(U), reinterpret_cast<void**>(other.ReleaseAndGetAddressOf()));
    }

    // Boolean conversion operator
    explicit operator bool() const noexcept {
        return (m_ptr != nullptr);
    }

    // Arrow operator
    T* operator->() const noexcept {
        return m_ptr;
    }

    // Dereference operator
    T& operator*() const noexcept {
        return *m_ptr;
    }

    // Equality operators
    bool operator==(const ComPtr& other) const noexcept {
        return m_ptr == other.m_ptr;
    }

    bool operator!=(const ComPtr& other) const noexcept {
        return m_ptr != other.m_ptr;
    }

    bool operator==(const T* ptr) const noexcept {
        return m_ptr == ptr;
    }

    bool operator!=(const T* ptr) const noexcept {
        return m_ptr != ptr;
    }

private:
    T* m_ptr;
};

// Helper function to create a ComPtr from a COM interface
template <typename T>
ComPtr<T> MakeComPtr(T* ptr) {
    return ComPtr<T>(ptr);
}

} // namespace DXHook
} // namespace UndownUnlock 