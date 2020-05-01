#pragma once

#include "muIterator.h"

// intrusive array
// similar to std::span in C++20
template<class T>
class Span
{
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    Span() : m_data(nullptr), m_size(0) {}
    template<size_t N> Span(const T (&d)[N]) : m_data(const_cast<T*>(d)), m_size(N) {}
    Span(const T *d, size_t s) : m_data(const_cast<T*>(d)), m_size(s) {}
    Span(const Span& v) : m_data(const_cast<T*>(v.m_data)), m_size(v.m_size) {}
    template<class Container> Span(const Container& v) : m_data(const_cast<T*>(v.data())), m_size(v.size()) {}
    Span& operator=(const Span& v) { m_data = const_cast<T*>(v.m_data); m_size = v.m_size; return *this; }

    bool empty() const { return m_size == 0; }
    size_t size() const { return m_size; }
    size_t size_bytes() const { return sizeof(T) * m_size; }

    T* data() { return m_data; }
    const T* data() const { return m_data; }
    const T* cdata() const { return m_data; }

    T& operator[](size_t i) { return m_data[i]; }
    const T& operator[](size_t i) const { return m_data[i]; }

    T& front() { return m_data[0]; }
    const T& front() const { return m_data[0]; }
    T& back() { return m_data[m_size - 1]; }
    const T& back() const { return m_data[m_size - 1]; }

    iterator begin() { return m_data; }
    const_iterator begin() const { return m_data; }
    iterator end() { return m_data + m_size; }
    const_iterator end() const { return m_data + m_size; }

    void zeroclear()
    {
        memset(m_data, 0, sizeof(T)*m_size);
    }
    void copy_to(pointer dst) const
    {
        memcpy(dst, m_data, sizeof(value_type) * m_size);
    }
    void copy_to(pointer dst, size_t length, size_t offset = 0) const
    {
        memcpy(dst, m_data + offset, sizeof(value_type) * length);
    }

private:
    T *m_data = nullptr;
    size_t m_size = 0;
};


// indexed version of Span
template<class I, class T>
class IndexedSpan
{
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = indexed_iterator<T*, const I*>;
    using const_iterator = indexed_iterator<const T*, const I*>;

    IndexedSpan() {}
    IndexedSpan(const I *i, const T *d, size_t s) : m_indices(const_cast<I*>(i)), m_data(const_cast<T*>(d)), m_size(s) {}
    IndexedSpan(const IndexedSpan& v) : m_indices(const_cast<I*>(v.m_indices)), m_data(const_cast<T*>(v.m_data)), m_size(v.m_size) {}
    template<class IContainer, class VContainer>
    IndexedSpan(const IContainer& i, const VContainer& v) : m_indices(const_cast<I*>(i.data())), m_data(const_cast<T*>(v.data())), m_size(i.size()) {}
    IndexedSpan& operator=(const IndexedSpan& v)
    {
        m_indices = const_cast<I*>(v.m_indices);
        m_data = const_cast<T*>(v.m_data);
        m_size = v.m_size;
        return *this;
    }

    void reset(const I *i, const T *d, size_t s)
    {
        m_indices = const_cast<I*>(i);
        m_data = const_cast<I*>(d);
        m_size = s;
    }

    bool empty() const { return m_size == 0; }
    size_t size() const { return m_size; }

    I* index() { return m_indices; }
    const I* index() const { return m_indices; }

    T* data() { return m_data; }
    const T* data() const { return m_data; }

    T& operator[](size_t i) { return m_data[m_indices[i]]; }
    const T& operator[](size_t i) const { return m_data[m_indices[i]]; }

    iterator begin() { return { m_data, m_indices }; }
    const_iterator begin() const { return { m_data, m_indices }; }
    iterator end() { return { m_data, m_indices + m_size }; }
    const_iterator end() const { return { m_data, m_indices + m_size }; }

private:
    I *m_indices = nullptr;
    T *m_data = nullptr;
    size_t m_size = 0;
};

template<class I, class T> using ISpan = IndexedSpan<I, T>;

template<class T>
inline Span<T> MakeSpan(const T* v, size_t s)
{
    return Span<T>(v, s);
}

template<class Data>
inline Span<typename Data::value_type> MakeSpan(const Data& v)
{
    return Span<typename Data::value_type>(v.data(), s.size());
}

template<class I, class T>
inline ISpan<I, T> MakeISpan(const I* i, const T* v, size_t s)
{
    return ISpan<I, T>(i, v, s);
}

template<class Indices, class Data>
inline ISpan<typename Indices::value_type, typename Data::value_type> MakeISpan(const Indices& i, const Data& v)
{
    return ISpan<typename Indices::value_type, typename Data::value_type>(i.data(), v.data(), i.size());
}
