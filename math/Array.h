#pragma once

template <typename T, int N>
struct Array {
    // The raw data sequence
    T m_Data[N];

    // Read/Write indexing
    T& operator[](int index) { return m_Data[index]; }
    const T& operator[](int index) const { return m_Data[index]; }

    // Pointer hooks for Vulkan
    T* data() { return m_Data; }
    const T* data() const { return m_Data; }

    // Hard count property
    constexpr int size() const { return N; }
};