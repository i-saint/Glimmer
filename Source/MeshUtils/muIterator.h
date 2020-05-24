#pragma once

#define Boilerplate(I, V)                                       \
    using this_t            = I;                                \
    using difference_type   = std::ptrdiff_t;                   \
    using value_type        = V;                                \
    using reference         = V&;                               \
    using pointer           = V*;                               \
    using iterator_category = std::random_access_iterator_tag;  \


template<class T, int Stride = 0>
struct strided_iterator
{
    Boilerplate(strided_iterator, T);
    static const int stride = Stride;

    char* data = nullptr;

    strided_iterator(const T* d = nullptr) : data((char*)d) {}
    reference operator[](int v)  { return *(T*)(data + stride*v); }
    reference operator*()        { return *(T*)data; }
    pointer   operator->()       { return &(T*)data; }
    this_t  operator+(size_t v)  { return data + stride*v; }
    this_t  operator-(size_t v)  { return data + stride*v; }
    this_t& operator+=(size_t v) { data += stride*v; return *this; }
    this_t& operator-=(size_t v) { data -= stride*v; return *this; }
    this_t& operator++()         { data += stride; return *this; }
    this_t& operator++(int)      { data += stride; return *this; }
    this_t& operator--()         { data -= stride; return *this; }
    this_t& operator--(int)      { data -= stride; return *this; }
    bool operator==(const this_t& v) const { return data == data; }
    bool operator!=(const this_t& v) const { return data != data; }
    operator T*() { return (T*)data; }

};

template<class T>
struct strided_iterator<T, 0>
{
    Boilerplate(strided_iterator, T);

    char* data = nullptr;
    size_t stride = 0;

    strided_iterator() {}
    strided_iterator(const T* d, size_t s) : data((char*)d), stride(s) {}
    reference operator[](int v)  { return *(T*)(data + stride*v); }
    reference operator*()        { return *(T*)data; }
    pointer   operator->()       { return &(T*)data; }
    this_t  operator+(size_t v)  { return data + stride*v; }
    this_t  operator-(size_t v)  { return data + stride*v; }
    this_t& operator+=(size_t v) { data += stride*v; return *this; }
    this_t& operator-=(size_t v) { data -= stride*v; return *this; }
    this_t& operator++()         { data += stride; return *this; }
    this_t& operator++(int)      { data += stride; return *this; }
    this_t& operator--()         { data -= stride; return *this; }
    this_t& operator--(int)      { data -= stride; return *this; }
    bool operator==(const this_t& v) const { return data == data; }
    bool operator!=(const this_t& v) const { return data != data; }
    operator T* () { return (T*)data; }
};


template<class T, class I>
struct indexed_iterator
{
    Boilerplate(indexed_iterator, T)

    T* data = nullptr;
    I* index = nullptr;
    
    indexed_iterator() {}
    indexed_iterator(const T* d, const I* i) : data((T*)d), index((T*)i) {}
    reference operator[](int v) { return data[index[v]]; }
    reference operator*()       { return data[*index]; }
    pointer   operator->()      { return &data[*index]; }
    this_t  operator+(size_t v) { return { data, index + v }; }
    this_t  operator-(size_t v) { return { data, index - v }; }
    this_t& operator+=(size_t v){ index += v; return *this; }
    this_t& operator-=(size_t v){ index -= v; return *this; }
    this_t& operator++()        { ++index; return *this; }
    this_t& operator++(int)     { ++index; return *this; }
    this_t& operator--()        { --index; return *this; }
    this_t& operator--(int)     { --index; return *this; }
    bool operator==(const this_t& v) const { return v.data == data && v.index == index; }
    bool operator!=(const this_t& v) const { return v.data != data || v.index != index; }
    operator T* () { return data; }
};

template<class T, class I>
struct indexed_iterator_s
{
    Boilerplate(indexed_iterator_s, T)

    T* data = nullptr;
    I* index = nullptr;

    indexed_iterator_s() {}
    indexed_iterator_s(const T* d, const I* i) : data((T*)d), index((T*)i) {}
    reference operator[](int v) { return index ? data[index[v]] : data[v]; }
    reference operator*()       { return index ? data[*index] : *data; }
    pointer   operator->()      { return index ? &data[*index] : data; }
    this_t  operator+(size_t v) { return index ? this_t{ data, index + v } : this_t{ data + v, nullptr }; }
    this_t  operator-(size_t v) { return index ? this_t{ data, index - v } : this_t{ data - v, nullptr }; }
    this_t& operator+=(size_t v){ if (index) index += v; else data += v; return *this; }
    this_t& operator-=(size_t v){ if (index) index -= v; else data -= v; return *this; }
    this_t& operator++()        { if (index) ++index; else ++data; return *this; }
    this_t& operator++(int)     { if (index) ++index; else ++data; return *this; }
    this_t& operator--()        { if (index) --index; else --data; return *this; }
    this_t& operator--(int)     { if (index) --index; else --data; return *this; }
    bool operator==(const this_t& v) const { return v.data == data && v.index == index; }
    bool operator!=(const this_t& v) const { return v.data != data || v.index != index; }
    operator T* () { return data; }
};

#undef Boilerplate
